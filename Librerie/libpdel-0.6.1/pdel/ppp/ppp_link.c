
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
#include "ppp/ppp_lcp.h"
#include "ppp/ppp_node.h"
#include "ppp/ppp_engine.h"
#include "ppp/ppp_bundle.h"
#include "ppp/ppp_channel.h"
#include "ppp/ppp_link.h"

#define LINK_MTYPE		"ppp_link"
#define PKTBUFLEN		4096
#define LINK_LATENCY		100		/* arbitrary fixed value */
#define LINK_BANDWIDTH		100		/* arbitrary fixed value */

#define AUTH_TIMEOUT		20		/* time limit for auth phase */

#define WINXP_PPTP_HACK(x)	((x) - 20)	/* winxp pptp stupidity hack */

/*
 * Authorization info for one direction
 */
struct ppp_link_auth {
	struct ppp_link			*link;	/* back pointer */
	u_int16_t			proto;	/* auth protocol number */
	const struct ppp_auth_type	*type;	/* auth type negotiated */
	void				*arg;	/* auth object in progress */
	struct ppp_auth_cred		cred;	/* auth credentials */
	struct ppp_auth_resp		resp;	/* auth response */
	union ppp_auth_mppe		mppe;	/* mppe keys from ms-chap */
	struct paction			*action;/* auth acquire/check action */
};

/*
 * PPP link structure
 */
struct ppp_link {
	enum ppp_link_state	state;		/* link state */
	struct ppp_log		*log;		/* log */
	struct ppp_link_config	conf;		/* link configuration */
	struct ppp_engine	*engine;	/* ppp engine */
	struct ppp_channel	*device;	/* underlying device */
	struct ppp_node		*node;		/* ng_ppp(4) node */
	struct ppp_fsm		*lcp;		/* lcp fsm */
	struct ppp_lcp_req	lcp_req;	/* lcp negotiation result */
	struct ppp_bundle	*bundle;	/* bundle, if joined */
	struct pevent_ctx	*ev_ctx;	/* event context */
	pthread_mutex_t		*mutex;		/* mutex */
	struct pevent		*lcp_event;	/* lcp fsm event */
	struct pevent		*dev_event;	/* device event */
	struct pevent		*auth_timer;	/* timer for auth phase */
	u_char			device_up;	/* device is up */
	struct ppp_link_auth	auth[2];	/* authorization info */
	u_int16_t		link_num;	/* ppp node link number */
	u_char			shutdown;	/* normal shutdown */
};

/* Internal functions */
static void	ppp_link_join(struct ppp_link *link);
static void	ppp_link_unjoin(struct ppp_link *link);
static int	ppp_link_get_node(struct ppp_link *link);
static void	ppp_link_auth_start(struct ppp_link *link);
static void	ppp_link_auth_stop(struct ppp_link *link);

static ppp_node_recv_t	ppp_link_node_recv;

static pevent_handler_t	ppp_link_device_event;
static pevent_handler_t	ppp_link_lcp_event;
static pevent_handler_t	ppp_link_auth_timeout;

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(link->log, sev, fmt , ## args)

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/*
 * Create a new PPP link.
 *
 * The link is not returned. Instead it disappears into the PPP engine.
 * The "device" and "log" are destroyed when the link is destroyed.
 */
int
ppp_link_create(struct ppp_engine *engine, struct ppp_channel *device,
	struct ppp_link_config *conf, struct ppp_log *log)
{
	struct ppp_fsm_instance *inst = NULL;
	struct ppp_lcp_config lcp_conf;
	struct ppp_fsm *lcp = NULL;
	struct ppp_link *link;
	int esave;
	int i;
	int j;

	/* Create new link structure */
	if ((link = MALLOC(LINK_MTYPE, sizeof(*link))) == NULL)
		return (-1);
	memset(link, 0, sizeof(*link));
	link->state = PPP_LINK_DOWN;
	link->engine = engine;
	link->ev_ctx = ppp_engine_get_ev_ctx(engine);
	link->mutex = ppp_engine_get_mutex(engine);
	link->device = device;
	link->conf = *conf;
	link->log = log;
	for (i = 0; i < 2; i++)
		link->auth[i].link = link;

	/* Derive LCP configuration from link configuration and device info */
	memset(&lcp_conf, 0, sizeof(lcp_conf));
	lcp_conf.max_mru[PPP_SELF] = conf->max_self_mru;
	lcp_conf.min_mru[PPP_PEER] = conf->min_peer_mru;
	lcp_conf.accm = ppp_channel_is_async(device) ? 0x0a000000 : ~0;
	if (ppp_channel_get_acfcomp(device)) {
		lcp_conf.acfcomp[PPP_SELF] = 1;
		lcp_conf.acfcomp[PPP_PEER] = 1;
	}
	if (ppp_channel_get_pfcomp(device)) {
		lcp_conf.pfcomp[PPP_SELF] = 1;
		lcp_conf.pfcomp[PPP_PEER] = 1;
	}
	for (i = 0; i < PPP_AUTH_MAX; i++) {
		for (j = 0; j < 2; j++) {
			if ((conf->auth.allow[j] & (1 << i)) != 0)
				lcp_conf.auth[j][i] = 1;
		}
	}
	lcp_conf.eid = conf->eid;
	if (conf->multilink) {
		lcp_conf.max_mrru[PPP_SELF] = conf->max_self_mrru;
		lcp_conf.min_mrru[PPP_PEER] = conf->min_peer_mrru;
		lcp_conf.multilink[PPP_SELF] = 1;
		lcp_conf.multilink[PPP_PEER] = 1;
		lcp_conf.shortseq[PPP_SELF] = 1;
		lcp_conf.shortseq[PPP_PEER] = 1;
	}

	/* Create ppp node object */
	if (ppp_link_get_node(link) == -1)
		goto fail;

	/* Create a new LCP FSM for this link */
	if ((inst = ppp_lcp_create(&lcp_conf)) == NULL) {
		LOG(LOG_ERR, "failed to create LCP: %m");
		goto fail;
	}
	if ((link->lcp = ppp_fsm_create(link->ev_ctx,
	    link->mutex, inst, link->log)) == NULL) {
		LOG(LOG_ERR, "failed to create LCP: %m");
		goto fail;
	}
	inst = NULL;

	/* Listen for device events */
	if (pevent_register(link->ev_ctx, &link->dev_event, PEVENT_RECURRING,
	    link->mutex, ppp_link_device_event, link, PEVENT_MESG_PORT,
	    ppp_channel_get_outport(link->device)) == -1) {
		LOG(LOG_ERR, "%s: %m", "adding read event");
		goto fail;
	}

	/* Listen for LCP events */
	if (pevent_register(link->ev_ctx, &link->lcp_event, PEVENT_RECURRING,
	    link->mutex, ppp_link_lcp_event, link, PEVENT_MESG_PORT,
	    ppp_fsm_get_outport(link->lcp)) == -1) {
		LOG(LOG_ERR, "%s: %m", "adding read event");
		goto fail;
	}

	/* Notify engine of new link */
	if (ppp_engine_add_link(engine, link) == -1) {
		LOG(LOG_ERR, "failed to add link: %m");
		goto fail;
	}

	/* Start LCP negotiations (whenver link comes up) */
	ppp_fsm_input(link->lcp, FSM_INPUT_OPEN);

	/* Done */
	return (0);

fail:
	/* Clean up after failure */
	esave = errno;
	pevent_unregister(&link->dev_event);
	pevent_unregister(&link->lcp_event);
	ppp_node_destroy(&link->node);
	ppp_node_destroy(&link->node);
	ppp_fsm_destroy(&lcp);
	if (inst != NULL)
		(*inst->type->destroy)(inst);
	FREE(LINK_MTYPE, link);
	errno = esave;
	return (-1);
}

/*
 * Destroy a link.
 */
void
ppp_link_destroy(struct ppp_link **linkp)
{
	struct ppp_link *const link = *linkp;
	int r;

	/* Sanity check */
	if (link == NULL)
		return;
	*linkp = NULL;

	/* Avoid recursion */
	if (link->shutdown)
		return;
	link->shutdown = 1;

	/* Acquire lock */
	r = pthread_mutex_lock(link->mutex);
	assert(r == 0);

	/* Stop authentication (if any) */
	ppp_link_auth_stop(link);

	/* Disconnect from bundle or engine */
	ppp_bundle_unjoin(&link->bundle, link);
	ppp_engine_del_link(link->engine, link);

	/* Destroy link */
	r = pthread_mutex_unlock(link->mutex);
	assert(r == 0);
	ppp_fsm_destroy(&link->lcp);
	pevent_unregister(&link->dev_event);
	pevent_unregister(&link->lcp_event);
	ppp_node_destroy(&link->node);
	ppp_channel_destroy(&link->device);
	ppp_log_close(&link->log);
	FREE(LINK_MTYPE, link);
}

/*
 * Close a link.
 */
void
ppp_link_close(struct ppp_link *link)
{
	ppp_link_auth_stop(link);
	ppp_fsm_input(link->lcp, FSM_INPUT_CLOSE);
}

/*
 * Get the device associated with a link.
 */
struct ppp_channel *
ppp_link_get_device(struct ppp_link *link)
{
	return (link->device);
}

/*
 * Get the link's origination (PPP_SELF or PPP_PEER) which
 * is simply inherited from the underlying device.
 */
int
ppp_link_get_origination(struct ppp_link *link)
{
	return (ppp_channel_get_origination(link->device));
}

/*
 * Get link state.
 */
enum ppp_link_state
ppp_link_get_state(struct ppp_link *link)
{
	return (link->state);
}

/*
 * Get bundle associated with link, if any.
 */
struct ppp_bundle *
ppp_link_get_bundle(struct ppp_link *link)
{
	if (link->bundle == NULL)
		errno = ENXIO;
	return (link->bundle);
}

/*
 * Get LCP request state.
 */
void
ppp_link_get_lcp_req(struct ppp_link *link, struct ppp_lcp_req *req)
{
	ppp_lcp_get_req(link->lcp, req);
}

/*
 * Get link authorization name.
 *
 * Note: we reverse the sense of 'dir' because we want PPP_SELF to
 * mean my authname to peer, which is the opposite of the way that
 * authorization is negotiated, i.e., PPP_SELF is peer's auth to me.
 */
const char *
ppp_link_get_authname(struct ppp_link *link, int dir)
{
	struct ppp_link_auth *const auth = &link->auth[!dir];

	if (auth->type == NULL)
		return ("");
	switch (auth->type->index) {
	case PPP_AUTH_PAP:
		return (auth->cred.u.pap.name);
	case PPP_AUTH_CHAP_MSV1:
	case PPP_AUTH_CHAP_MSV2:
	case PPP_AUTH_CHAP_MD5:
		return (auth->cred.u.chap.name);
	default:
		return ("");
	}
}

/*
 * Get endpoint ID.
 */
void
ppp_link_get_eid(struct ppp_link *link, int dir, struct ppp_eid *eid)
{
	struct ppp_lcp_req req;

	dir &= 1;
	ppp_lcp_get_req(link->lcp, &req);
	*eid = req.eid[dir];
}

void
ppp_link_get_mppe(struct ppp_link *link, int dir, union ppp_auth_mppe *mppe)
{
	struct ppp_link_auth *const auth = &link->auth[dir & 1];

	memcpy(mppe, &auth->mppe, sizeof(*mppe));
}

/*
 * Ouput a packet on the link.
 */
void
ppp_link_write(struct ppp_link *link,
	u_int16_t proto, const void *data, size_t len)
{
	int rtn;

	/* Drop packet if channel is down */
	if (!link->device_up)
		return;

	/* Write packet */
	if (link->bundle != NULL) {
		rtn = ppp_bundle_write(link->bundle,
		    link->link_num, proto, data, len);
	} else  {
		rtn = ppp_node_write(link->node,
		    link->link_num, proto, data, len);
	}

	if (rtn == -1) {
		LOG(LOG_ERR, "%s: %m", "error writing to bypass");
		ppp_link_close(link);
	}
}

/***********************************************************************
			LCP EVENT HANDLER
***********************************************************************/

static void
ppp_link_lcp_event(void *arg)
{
	struct ppp_link *link = arg;
	struct mesg_port *const outport = ppp_fsm_get_outport(link->lcp);
	struct ppp_fsm_output *output;

	/* Read and handle all FSM events */
	while ((output = mesg_port_get(outport, 0)) != NULL) {

		/* Check it out */
		switch (output->type) {
		case FSM_OUTPUT_OPEN:
			ppp_channel_open(link->device);
			break;
		case FSM_OUTPUT_CLOSE:
			ppp_channel_close(link->device);
			break;
		case FSM_OUTPUT_UP:
		    {
			struct ng_ppp_node_conf conf;
			struct ng_ppp_link_conf *const lconf = &conf.links[0];

			/* Get ng_ppp(4) node configuration */
			if (ppp_node_get_config(link->node, &conf) == -1) {
				LOG(LOG_ERR, "can't configure node: %m");
				ppp_link_close(link);
				break;
			}

			/* Update with negotiated LCP parameters */
			ppp_lcp_get_req(link->lcp, &link->lcp_req);
			lconf->enableProtoComp = link->lcp_req.pfcomp[PPP_PEER];
			lconf->enableACFComp = link->lcp_req.acfcomp[PPP_PEER];
			lconf->mru = MIN(
			    WINXP_PPTP_HACK(link->lcp_req.mru[PPP_PEER]),
			    ppp_channel_get_mtu(link->device));
			lconf->latency = LINK_LATENCY;
			lconf->bandwidth = LINK_BANDWIDTH;
			if (ppp_node_set_config(link->node, &conf) == -1) {
				LOG(LOG_ERR, "can't configure node: %m");
				ppp_link_close(link);
				break;
			}

			/* Begin authentication phase */
			link->state = PPP_LINK_AUTH;
			ppp_link_auth_start(link);
			break;
		    }
		case FSM_OUTPUT_DOWN:
			link->state = PPP_LINK_DOWN;
			ppp_link_auth_stop(link);
			if (link->bundle != NULL) {

				/* Leave our bundle */
				assert(link->node == NULL);
				ppp_bundle_unjoin(&link->bundle, link);

				/* Create a new node for this link */
				if (ppp_link_get_node(link) == -1) {
					ppp_link_close(link);
					break;
				}
			}
			break;
		case FSM_OUTPUT_DATA:
			ppp_link_write(link, PPP_PROTO_LCP,
			    output->u.data.data, output->u.data.length);
			break;
		case FSM_OUTPUT_PROTOREJ:
		    {
			/* Log it */
			LOG(LOG_NOTICE,
			    "peer rejected protocol 0x%04x", output->u.proto);

			/* If fatal, shut down, else report to bundle */
			switch (output->u.proto) {
			case PPP_PROTO_LCP:
			case PPP_PROTO_CHAP:
			case PPP_PROTO_PAP:
			case PPP_PROTO_MP:
				ppp_fsm_input(link->lcp,
				    FSM_INPUT_RECD_PROTOREJ, output->u.proto);
				break;
			default:
				if (link->bundle != NULL) {
					ppp_bundle_protorej(link->bundle,
					    output->u.proto);
				}
				break;
			}
			break;
		    }
		case FSM_OUTPUT_DEAD:
			LOG(LOG_INFO, "LCP is dead: %s",
			    ppp_fsm_reason_str(output));
			if (link->bundle != NULL)
				ppp_link_unjoin(link);
			ppp_fsm_free_output(output);
			ppp_link_destroy(&link);
			return;
		}

		/* Free output */
		ppp_fsm_free_output(output);
	}
}

/***********************************************************************
			DEVICE EVENT HANDLER
***********************************************************************/

static void
ppp_link_device_event(void *arg)
{
	struct ppp_link *const link = arg;
	struct mesg_port *const outport = ppp_channel_get_outport(link->device);
	struct ppp_channel_output *output;

	/* Handle channel events */
	while ((output = mesg_port_get(outport, 0)) != NULL) {

		/* Check event */
		switch (output->type) {
		case PPP_CHANNEL_OUTPUT_UP:
			LOG(LOG_INFO, "device layer is up");
			ppp_fsm_input(link->lcp, FSM_INPUT_UP);
			link->device_up = 1;
			break;
		case PPP_CHANNEL_OUTPUT_DOWN_FATAL:
		case PPP_CHANNEL_OUTPUT_DOWN_NONFATAL:
			LOG(LOG_INFO, "device layer is down: %s", output->info);
			ppp_fsm_input(link->lcp,
			    output->type == PPP_CHANNEL_OUTPUT_DOWN_FATAL ?
			      FSM_INPUT_DOWN_FATAL : FSM_INPUT_DOWN_NONFATAL);
			link->device_up = 0;
			break;
		}

		/* Free channel output */
		ppp_channel_free_output(link->device, output);
	}
}

/***********************************************************************
			PPP NODE OUTPUT HANDLER
***********************************************************************/

/*
 * Handle data received from the node's bypass hook.
 */
void
ppp_link_recv_bypass(struct ppp_link *link,
	u_int16_t proto, u_char *data, size_t len)
{
	LOG(LOG_DEBUG + 1, "rec'd proto 0x%04x %u bytes", proto, len);
	switch (proto) {
	case PPP_PROTO_LCP:
		ppp_fsm_input(link->lcp, FSM_INPUT_DATA, data, len);
		break;
	case PPP_PROTO_PAP:
	case PPP_PROTO_CHAP:
	    {
		int i;

		for (i = 0; i < 2; i++) {
			struct ppp_link_auth *const auth = &link->auth[i];

			if (auth->type == NULL
			    || auth->arg == NULL
			    || auth->proto != proto)
				continue;
			(*auth->type->input)(auth->arg, i, data, len);
		}
		break;
	    }
	case PPP_PROTO_MP:
	case PPP_PROTO_IPCP:
	case PPP_PROTO_IP:
	case PPP_PROTO_VJCOMP:
	case PPP_PROTO_VJUNCOMP:
	case PPP_PROTO_CCP:
	case PPP_PROTO_COMPD:
		break;
	default:			/* send a protocol-reject */
		ppp_fsm_input(link->lcp,
		    FSM_INPUT_XMIT_PROTOREJ, proto, data, len);
		break;
	}
}

static void
ppp_link_node_recv(void *arg, u_int link_num,
	u_int16_t proto, u_char *data, size_t len)
{
	struct ppp_link *const link = arg;

	assert(link_num == 0);
	ppp_link_recv_bypass(link, proto, data, len);
}

/***********************************************************************
			AUTHORIZATION PHASE
***********************************************************************/

/*
 * Begin link authorization.
 */
static void
ppp_link_auth_start(struct ppp_link *link)
{
	int i;

	/* Get auth types negotiated by LCP */
	for (i = 0; i < 2; i++) {
		link->auth[i].type = (link->lcp_req.auth[i] != PPP_AUTH_NONE) ?
		    ppp_auth_by_index(link->lcp_req.auth[i]) : NULL;
	}
	LOG(LOG_DEBUG, "auth required: self=%s peer=%s",
	    link->auth[PPP_PEER].type != NULL ?
	      link->auth[PPP_PEER].type->name : "None",
	    link->auth[PPP_SELF].type != NULL ?
	      link->auth[PPP_SELF].type->name : "None");

	/* If no auth required, skip it */
	if (link->auth[PPP_SELF].type == NULL
	    && link->auth[PPP_PEER].type == NULL) {
		ppp_link_join(link);
		return;
	}

	/* Start auth timer */
	pevent_unregister(&link->auth_timer);
	if (pevent_register(link->ev_ctx, &link->auth_timer, 0,
	    link->mutex, ppp_link_auth_timeout, link, PEVENT_TIME,
	    AUTH_TIMEOUT * 1000) == -1) {
		LOG(LOG_ERR, "%s: %m", "pevent_register");
		ppp_link_close(link);
		return;
	}

	/* Start authorization in each direction */
	for (i = 0; i < 2; i++) {
		struct ppp_link_auth *const auth = &link->auth[i];

		if (auth->type == NULL)
			continue;
		if ((auth->arg = (*auth->type->start)(link->ev_ctx, link,
		    link->mutex, i, &auth->proto, ppp_log_dup(link->log)))
		    == NULL) {
			LOG(LOG_ERR, "failed to initialize authorization");
			ppp_link_close(link);
			return;
		}
	}
}

/*
 * Stop link authorization.
 */
static void
ppp_link_auth_stop(struct ppp_link *link)
{
	int i;

	/* Stop auth timer */
	pevent_unregister(&link->auth_timer);

	/* Stop auth in both directions */
	for (i = 0; i < 2; i++) {
		struct ppp_link_auth *const auth = &link->auth[i];

		/* Kill any threads */
		paction_cancel(&auth->action);

		/* Clean up authorization code */
		if (auth->arg != NULL) {
			(*auth->type->cancel)(auth->arg);
			auth->arg = NULL;
		}
	}
}

/*
 * Authorization failed to happen within the alloted time.
 */
static void
ppp_link_auth_timeout(void *arg)
{
	struct ppp_link *const link = arg;

	pevent_unregister(&link->auth_timer);
	LOG(LOG_ERR, "authorization timed out");
	ppp_link_close(link);
}

/***********************************************************************
			AUTHORIZATION CALLBACKS
***********************************************************************/

#define AUTHINFO_MTYPE		"ppp_link.authinfo"

/* Authorization acquire/check state */
struct ppp_link_auth_info {
	struct ppp_link			*link;	/* link being authorized */
	struct ppp_auth_cred		cred;	/* auth credentials */
	struct ppp_auth_resp		resp;	/* auth response */
	ppp_link_auth_finish_t		*finish;/* auth finish routine */
	int				rtn;	/* return value from user */
	int				error;	/* saved value of 'errno' */
};

static paction_handler_t	ppp_link_auth_acquire_main;
static paction_finish_t		ppp_link_auth_acquire_finish;

static paction_handler_t	ppp_link_auth_check_main;
static paction_finish_t		ppp_link_auth_check_finish;

/*
 * Acquire or check authorization credentials.
 *
 * This action is done in a separate thread.
 */
int
ppp_link_authorize(struct ppp_link *link, int dir,
	const struct ppp_auth_cred *cred, ppp_link_auth_finish_t *authfinish)
{
	struct ppp_link_auth *const auth = &link->auth[dir];
	struct ppp_link_auth_info *authinfo;
	paction_handler_t *handler;
	paction_finish_t *finish;

	/* Sanity check */
	assert(auth->action == NULL);

	/* Set up for 'acquire' or 'check' */
	if (dir == PPP_PEER) {
		if (link->conf.auth.meth->acquire == NULL) {
			LOG(LOG_ERR, "no method provided for %s credentials",
			    "acquiring");
			return (-1);
		}
		handler = ppp_link_auth_acquire_main;
		finish = ppp_link_auth_acquire_finish;
	} else {
		if (link->conf.auth.meth->check == NULL) {
			LOG(LOG_ERR, "no method provided for %s credentials",
			    "checking");
			return (-1);
		}
		handler = ppp_link_auth_check_main;
		finish = ppp_link_auth_check_finish;
	}

	/* Create auth info */
	if ((authinfo = MALLOC(AUTHINFO_MTYPE, sizeof(*authinfo))) == NULL) {
		LOG(LOG_ERR, "%s: %m", "malloc");
		return (-1);
	}
	memset(authinfo, 0, sizeof(*authinfo));
	authinfo->link = link;
	authinfo->cred = *cred;
	authinfo->finish = authfinish;

	/* Initiate authorization action */
	if (paction_start(&auth->action, link->mutex,
	    handler, finish, authinfo) == -1) {
		LOG(LOG_ERR, "%s: %m", "paction_start");
		FREE(AUTHINFO_MTYPE, authinfo);
		return (-1);
	}

	/* Done */
	return (0);
}

/*
 * Acquire authorization credentials.
 *
 * The mutex is NOT locked when this is called.
 */
static void
ppp_link_auth_acquire_main(void *arg)
{
	struct ppp_link_auth_info *const authinfo = arg;
	struct ppp_link *const link = authinfo->link;

	/* Acquire credentials */
	authinfo->rtn = (*link->conf.auth.meth->acquire)(link,
	    &authinfo->cred, &authinfo->resp);
	authinfo->error = errno;
}

/*
 * Finish acquiring authorization credentials.
 *
 * The mutex is locked when this is called unless 'canceled' is true.
 */
static void
ppp_link_auth_acquire_finish(void *arg, int canceled)
{
	struct ppp_link_auth_info *const authinfo = arg;
	struct ppp_link *const link = authinfo->link;
	struct ppp_link_auth *const auth = &link->auth[PPP_PEER];

	/* If canceled, just clean up */
	if (canceled)
		goto done;

	/* If acquiring credentials failed, bail out here */
	if (authinfo->rtn != 0) {
		if (*authinfo->resp.errmsg == '\0') {
			strlcpy(authinfo->resp.errmsg,
			    strerror(authinfo->error),
			    sizeof(authinfo->resp.errmsg));
		}
		LOG(LOG_WARNING, "failed to acquire credentials: %s",
		    auth->resp.errmsg);
		ppp_link_auth_complete(link, PPP_PEER, NULL, NULL);
		return;
	}

	/* Save a copy of credentials */
	auth->cred = authinfo->cred;
	auth->resp = authinfo->resp;

	/* Report credentials back to authorization code */
	(*authinfo->finish)(auth->arg, &authinfo->cred, &authinfo->resp);

done:
	/* Free authinfo */
	FREE(AUTHINFO_MTYPE, authinfo);
}

/*
 * Check authorization credentials.
 *
 * The mutex is NOT locked when this is called.
 */
static void
ppp_link_auth_check_main(void *arg)
{
	struct ppp_link_auth_info *const authinfo = arg;
	struct ppp_link *const link = authinfo->link;

	/* Check credentials */
	authinfo->rtn = (*link->conf.auth.meth->check)(link,
	    &authinfo->cred, &authinfo->resp);
	authinfo->error = errno;
}

/*
 * Finish checking authorization credentials.
 *
 * The mutex is locked when this is called.
 */
static void
ppp_link_auth_check_finish(void *arg, int canceled)
{
	struct ppp_link_auth_info *const authinfo = arg;
	struct ppp_link *const link = authinfo->link;
	struct ppp_link_auth *const auth = &link->auth[PPP_SELF];

	/* If canceled, just clean up */
	if (canceled)
		goto done;

	/* Save a copy of credentials */
	auth->cred = authinfo->cred;
	auth->resp = authinfo->resp;

	/* Fill in error message to indicate invalid credentials */
	if (authinfo->rtn != 0) {
		if (*authinfo->resp.errmsg == '\0') {
			strlcpy(authinfo->resp.errmsg,
			    strerror(authinfo->error),
			    sizeof(authinfo->resp.errmsg));
		}
	}

	/* Report result back to authorization code */
	(*authinfo->finish)(auth->arg, &authinfo->cred, &authinfo->resp);

done:
	/* Free authinfo */
	FREE(AUTHINFO_MTYPE, authinfo);
}

/* 
 * Determine if an authorization action is already in progress.
 */
int
ppp_link_auth_in_progress(struct ppp_link *link, int dir)
{
	struct ppp_link_auth *const auth = &link->auth[dir];

	return (auth->action != NULL);
}

/*
 * Finish link authorization (in one direction).
 *
 * A NULL 'cred' indicates failure.
 */
void
ppp_link_auth_complete(struct ppp_link *link, int dir,
	const struct ppp_auth_cred *cred, const union ppp_auth_mppe *mppe)
{
	struct ppp_link_auth *const auth = &link->auth[dir];

	/* Sanity check */
	assert(auth->arg != NULL);

	/* If auth failed, close link */
	if (cred == NULL) {
		LOG(LOG_NOTICE, "authorization %s peer failed",
		    dir == PPP_SELF ? "from" : "to");
		ppp_link_close(link);
		return;
	}

	/* Save credentials and MPPE info */
	auth->cred = *cred;
	if (mppe != NULL)
		auth->mppe = *mppe;

	/* Destroy auth object for this direction */
	(*auth->type->cancel)(auth->arg);
	auth->arg = NULL;

	/* If other direction still active, let it finish */
	if (link->auth[!dir].arg != NULL)
		return;

	/* Move to the 'UP' state */
	ppp_link_auth_stop(link);
	ppp_link_join(link);
}

/*
 * Get this link's authorization configuration.
 */
const struct ppp_auth_config *
ppp_link_auth_get_config(struct ppp_link *link)
{
	return (&link->conf.auth);
}

/*
 * Get this link's authorization type in one direction.
 */
const struct ppp_auth_type *
ppp_link_get_auth(struct ppp_link *link, int dir)
{
	dir &= 1;
	return (link->auth[dir].type);
}

/*
 * Get this link's log
 */
struct ppp_log *
ppp_link_get_log(struct ppp_link *link)
{
	return (link->log);
}

/***********************************************************************
			BUNDLE OPERATIONS
***********************************************************************/

/*
 * The link FSM has reached the OPENED state and authentication
 * was successful, so join a bundle.
 */
static void
ppp_link_join(struct ppp_link *link)
{
	assert(link->bundle == NULL);
	if ((link->bundle = ppp_engine_join(link->engine,
	    link, &link->node, &link->link_num)) == NULL) {
		ppp_link_close(link);
		return;
	}
	link->state = PPP_LINK_UP;
}

/*
 * Link has left the OPENED state, so leave bundle (if any).
 */
static void
ppp_link_unjoin(struct ppp_link *link)
{
	ppp_bundle_unjoin(&link->bundle, link);		/* ok if bundle null */
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

/*
 * Acquire an ng_ppp(4) node for this link to use.
 */
static int
ppp_link_get_node(struct ppp_link *link)
{
	const char *node;
	const char *hook;

	/* Sanity check */
	if (link->node != NULL)
		return (0);

	/* Get new node */
	if ((link->node = ppp_node_create(link->ev_ctx,
	    link->mutex, ppp_log_dup(link->log))) == NULL) {
		LOG(LOG_ERR, "%s: %m", "creating ppp node");
		return (-1);
	}

	/* Connect device to node */
	if ((node = ppp_channel_get_node(link->device)) == NULL
	    || (hook = ppp_channel_get_hook(link->device)) == NULL) {
		LOG(LOG_ERR, "channel is not a device");
		ppp_node_destroy(&link->node);
		return (-1);
	}
	if (ppp_node_connect(link->node, 0, node, hook) == -1) {
		LOG(LOG_ERR, "%s: %m", "connecting device to node");
		ppp_node_destroy(&link->node);
		return (-1);
	}

	/* Receive all node bypass packets */
	ppp_node_set_recv(link->node, ppp_link_node_recv, link);

	/* Done */
	return (0);
}

