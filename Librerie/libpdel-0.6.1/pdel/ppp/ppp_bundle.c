
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_fsm.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_node.h"
#include "ppp/ppp_engine.h"
#include "ppp/ppp_link.h"
#include "ppp/ppp_lcp.h"
#include "ppp/ppp_ipcp.h"
#include "ppp/ppp_ccp.h"
#include "ppp/ppp_bundle.h"
#include "ppp/ppp_channel.h"
#include "ppp/ppp_msoft.h"

#include <netgraph/ng_mppc.h>

#ifndef MPPE_56
#define MPPE_56	0x00000080
#define MPPE_56_UNSUPPORTED
#endif

#define BUNDLE_MTYPE		"ppp_bundle"

/* Bundle structure */
struct ppp_bundle {
	struct ppp_engine	*engine;	/* engine that owns me */
	struct ppp_bundle_config conf;		/* bundle configuration */
	struct ppp_node		*node;		/* ng_ppp(4) node */
	struct ppp_log		*log;		/* log */
	struct ng_ppp_node_conf	node_conf;	/* ng_ppp(4) node config */
	struct paction		*action;	/* configuration action */
	struct ppp_fsm		*lcp;		/* lcp fsm (for bundle only) */
	struct ppp_fsm		*ipcp;		/* ipcp fsm */
	struct ppp_fsm		*ccp;		/* ccp fsm */
	struct pevent_ctx	*ev_ctx;	/* event context */
	pthread_mutex_t		*mutex;		/* mutex */
	struct pevent		*lcp_event;	/* lcp fsm event */
	struct pevent		*ipcp_event;	/* ipcp fsm event */
	struct pevent		*ccp_event;	/* ccp fsm event */
	struct pevent		*config_timeout;/* timer for config operation */
	void			*cookie;	/* client bundle cookie */
	void			*plumb_arg;	/* client plumbing arg */
	union ppp_auth_mppe	mppe;		/* mppe from first link auth */
	enum ppp_auth_index	mppe_auth;	/* type of auth on first link */
	u_char			mppe_server;	/* which side is mppe server */
	u_char			mppe_64[2][8];	/* 64 bit mppe keys */
	u_char			mppe_128[2][16];/* 128 bit mppe keys */
	u_char			multilink;	/* multilink active */
	u_char			shutdown;	/* bundle shutting down */
	u_char			ipcp_up;	/* ipcp is up */
	u_char			ccp_up;		/* ccp is up */
	struct ppp_eid		eid[2];		/* endpoint id */
	u_int			nlinks;		/* number of links */
	struct ppp_link		*links[NG_PPP_MAX_LINKS];	/* links */
	char			authname[2][PPP_MAX_AUTHNAME];	/* authname's */
};

/* Internal functions */
static int	ppp_bundle_configure(struct ppp_bundle *bundle);
static void	ppp_bundle_shutdown(struct ppp_bundle *bundle);
static void	ppp_bundle_fsm_dead(struct ppp_bundle *bundle);

static ppp_node_recv_t		ppp_bundle_node_recv;

/* Event handlers */
static pevent_handler_t		ppp_bundle_lcp_event;
static pevent_handler_t		ppp_bundle_ipcp_event;
static pevent_handler_t		ppp_bundle_ccp_event;

/* Internal variables */
static const u_char	ppp_bundle_zero[16];	/* all zeros */

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(bundle->log, sev, fmt , ## args)

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/*
 * Create a new bundle from a newly opened link.
 *
 * We 'steal' the link's ng_ppp(4) node for the bundle's use.
 * We assume the link's device is already connected as link #0.
 *
 * The "log" is destroyed when the bundle is destroyed.
 */
struct ppp_bundle *
ppp_bundle_create(struct ppp_engine *engine,
	struct ppp_link *link, struct ppp_node *node)
{
	struct ppp_lcp_req lcp_req;
	struct ppp_bundle *bundle;
	int i;

	/* Create bundle */
	if ((bundle = MALLOC(BUNDLE_MTYPE, sizeof(*bundle))) == NULL)
		return (NULL);
	memset(bundle, 0, sizeof(*bundle));
	bundle->engine = engine;
	bundle->ev_ctx = ppp_engine_get_ev_ctx(engine);
	bundle->mutex = ppp_engine_get_mutex(engine);
	bundle->node = node;
	bundle->links[0] = link;
	bundle->nlinks = 1;

	/* Make bundle known to engine */
	if (ppp_engine_add_bundle(bundle->engine, bundle) == -1)
		goto fail;

	/* Get link's negotiated LCP config and inherit from it */
	ppp_link_get_lcp_req(link, &lcp_req);
	for (i = 0; i < 2; i++) {
		strlcpy(bundle->authname[i], ppp_link_get_authname(link, i),
		    sizeof(bundle->authname[i]));
		bundle->eid[i] = lcp_req.eid[i];
	}
	bundle->multilink = lcp_req.multilink[PPP_SELF];
	bundle->mppe_server = !ppp_link_get_origination(link);
	bundle->mppe_auth = lcp_req.auth[bundle->mppe_server];
	ppp_link_get_mppe(link, bundle->mppe_server, &bundle->mppe);

	/* Initialize ng_ppp(4) node configuration, enabling the first link */
	if (ppp_node_get_config(bundle->node, &bundle->node_conf) == -1)
		goto fail;
	bundle->node_conf.links[0].enableLink = 1;
	bundle->node_conf.bund.mrru = lcp_req.mrru[PPP_PEER];
	bundle->node_conf.bund.enableMultilink = bundle->multilink;
	bundle->node_conf.bund.xmitShortSeq = lcp_req.shortseq[PPP_PEER];
	bundle->node_conf.bund.recvShortSeq = lcp_req.shortseq[PPP_SELF];

	/* Update ng_ppp(4) node configuration */
	if (ppp_node_set_config(bundle->node, &bundle->node_conf) == -1)
		goto fail;

	/* Acquire bundle configuration */
	if (ppp_bundle_configure(bundle) == -1) {
		ppp_log_put(ppp_link_get_log(link), LOG_ERR,
		    "%s: %m", "error configuring bundle");
		goto fail;
	}

	/* Receive all node bypass packets */
	ppp_node_set_recv(node, ppp_bundle_node_recv, bundle);

	/* Done */
	return (bundle);

fail:
	/* Clean up after failure */
	paction_cancel(&bundle->action);
	pevent_unregister(&bundle->lcp_event);
	ppp_fsm_destroy(&bundle->lcp);
	ppp_engine_del_bundle(bundle->engine, bundle);
	FREE(BUNDLE_MTYPE, bundle);
	return (NULL);
}

/*
 * Close a bundle.
 */
void
ppp_bundle_close(struct ppp_bundle *bundle)
{
	int i;

	if (bundle->ipcp != NULL)
		ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
	if (bundle->ccp != NULL)
		ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
	for (i = 0; i < bundle->nlinks; i++)
		ppp_link_close(bundle->links[i]);
}

/*
 * Destroy a bundle.
 */
void
ppp_bundle_destroy(struct ppp_bundle **bundlep)
{
	struct ppp_bundle *const bundle = *bundlep;

	/* Avoid recursion */
	if (bundle == NULL)
		return;
	*bundlep = NULL;
	if (bundle->shutdown)
		return;
	bundle->shutdown = 1;

	/* XXX disable traffic on node first? */

	/* Unplumb bundle and release IP address */
	if (bundle->plumb_arg != NULL) {
		ppp_engine_bundle_unplumb(bundle->engine,
		    bundle->plumb_arg, bundle);
	}
	if (bundle->conf.ip[PPP_PEER].s_addr != 0) {
		ppp_engine_release_ip(bundle->engine,
		    bundle, bundle->conf.ip[PPP_PEER]);
	}

	/* Destroy all links */
	while (bundle->nlinks > 0) {
		struct ppp_link *link = bundle->links[0];

		ppp_link_destroy(&link);
	}

	/* Destroy bundle */
	paction_cancel(&bundle->action);
	ppp_engine_del_bundle(bundle->engine, bundle);
	pevent_unregister(&bundle->config_timeout);
	pevent_unregister(&bundle->lcp_event);
	pevent_unregister(&bundle->ipcp_event);
	pevent_unregister(&bundle->ccp_event);
	ppp_fsm_destroy(&bundle->lcp);
	ppp_fsm_destroy(&bundle->ipcp);
	ppp_fsm_destroy(&bundle->ccp);
	ppp_node_destroy(&bundle->node);
	ppp_log_close(&bundle->log);
	FREE(BUNDLE_MTYPE, bundle);
}

const char *
ppp_bundle_get_authname(struct ppp_bundle *bundle, int dir)
{
	dir &= 1;
	return (bundle->authname[dir]);
}

void
ppp_bundle_get_eid(struct ppp_bundle *bundle, int dir, struct ppp_eid *eid)
{
	dir &= 1;
	*eid = bundle->eid[dir];
}

int
ppp_bundle_get_multilink(struct ppp_bundle *bundle)
{
	return (bundle->multilink);
}

int
ppp_bundle_get_links(struct ppp_bundle *bundle, struct ppp_link **list, int max)
{
	int num;
	int i;

	num = MIN(bundle->nlinks, max);
	for (i = 0; i < num; i++)
		list[i] = bundle->links[i];
	return (num);
}

int
ppp_bundle_get_ipcp(struct ppp_bundle *bundle,
	struct ppp_ipcp_req *ipcp, int *is_up)
{
	if (bundle->ipcp == NULL) {
		errno = ENXIO;
		return (-1);
	}
	if (ipcp != NULL)
		ppp_ipcp_get_req(bundle->ipcp, ipcp);
	if (is_up != NULL)
		*is_up = (ppp_fsm_get_state(bundle->ipcp) == FSM_STATE_OPENED);
	return (0);
}

int
ppp_bundle_get_ccp(struct ppp_bundle *bundle,
	struct ppp_ccp_req *ccp, int *is_up)
{
	if (bundle->ccp == NULL) {
		errno = ENXIO;
		return (-1);
	}
	if (ccp != NULL)
		ppp_ccp_get_req(bundle->ccp, ccp);
	if (is_up != NULL)
		*is_up = (ppp_fsm_get_state(bundle->ccp) == FSM_STATE_OPENED);
	return (0);
}

void *
ppp_bundle_get_cookie(struct ppp_bundle *bundle)
{
	return (bundle->cookie);
}

/*
 * Join a new link into a bundle that already has one or more links.
 */
int
ppp_bundle_join(struct ppp_bundle *bundle, struct ppp_link *link,
	struct ppp_node *node, u_int16_t *link_num)
{
	struct ppp_channel *const device = ppp_link_get_device(link);
	struct ng_ppp_node_conf bconf;
	const char *path;
	const char *hook;

	/* Sanity check */
	if (!bundle->multilink) {
		LOG(LOG_ERR, "multilink disabled on this bundle");
		errno = EINVAL;
		return (-1);
	}
	if (bundle->nlinks == NG_PPP_MAX_LINKS) {
		LOG(LOG_ERR, "too many links in this bundle");
		errno = ENOSPC;
		return (-1);
	}

	/* Disconnect device from link's node */
	if (ppp_node_disconnect(bundle->node, 0) == -1) {
		LOG(LOG_ERR, "can't disconnect link device: %m");
		return (-1);
	}

	/* Connect link's device to our node */
	if ((path = ppp_channel_get_node(device)) == NULL
	    || (hook = ppp_channel_get_hook(device)) == NULL) {
		LOG(LOG_ERR, "link's channel is not a device");
		return (-1);
	}
	if (ppp_node_connect(bundle->node, bundle->nlinks, path, hook) == -1) {
		LOG(LOG_ERR, "%s: %m", "connecting device to node");
		return (-1);
	}

	/* Copy over link's configuration from link node to bundle node */
	if (ppp_node_get_config(node, &bconf) == -1) {
		LOG(LOG_ERR, "can't get link's node configuration: %m");
		return (-1);
	}
	bundle->node_conf.links[bundle->nlinks] = bconf.links[0];
	bundle->node_conf.links[bundle->nlinks].enableLink = 1;

	/* Update node configuration */
	if (ppp_node_set_config(bundle->node, &bundle->node_conf) == -1)
		goto fail;

	/* Done */
	*link_num = bundle->nlinks;
	bundle->links[bundle->nlinks++] = link;
	return (0);

fail:
	/* Clean up after failure */
	bundle->node_conf.links[bundle->nlinks].enableLink = 0;
	(void)ppp_node_set_config(bundle->node, &bundle->node_conf);
	return (-1);
}

/*
 * Remove a link from a bundle.
 */
void
ppp_bundle_unjoin(struct ppp_bundle **bundlep, struct ppp_link *link)
{
	struct ppp_bundle *bundle = *bundlep;
	int link_num;

	/* Get bundle */
	if (bundle == NULL)
		return;
	*bundlep = NULL;

	/* Find link; do nothing if not found */
	for (link_num = 0; link_num < bundle->nlinks
	    && bundle->links[link_num] != link; link_num++);
	if (link_num == bundle->nlinks) {
		LOG(LOG_ERR, "link %p not found in bundle", link);
		return;
	}

	/* Disable traffic on link */
	bundle->node_conf.links[link_num].enableLink = 0;
	if (ppp_node_set_config(bundle->node, &bundle->node_conf) == -1)
		LOG(LOG_ERR, "can't disable link: %m");

	/* Disconnect link's device from bundle's node */
	if (ppp_node_disconnect(bundle->node, link_num) == -1)
		LOG(LOG_ERR, "can't disconnect link device: %m");

	/* Remove link from bundle */
	memmove(&bundle->links[link_num], &bundle->links[link_num + 1],
	    (--bundle->nlinks - link_num) * sizeof(*bundle->links));

	/* If no links remain, remove bundle */
	if (bundle->nlinks == 0)
		ppp_bundle_destroy(&bundle);
}

/*
 * Handle protocol rejection by peer.
 */
void
ppp_bundle_protorej(struct ppp_bundle *bundle, u_int16_t proto)
{
	switch (proto) {
	case PPP_PROTO_LCP:			/* these are required */
	case PPP_PROTO_MP:
	case PPP_PROTO_IPCP:
	case PPP_PROTO_IP:
	case PPP_PROTO_VJCOMP:
	case PPP_PROTO_VJUNCOMP:
	case PPP_PROTO_COMPD:
		ppp_bundle_shutdown(bundle);
		break;
	case PPP_PROTO_CCP:			/* this one is maybe optional */
		if (bundle->conf.mppe_reqd)
			ppp_bundle_shutdown(bundle);
		else {
			pevent_unregister(&bundle->ccp_event);
			ppp_fsm_destroy(&bundle->ccp);
		}
		return;
	default:				/* others we don't care */
		break;
	}
}

/*
 * Write to ng_ppp(4) node "bypass" hook.
 */
int
ppp_bundle_write(struct ppp_bundle *bundle, u_int link_num,
	u_int16_t proto, const void *data, size_t len)
{
	return (ppp_node_write(bundle->node, link_num, proto, data, len));
}

/***********************************************************************
			BUNDLE CONFIGURATION
***********************************************************************/

#define BCONFIG_MTYPE		"ppp_bundle.config"
#define CONFIG_TIMEOUT		20			/* 20 seconds */

/* Configuration state */
struct ppp_bundle_config_state {
	struct ppp_engine		*engine;
	struct ppp_bundle		*bundle;
	struct ppp_link			*link;
	struct ppp_bundle_config	config;
	void				*cookie;
};

static pevent_handler_t		ppp_bundle_config_timeout;

static paction_handler_t	ppp_bundle_configure_main;
static paction_finish_t		ppp_bundle_configure_finish;

/*
 * Initiate action to configure bundle.
 */
static int
ppp_bundle_configure(struct ppp_bundle *bundle)
{
	struct ppp_bundle_config_state *state;

	/* Initialize state */
	if ((state = MALLOC(BCONFIG_MTYPE, sizeof(*state))) == NULL)
		return (-1);
	memset(state, 0, sizeof(*state));
	state->engine = bundle->engine;
	state->bundle = bundle;
	state->link = bundle->links[0];

	/* Set a timeout for ppp_engine_bundle_config() to return */
	if (pevent_register(bundle->ev_ctx, &bundle->config_timeout, 0,
	    bundle->mutex, ppp_bundle_config_timeout, bundle, PEVENT_TIME,
	    CONFIG_TIMEOUT * 1000) == -1) {
		LOG(LOG_ERR, "%s: %m", "pevent_register");
		goto fail;
	}

	/* Get the configuration in a separate thread */
	if (paction_start(&bundle->action, bundle->mutex,
	    ppp_bundle_configure_main, ppp_bundle_configure_finish,
	    state) == -1) {
		LOG(LOG_ERR, "%s: %m", "paction_start");
		goto fail;
	}

	/* Done */
	return (0);

fail:
	/* Clean up */
	paction_cancel(&bundle->action);
	pevent_unregister(&bundle->config_timeout);
	FREE(BCONFIG_MTYPE, state);
	return (-1);
}

/*
 * Configure bundle main routine.
 * 
 * This is called from a separate thread.
 */
static void
ppp_bundle_configure_main(void *arg)
{
	struct ppp_bundle_config_state *const state = arg;

	/* Get configuration for the new bundle */
	state->cookie = ppp_engine_bundle_config(state->engine,
	    state->link, &state->config);

	/* Disable 56-bit MPPE if not supported */
#ifdef MPPE_56_UNSUPPORTED
	state->config.mppe_56 = 0;
#endif
}

/*
 * Configure bundle finish routine.
 * 
 * This is called from a separate thread.
 */
static void
ppp_bundle_configure_finish(void *arg, int was_canceled)
{
	struct ppp_bundle_config_state *const state = arg;
	struct ppp_bundle *bundle = state->bundle;
	struct ppp_fsm_instance *inst = NULL;
	struct ppp_ipcp_config ipcp_config;
	int i;

	/* Canceled? */
	if (was_canceled)
		goto done;

	/* Cancel config timer */
	pevent_unregister(&bundle->config_timeout);

	/* Check result */
	if (state->cookie == NULL) {
		ppp_log_put(ppp_link_get_log(bundle->links[0]),
		    LOG_ERR, "failed to configure new bundle");
		goto fail;
	}

	/* Copy config info to bundle */
	bundle->conf = state->config;
	bundle->cookie = state->cookie;
	bundle->conf.logname[sizeof(bundle->conf.logname) - 1] = '\0';

	/* Create log */
	if ((bundle->log = ppp_log_prefix(ppp_engine_get_log(bundle->engine),
	    "%s: ", *bundle->conf.logname != '\0' ? bundle->conf.logname :
	    ppp_link_get_authname(bundle->links[0], PPP_PEER))) == NULL) {
		ppp_log_put(ppp_link_get_log(bundle->links[0]),
		    LOG_ERR, "failed to create bundle log: %m");
		goto fail;
	}

	/* Create LCP FSM */
	if ((inst = ppp_lcp_create(NULL)) == NULL) {
		LOG(LOG_ERR, "failed to create LCP: %m");
		goto fail;
	}
	if ((bundle->lcp = ppp_fsm_create(bundle->ev_ctx,
	    bundle->mutex, inst, bundle->log)) == NULL) {
		LOG(LOG_ERR, "failed to create LCP: %m");
		goto fail;
	}
	inst = NULL;

	/* Listen for LCP events */
	if (pevent_register(bundle->ev_ctx, &bundle->lcp_event,
	    PEVENT_RECURRING, bundle->mutex, ppp_bundle_lcp_event, bundle,
	    PEVENT_MESG_PORT, ppp_fsm_get_outport(bundle->lcp)) == -1) {
		LOG(LOG_ERR, "%s: %m", "adding read event for lcp");
		goto fail;
	}

	/* Create IPCP FSM */
	memset(&ipcp_config, 0, sizeof(ipcp_config));
	if (bundle->conf.dns_servers[0].s_addr != 0) {
		ipcp_config.dns[0] = bundle->conf.dns_servers[0];
		ipcp_config.dns[1] = bundle->conf.dns_servers[1];
		ipcp_config.do_dns[PPP_PEER] = 1;
	}
	if (bundle->conf.nbns_servers[0].s_addr != 0) {
		ipcp_config.nbns[0] = bundle->conf.nbns_servers[0];
		ipcp_config.nbns[1] = bundle->conf.nbns_servers[1];
		ipcp_config.do_nbns[PPP_PEER] = 1;
	}
	for (i = 0; i < 2; i++) {
		ipcp_config.ip[i] = bundle->conf.ip[i];
		ipcp_config.mask[i].s_addr
		    = (ipcp_config.ip[i].s_addr == 0) ? 0 : 0xffffffff;
	}
	if ((inst = ppp_ipcp_create(&ipcp_config, bundle->node)) == NULL) {
		LOG(LOG_ERR, "failed to create IPCP: %m");
		goto fail;
	}
	if ((bundle->ipcp = ppp_fsm_create(bundle->ev_ctx,
	    bundle->mutex, inst, bundle->log)) == NULL) {
		LOG(LOG_ERR, "failed to create IPCP: %m");
		goto fail;
	}
	inst = NULL;

	/* Listen for IPCP events */
	if (pevent_register(bundle->ev_ctx, &bundle->ipcp_event,
	    PEVENT_RECURRING, bundle->mutex, ppp_bundle_ipcp_event, bundle,
	    PEVENT_MESG_PORT, ppp_fsm_get_outport(bundle->ipcp)) == -1) {
		LOG(LOG_ERR, "%s: %m", "adding ipcp event");
		goto fail;
	}

	/* Start IPCP */
	ppp_fsm_input(bundle->ipcp, FSM_INPUT_UP);
	ppp_fsm_input(bundle->ipcp, FSM_INPUT_OPEN);

	/* Create CCP FSM (if MPPE enabled) */
	if (bundle->conf.mppe_40
	    || bundle->conf.mppe_56
	    || bundle->conf.mppe_128) {
		struct ppp_ccp_config ccp_config;
		int i;

		/* Create CCP config */
		memset(&ccp_config, 0, sizeof(ccp_config));
		for (i = 0; i < 2; i++) {
			if (bundle->conf.mppe_40)
				ccp_config.mppe40[i] = 1;
			if (bundle->conf.mppe_56)
				ccp_config.mppe56[i] = 1;
			if (bundle->conf.mppe_128)
				ccp_config.mppe128[i] = 1;
			if (bundle->conf.mppe_stateless)
				ccp_config.mppe_stateless[i] = 1;
		}

		/* Derive the MPPE keys we'll need */
		switch (bundle->mppe_auth) {
		case PPP_AUTH_CHAP_MSV1:
			for (i = 0; i < 2; i++) {
				memcpy(&bundle->mppe_64[i],
				    bundle->mppe.msv1.key_64,
				    sizeof(bundle->mppe_64[i]));
				memcpy(&bundle->mppe_128[i],
				    bundle->mppe.msv1.key_128,
				    sizeof(bundle->mppe_128[i]));
			}
			break;
		case PPP_AUTH_CHAP_MSV2:
			for (i = 0; i < 2; i++) {
				memcpy(&bundle->mppe_64[i],
				    bundle->mppe.msv2.keys[!i
				      ^ bundle->mppe_server],
				    sizeof(bundle->mppe_64[i]));
				memcpy(&bundle->mppe_128[i],
				    bundle->mppe.msv2.keys[!i
				      ^ bundle->mppe_server],
				    sizeof(bundle->mppe_128[i]));
			}
			break;
		default:
			if (bundle->conf.mppe_reqd) {
				LOG(LOG_ERR, "MPPE %s but MS-CHAP was not"
				    " used for authentication", "required");
				goto fail;
			} else {
				LOG(LOG_WARNING, "MPPE %s but MS-CHAP was not"
				    " used for authentication", "requested");
				goto done;
			}
			break;
		}

		/* Create CCP instance */
		if ((inst = ppp_ccp_create(&ccp_config,
		    bundle->node)) == NULL) {
			LOG(LOG_ERR, "failed to create CCP: %m");
			goto fail;
		}
		if ((bundle->ccp = ppp_fsm_create(bundle->ev_ctx,
		    bundle->mutex, inst, bundle->log)) == NULL) {
			LOG(LOG_ERR, "failed to create CCP: %m");
			goto fail;
		}
		inst = NULL;

		/* Listen for CCP events */
		if (pevent_register(bundle->ev_ctx, &bundle->ccp_event,
		    PEVENT_RECURRING, bundle->mutex, ppp_bundle_ccp_event,
		    bundle, PEVENT_MESG_PORT, ppp_fsm_get_outport(bundle->ccp))
		    == -1) {
			LOG(LOG_ERR, "%s: %m", "adding ccp event");
			goto fail;
		}

		/* Start CCP */
		ppp_fsm_input(bundle->ccp, FSM_INPUT_UP);
		ppp_fsm_input(bundle->ccp, FSM_INPUT_OPEN);
	}

done:
	/* Done */
	FREE(BCONFIG_MTYPE, state);
	return;

fail:
	/* Clean up after failure */
	if (inst != NULL)
		(*inst->type->destroy)(inst);
	ppp_bundle_destroy(&bundle);
	FREE(BCONFIG_MTYPE, state);
}

/*
 * Handle a timeout trying to configure the bundle.
 */
static void
ppp_bundle_config_timeout(void *arg)
{
	struct ppp_bundle *bundle = arg;

	LOG(LOG_ERR, "timed out configuring new bundle");
	paction_cancel(&bundle->action);
	ppp_bundle_destroy(&bundle);
}

/***********************************************************************
			PPP NODE OUTPUT HANDLER
***********************************************************************/

/*
 * Handle data received from the node's bypass hook.
 */
static void
ppp_bundle_node_recv(void *arg, u_int link_num,
	u_int16_t proto, u_char *data, size_t len)
{
	struct ppp_bundle *const bundle = arg;

	/* Check for link-specific packets */
	if (PPP_PROTO_LINK_LAYER(proto) && link_num != NG_PPP_BUNDLE_LINKNUM) {
		if (link_num >= bundle->nlinks)
			return;
		ppp_link_recv_bypass(bundle->links[link_num], proto, data, len);
		return;
	}

	/* Handle packet at the bundle level */
	switch (proto) {
	case PPP_PROTO_IPCP:
		if (bundle->ipcp != NULL)
			ppp_fsm_input(bundle->ipcp, FSM_INPUT_DATA, data, len);
		break;
	case PPP_PROTO_CCP:
		if (bundle->ccp != NULL)
			ppp_fsm_input(bundle->ccp, FSM_INPUT_DATA, data, len);
		else if (bundle->action == NULL)	/* got config already */
			goto proto_reject;
		break;
	case PPP_PROTO_MP:
	case PPP_PROTO_IP:
	case PPP_PROTO_VJCOMP:
	case PPP_PROTO_VJUNCOMP:
	case PPP_PROTO_COMPD:
		break;
	default:
		goto proto_reject;
	}
	return;

proto_reject:
	/* Send a protocol reject */
	if (bundle->lcp != NULL) {
		ppp_fsm_input(bundle->lcp,
		    FSM_INPUT_XMIT_PROTOREJ, proto, data, len);
	}
}

/***********************************************************************
			LCP EVENT HANDLER
***********************************************************************/

static void
ppp_bundle_lcp_event(void *arg)
{
	struct ppp_bundle *const bundle = arg;
	struct mesg_port *const outport = ppp_fsm_get_outport(bundle->lcp);
	struct ppp_fsm_output *output;

	/* Read and handle all FSM events */
	while ((output = mesg_port_get(outport, 0)) != NULL) {

		/* Check it out */
		switch (output->type) {
		case FSM_OUTPUT_DATA:		/* probably an echo reply */
			ppp_node_write(bundle->node, NG_PPP_BUNDLE_LINKNUM,
			    PPP_PROTO_LCP, output->u.data.data,
			    output->u.data.length);
			break;
		case FSM_OUTPUT_PROTOREJ:
		    {
			LOG(LOG_NOTICE, "peer rejected protocol 0x%04x",
			    output->u.proto);
			ppp_bundle_protorej(bundle, output->u.proto);
			break;
		    }
		case FSM_OUTPUT_OPEN:
		case FSM_OUTPUT_CLOSE:
		case FSM_OUTPUT_UP:
		case FSM_OUTPUT_DOWN:
		case FSM_OUTPUT_DEAD:
			LOG(LOG_NOTICE, "unexpected LCP output: %s",
			    ppp_fsm_output_str(output));
			break;
		}

		/* Free output */
		ppp_fsm_free_output(output);
	}
}

/***********************************************************************
			IPCP EVENT HANDLER
***********************************************************************/

static void
ppp_bundle_ipcp_event(void *arg)
{
	struct ppp_bundle *const bundle = arg;
	struct mesg_port *const outport = ppp_fsm_get_outport(bundle->ipcp);
	struct ppp_fsm_output *output;

	/* Read and handle all FSM events */
	while ((output = mesg_port_get(outport, 0)) != NULL) {

		/* Check it out */
		switch (output->type) {
		case FSM_OUTPUT_OPEN:
		case FSM_OUTPUT_CLOSE:
			break;
		case FSM_OUTPUT_UP:
		    {
			static const char *chooks[][2] = {
			    { NG_PPP_HOOK_VJC_IP,	NG_VJC_HOOK_IP },
			    { NG_PPP_HOOK_VJC_COMP,	NG_VJC_HOOK_VJCOMP },
			    { NG_PPP_HOOK_VJC_UNCOMP,	NG_VJC_HOOK_VJUNCOMP },
			    { NG_PPP_HOOK_VJC_VJIP,	NG_VJC_HOOK_VJIP },
			};
			struct ng_ppp_node_conf conf;
			struct ngm_vjc_config vjc;
			struct ppp_ipcp_req req;
			struct ngm_mkpeer mkpeer;
			char buf[16];
			u_int mtu;
			int i;

			/* Remember IPCP is up */
			bundle->ipcp_up = 1;

			/* Get negotiated parameters */
			ppp_ipcp_get_req(bundle->ipcp, &req);
			strlcpy(buf, inet_ntoa(req.ip[PPP_SELF]), sizeof(buf));
			LOG(LOG_INFO, "IPCP successfully configured: "
			    "%s -> %s", buf, inet_ntoa(req.ip[PPP_PEER]));

			/* Get ppp node config */
			if (ppp_node_get_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "can't get ppp node config: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}

			/* Configure IP traffic */
			conf.bund.enableIP =
			    !bundle->conf.mppe_reqd || bundle->ccp_up;
			conf.bund.enableVJCompression = 0;
			conf.bund.enableVJDecompression = 0;

			/* Skip VJC config if not negotiated */
			if (!req.vjc[PPP_SELF].enabled
			    && !req.vjc[PPP_PEER].enabled)
				goto no_vjc;

			/* Attach vjc node to ppp node */
			(void)ppp_node_send_msg(bundle->node,
			    chooks[0][0], NGM_GENERIC_COOKIE,
			    NGM_SHUTDOWN, NULL, 0);
			memset(&mkpeer, 0, sizeof(mkpeer));
			strlcpy(mkpeer.type,
			    NG_VJC_NODE_TYPE, sizeof(mkpeer.type));
			strlcpy(mkpeer.ourhook,
			    chooks[0][0], sizeof(mkpeer.ourhook));
			strlcpy(mkpeer.peerhook,
			    chooks[0][1], sizeof(mkpeer.peerhook));
			if (ppp_node_send_msg(bundle->node, NULL,
			    NGM_GENERIC_COOKIE, NGM_MKPEER, &mkpeer,
			    sizeof(mkpeer)) == -1) {
				LOG(LOG_ERR, "can't attach VJC node: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}
			for (i = 1; i < sizeof(chooks) / sizeof(*chooks); i++) {
				struct ngm_connect connect;

				memset(&connect, 0, sizeof(connect));
				strlcpy(connect.path,
				    NG_PPP_HOOK_VJC_IP, sizeof(connect.path));
				strlcpy(connect.ourhook,
				    chooks[i][0], sizeof(connect.ourhook));
				strlcpy(connect.peerhook,
				    chooks[i][1], sizeof(connect.peerhook));
				if (ppp_node_send_msg(bundle->node, NULL,
				    NGM_GENERIC_COOKIE, NGM_CONNECT, &connect,
				    sizeof(connect)) == -1) {
					LOG(LOG_ERR,
					    "can't connect VJC node: %m");
					ppp_fsm_input(bundle->ipcp,
					    FSM_INPUT_CLOSE);
					break;
				}
			}
			if (i < sizeof(chooks) / sizeof(*chooks))  /* failed */
				break;

			/* Configure VJ compression node */
			memset(&vjc, 0, sizeof(vjc));
			vjc.enableComp = req.vjc[PPP_PEER].enabled;
			vjc.enableDecomp = req.vjc[PPP_SELF].enabled;
			vjc.maxChannel = req.vjc[PPP_PEER].maxchan;
			vjc.compressCID = req.vjc[PPP_PEER].compcid;
			if (ppp_node_send_msg(bundle->node, NG_PPP_HOOK_VJC_IP,
			    NGM_VJC_COOKIE, NGM_VJC_SET_CONFIG,
			    &vjc, sizeof(vjc)) == -1) {
				LOG(LOG_ERR, "error configuring VJC node: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}

			/* Configure ppp node to enable VJ (de)compression */
			conf.bund.enableVJCompression
			    = req.vjc[PPP_PEER].enabled;
			conf.bund.enableVJDecompression
			    = req.vjc[PPP_SELF].enabled;

no_vjc:
			/*
			 * Determine the MTU for the interface
			 *
			 * XXX if/when compression is added, we must account
			 * XXX for any possible payload expansion here as well
			 */
			mtu = bundle->multilink ? bundle->node_conf.bund.mrru
			    : bundle->node_conf.links[0].mru;
			if (bundle->conf.mppe_40
			    || bundle->conf.mppe_56
			    || bundle->conf.mppe_128)
				mtu -= 4;	/* allow for mppe header */

			/* Plumb the 'top' side of the node */
			if ((bundle->plumb_arg =
			    ppp_engine_bundle_plumb(bundle->engine, bundle,
			    ppp_node_get_path(bundle->node), NG_PPP_HOOK_INET,
			    req.ip, req.dns, req.nbns, mtu)) == NULL) {
				LOG(LOG_ERR, "error plumbing ppp node: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}

			/* Update ppp node configuration */
			if (ppp_node_set_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "error configuring ppp node: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}
			break;
		    }
		case FSM_OUTPUT_DOWN:
		    {
			struct ng_ppp_node_conf conf;
			void *parg;

			/* Remember IPCP is down */
			bundle->ipcp_up = 0;

			/* Get ppp node config */
			if (ppp_node_get_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "can't get ppp node config: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}

			/* Disable IP and VJC packets */
			conf.bund.enableIP = 0;
			conf.bund.enableVJCompression = 0;
			conf.bund.enableVJDecompression = 0;

			/* Update ppp node configuration */
			if (ppp_node_set_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "error configuring ppp node: %m");
				ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
				break;
			}

			/* Clobber VJC node (if any) */
			(void)ppp_node_send_msg(bundle->node,
			    NG_PPP_HOOK_VJC_IP, NGM_GENERIC_COOKIE,
			    NGM_SHUTDOWN, NULL, 0);

			/* Disconnect the 'top' side of the node */
			if ((parg = bundle->plumb_arg) != NULL) {
				bundle->plumb_arg = NULL;
				ppp_engine_bundle_unplumb(bundle->engine,
				    parg, bundle);
			}
			break;
		    }
		case FSM_OUTPUT_DATA:
			ppp_node_write(bundle->node, NG_PPP_BUNDLE_LINKNUM,
			    PPP_PROTO_IPCP, output->u.data.data,
			    output->u.data.length);
			break;
		case FSM_OUTPUT_PROTOREJ:
			LOG(LOG_NOTICE, "unexpected IPCP output: %s",
			    ppp_fsm_output_str(output));
			break;
		case FSM_OUTPUT_DEAD:
			LOG(LOG_INFO, "IPCP is dead: %s",
			    ppp_fsm_reason_str(output));
			ppp_fsm_destroy(&bundle->ipcp);
			ppp_bundle_fsm_dead(bundle);
			ppp_fsm_free_output(output);
			return;
		}

		/* Free output */
		ppp_fsm_free_output(output);
	}
}

/***********************************************************************
			CCP EVENT HANDLER
***********************************************************************/

static void
ppp_bundle_ccp_event(void *arg)
{
	struct ppp_bundle *const bundle = arg;
	struct mesg_port *const outport = ppp_fsm_get_outport(bundle->ccp);
	struct ppp_fsm_output *output;

	/* Read and handle all FSM events */
	while ((output = mesg_port_get(outport, 0)) != NULL) {

		/* Check it out */
		switch (output->type) {
		case FSM_OUTPUT_OPEN:
		case FSM_OUTPUT_CLOSE:
			break;
		case FSM_OUTPUT_UP:
		    {
			struct ng_ppp_node_conf conf;
			struct ngm_connect connect;
			struct ngm_mkpeer mkpeer;
			struct ppp_ccp_req req;
			char buf[2][64];
			int i;

			/* Remember CCP is up */
			bundle->ccp_up = 1;

			/* Some peers don't know 56 bit and will ack it too */
			for (i = 0; i < 2; i++) {
				if (req.mppe56[i]
				    && (req.mppe40[i] || req.mppe128[i]))
					req.mppe56[i] = 0;
			}

			/* Get negotiated parameters */
			ppp_ccp_get_req(bundle->ccp, &req);
			for (i = 0; i < 2; i++) {
				strlcpy(buf[i], "MPPC", sizeof(buf[i]));
				if (req.mppc[i]) {
					strlcat(buf[i], " compression",
					    sizeof(buf[i]));
				}
				if (req.mppe40[i]) {
					strlcat(buf[i], " 40 bit encryption",
					    sizeof(buf[i]));
				} else if (req.mppe56[i]) {
					strlcat(buf[i], " 56 bit encryption",
					    sizeof(buf[i]));
				} else if (req.mppe128[i]) {
					strlcat(buf[i], " 128 bit encryption",
					    sizeof(buf[i]));
				}
				if (req.mppe_stateless[i]) {
					strlcat(buf[i], ", stateless",
					    sizeof(buf[i]));
				}
			}
			LOG(LOG_INFO, "CCP successfully configured: "
			    "Recv: %s; Xmit: %s", buf[PPP_SELF], buf[PPP_PEER]);

			/* Get ppp node config */
			if (ppp_node_get_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "can't get ppp node config: %m");
				ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
				break;
			}

			/* Enable IP traffic now */
			if (bundle->ipcp_up)
				conf.bund.enableIP = 1;

			/* Configure ppp node to enable MPPC (de)compression */
			conf.bund.enableCompression = req.mppc[PPP_PEER]
			    || req.mppe40[PPP_PEER]
			    || req.mppe56[PPP_PEER]
			    || req.mppe128[PPP_PEER];
			conf.bund.enableDecompression = req.mppc[PPP_SELF]
			    || req.mppe40[PPP_SELF]
			    || req.mppe56[PPP_SELF]
			    || req.mppe128[PPP_SELF];

			/* Attach mppc node to ppp node */
			(void)ppp_node_send_msg(bundle->node,
			    NG_PPP_HOOK_COMPRESS, NGM_GENERIC_COOKIE,
			    NGM_SHUTDOWN, NULL, 0);
			memset(&mkpeer, 0, sizeof(mkpeer));
			strlcpy(mkpeer.type,
			    NG_MPPC_NODE_TYPE, sizeof(mkpeer.type));
			strlcpy(mkpeer.ourhook,
			    NG_PPP_HOOK_COMPRESS, sizeof(mkpeer.ourhook));
			strlcpy(mkpeer.peerhook,
			    NG_MPPC_HOOK_COMP, sizeof(mkpeer.peerhook));
			if (ppp_node_send_msg(bundle->node, NULL,
			    NGM_GENERIC_COOKIE, NGM_MKPEER, &mkpeer,
			    sizeof(mkpeer)) == -1) {
				LOG(LOG_ERR, "can't attach MPPC node: %m");
				ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
				break;
			}
			memset(&connect, 0, sizeof(connect));
			strlcpy(connect.path,
			    NG_PPP_HOOK_COMPRESS, sizeof(connect.path));
			strlcpy(connect.ourhook,
			    NG_PPP_HOOK_DECOMPRESS, sizeof(connect.ourhook));
			strlcpy(connect.peerhook,
			    NG_MPPC_HOOK_DECOMP, sizeof(connect.peerhook));
			if (ppp_node_send_msg(bundle->node, NULL,
			    NGM_GENERIC_COOKIE, NGM_CONNECT, &connect,
			    sizeof(connect)) == -1) {
				LOG(LOG_ERR, "can't connect MPPC node: %m");
				ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
				break;
			}

			/* Verify we have the MPPE keys that we need */
			for (i = 0; i < 2; i++) {
				if (((req.mppe40[i] || req.mppe56[i])
				     && memcmp(&bundle->mppe_64[i],
				       ppp_bundle_zero, 8) == 0)
				    || (req.mppe128[i]
				      && memcmp(&bundle->mppe_128[i],
				       ppp_bundle_zero, 16) == 0)) {
					LOG(LOG_ERR, "can't do MPPE encryption:"
					    " no keys were provided by the"
					    " authentication process");
					ppp_fsm_input(bundle->ccp,
					    FSM_INPUT_CLOSE);
					break;
				}
			}
			if (i < 2)
				break;

			/* Configure MPPC node in both directions */
			for (i = 0; i < 2; i++) {
				struct ng_mppc_config mppc;

				memset(&mppc, 0, sizeof(mppc));
				mppc.enable = req.mppc[i] || req.mppe40[i]
				    || req.mppe56[i] || req.mppe128[i];
				if (req.mppc[i])
					mppc.bits |= MPPC_BIT;
				if (req.mppe40[i] || req.mppe56[i]) {
					mppc.bits |= req.mppe40[i] ?
					    MPPE_40 : MPPE_56;
					memcpy(mppc.startkey,
					    &bundle->mppe_64[i],
					    sizeof(bundle->mppe_64[i]));
				} else if (req.mppe128[i]) {
					mppc.bits |= MPPE_128;
					memcpy(mppc.startkey,
					    &bundle->mppe_128[i],
					    sizeof(bundle->mppe_128[i]));
				}
				if (req.mppe_stateless[i])
					mppc.bits |= MPPE_STATELESS;
				if (ppp_node_send_msg(bundle->node,
				    NG_PPP_HOOK_COMPRESS, NGM_MPPC_COOKIE,
				    (i == PPP_SELF) ? NGM_MPPC_CONFIG_DECOMP
				      : NGM_MPPC_CONFIG_COMP,
				    &mppc, sizeof(mppc)) == -1) {
					LOG(LOG_ERR,
					    "error configuring MPPC node: %m");
					ppp_fsm_input(bundle->ccp,
					    FSM_INPUT_CLOSE);
					break;
				}
			}

			/* Update ppp node configuration */
			if (ppp_node_set_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "error configuring ppp node: %m");
				ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
				break;
			}
			break;
		    }
		case FSM_OUTPUT_DOWN:
		    {
			struct ng_ppp_node_conf conf;

			/* Remember CCP is down */
			bundle->ccp_up = 0;

			/* Get ppp node config */
			if (ppp_node_get_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "can't get ppp node config: %m");
				ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
				break;
			}

			/* Disable IP traffic if encryption is required */
			if (bundle->conf.mppe_reqd)
				conf.bund.enableIP = 0;

			/* Disable compression */
			conf.bund.enableCompression = 0;
			conf.bund.enableDecompression = 0;

			/* Update ppp node configuration */
			if (ppp_node_set_config(bundle->node, &conf) == -1) {
				LOG(LOG_ERR, "error configuring ppp node: %m");
				ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
				break;
			}

			/* Clobber MPPC node */
			(void)ppp_node_send_msg(bundle->node,
			    NG_PPP_HOOK_COMPRESS, NGM_GENERIC_COOKIE,
			    NGM_SHUTDOWN, NULL, 0);
			break;
		    }
		case FSM_OUTPUT_DATA:
			ppp_node_write(bundle->node, NG_PPP_BUNDLE_LINKNUM,
			    PPP_PROTO_CCP, output->u.data.data,
			    output->u.data.length);
			break;
		case FSM_OUTPUT_PROTOREJ:
			LOG(LOG_NOTICE, "unexpected CCP output: %s",
			    ppp_fsm_output_str(output));
			break;
		case FSM_OUTPUT_DEAD:
			LOG(LOG_INFO, "CCP is dead: %s",
			    ppp_fsm_reason_str(output));
			ppp_fsm_destroy(&bundle->ccp);
			ppp_bundle_fsm_dead(bundle);
			ppp_fsm_free_output(output);
			return;
		}

		/* Free output */
		ppp_fsm_free_output(output);
	}
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

/*
 * One of our network control protocols has died.
 * If this is fatal, then shutdown the bundle.
 */
static void
ppp_bundle_fsm_dead(struct ppp_bundle *bundle)
{
	if (bundle->ipcp == NULL) {
		ppp_bundle_shutdown(bundle);
		return;
	}
	if (bundle->conf.mppe_reqd && bundle->ccp == NULL) {
		LOG(LOG_ERR, "MPPE is required but CCP negotiation failed");
		ppp_bundle_shutdown(bundle);
	}
}

/*
 * Shutdown the bundle.
 */
static void
ppp_bundle_shutdown(struct ppp_bundle *bundle)
{
	int i;

	if (bundle->ipcp != NULL)
		ppp_fsm_input(bundle->ipcp, FSM_INPUT_CLOSE);
	if (bundle->ccp != NULL)
		ppp_fsm_input(bundle->ccp, FSM_INPUT_CLOSE);
	for (i = 0; i < bundle->nlinks; i++)
		ppp_link_close(bundle->links[i]);
}

