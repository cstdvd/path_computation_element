
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_node.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_lcp.h"
#include "ppp/ppp_link.h"
#include "ppp/ppp_bundle.h"
#include "ppp/ppp_manager.h"
#include "ppp/ppp_engine.h"
#include "ppp/ppp_pptp_server.h"
#include "ppp/ppp_l2tp_server.h"

#define ENGINE_MTYPE		"ppp_engine"
#define ENGINE_HASH_LOAD	200

struct ppp_engine {
	struct ppp_manager	*manager;	/* ppp manager object */
	struct pevent_ctx	*ev_ctx;	/* event context */
	pthread_mutex_t		mutex;		/* mutex for context */
	struct ppp_log		*log;		/* log */
	struct ghash		*bundles;	/* bundles, each >= 1 link */
	struct ghash		*links;		/* "floating" unbundled links */
	void			*pptp_server;	/* pptp server */
	void			*l2tp_server;	/* l2tp server */
};

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(engine->log, sev, fmt , ## args)

/*
 * Create a new PPP engine.
 *
 * The "log" is destroyed when the engine is destroyed.
 */
struct ppp_engine *
ppp_engine_create(struct ppp_manager *manager,
	const pthread_attr_t *attr, struct ppp_log *log)
{
	struct ppp_engine *engine;
	pthread_mutexattr_t mattr;
	int got_mutex = 0;
	int got_mattr = 0;

	/* Create engine object */
	if ((engine = MALLOC(ENGINE_MTYPE, sizeof(*engine))) == NULL)
		return (NULL);
	memset(engine, 0, sizeof(*engine));
	engine->manager = manager;
	engine->log = log;

	/* Create hash tables */
	if ((engine->links = ghash_create(engine, 0, ENGINE_HASH_LOAD,
	    ENGINE_MTYPE, NULL, NULL, NULL, NULL)) == NULL)
		goto fail;
	if ((engine->bundles = ghash_create(engine, 0, ENGINE_HASH_LOAD,
	    ENGINE_MTYPE, NULL, NULL, NULL, NULL)) == NULL)
		goto fail;

	/* Initialize mutex */
	if ((errno = pthread_mutexattr_init(&mattr)) != 0)
		goto fail;
	got_mattr = 1;
	if ((errno = pthread_mutexattr_settype(&mattr,
	    PTHREAD_MUTEX_RECURSIVE)) != 0)
		goto fail;
	if ((errno = pthread_mutex_init(&engine->mutex, &mattr)) != 0)
		goto fail;
	got_mutex = 1;
	pthread_mutexattr_destroy(&mattr);
	got_mattr = 0;

	/* Create event context */
	if ((engine->ev_ctx = pevent_ctx_create("ppp_engine", attr)) == NULL)
		goto fail;

	/* Done */
	return (engine);

fail:
	/* Clean up after failure */
	ghash_destroy(&engine->bundles);
	ghash_destroy(&engine->links);
	if (got_mutex)
		pthread_mutex_destroy(&engine->mutex);
	if (got_mattr)
		pthread_mutexattr_destroy(&mattr);
	FREE(ENGINE_MTYPE, engine);
	return (NULL);
}

/*
 * Destroy a PPP engine.
 *
 * If "wait" is true, wait for all associated threads to exit.
 */
void
ppp_engine_destroy(struct ppp_engine **enginep, int wait)
{
	struct ppp_engine *const engine = *enginep;
	struct ppp_bundle **bundles;
	struct ppp_link **links;
	int num;
	int i;

	/* Sanity check */
	if (engine == NULL)
		return;
	*enginep = NULL;

	/* Stop PPTP and L2TP servers (if any) */
	ppp_pptp_server_stop(engine);
	ppp_l2tp_server_stop(engine);

	/* Destroy floating links */
	if ((num = ghash_dump(engine->links,
	    (void ***)&links, TYPED_MEM_TEMP)) == -1)
		LOG(LOG_ERR, "%s: %m", "ghash_dump");
	else {
		for (i = 0; i < num; i++)
			ppp_link_destroy(&links[i]);
		FREE(TYPED_MEM_TEMP, links);
	}

	/* Destroy bundles */
	if ((num = ghash_dump(engine->bundles,
	    (void ***)&bundles, TYPED_MEM_TEMP)) == -1)
		LOG(LOG_ERR, "%s: %m", "ghash_dump");
	else {
		for (i = 0; i < num; i++)
			ppp_bundle_destroy(&bundles[i]);
		FREE(TYPED_MEM_TEMP, bundles);
	}

	/* Destroy event context; there should be no remaining events */
	if ((num = pevent_ctx_count(engine->ev_ctx)) != 0) {
		LOG(LOG_ERR, "%s: %d events remain at shutdown",
		    __FUNCTION__, num);
	}
	pevent_ctx_destroy(&engine->ev_ctx);

	/* Free engine object */
	ghash_destroy(&engine->links);
	ghash_destroy(&engine->bundles);
	ppp_log_close(&engine->log);
	FREE(ENGINE_MTYPE, engine);
}

/*
 * Get a list of all known bundles.
 */
int
ppp_engine_get_bundles(struct ppp_engine *engine,
	struct ppp_bundle ***listp, const char *mtype)
{
	return (ghash_dump(engine->bundles, (void ***)listp, mtype));
}

/*
 * Add a newly created (and therefore "floating") link.
 */
int
ppp_engine_add_link(struct ppp_engine *engine, struct ppp_link *link)
{
	return (ghash_put(engine->links, link));
}

/*
 * Remove a "floating" link from the floating link hash table.
 */
void
ppp_engine_del_link(struct ppp_engine *engine, struct ppp_link *link)
{
	ghash_remove(engine->links, link);
}

/*
 * Add a bundle.
 */
int
ppp_engine_add_bundle(struct ppp_engine *engine, struct ppp_bundle *bundle)
{
	return (ghash_put(engine->bundles, bundle));
}

/*
 * Remove a bundle.
 */
void
ppp_engine_del_bundle(struct ppp_engine *engine, struct ppp_bundle *bundle)
{
	ghash_remove(engine->bundles, bundle);
}

/*
 * An "unbundled" link has reached the OPENED state and authenticated.
 * Add it to an existing bundle or create a new bundle as appropriate.
 */
struct ppp_bundle *
ppp_engine_join(struct ppp_engine *engine, struct ppp_link *link,
	struct ppp_node **nodep, u_int16_t *link_num)
{
	struct ppp_bundle *bundle = NULL;
	struct ppp_lcp_req lcp_req;
	struct ghash_walk walk;

	/* Sanity */
	assert(*nodep != NULL);

	/* If link does not have multilink, can't join any bundles */
	ppp_link_get_lcp_req(link, &lcp_req);
	if (!lcp_req.multilink[PPP_SELF] || !lcp_req.multilink[PPP_PEER])
		goto no_join;

	/* See if link matches any existing bundle */
	ghash_walk_init(engine->bundles, &walk);
	while ((bundle = ghash_walk_next(engine->bundles, &walk)) != NULL) {
		const char *link_authname[2];
		const char *bund_authname[2];
		struct ppp_eid link_eid[2];
		struct ppp_eid bund_eid[2];
		int j;

		/* If bundle does not have multilink, can't join it */
		if (!ppp_bundle_get_multilink(bundle))
			continue;

		/* Get info about link and bundle */
		for (j = 0; j < 2; j++) {
			link_authname[j] = ppp_link_get_authname(link, j);
			ppp_link_get_eid(link, j, &link_eid[j]);
			bund_authname[j] = ppp_bundle_get_authname(bundle, j);
			ppp_bundle_get_eid(bundle, j, &bund_eid[j]);
		}

		/* Compare them */
		for (j = 0; j < 2; j++) {
			if (strcmp(link_authname[j], bund_authname[j]) != 0)
				break;
			if (link_eid[j].class != bund_eid[j].class)
				break;
			if (link_eid[j].length != bund_eid[j].length)
				break;
			if (memcmp(link_eid[j].value,
			    bund_eid[j].value, link_eid[j].length) != 0)
				break;
		}

		/* If equal, stop */
		if (j == 2)
			break;
	}

no_join:
	/* If no matching bundle found, create a new one */
	if (bundle == NULL) {

		/* Create new bundle */
		if ((bundle = ppp_bundle_create(engine,
		    link, *nodep)) == NULL) {
			PPP_LOG(ppp_link_get_log(link), LOG_ERR,
			    "failed to create new bundle: %m");
			return (NULL);
		}

		/* The bundle steals the link's node */
		*nodep = NULL;
		*link_num = 0;
	} else {

		/* Join link into bundle */
		if (ppp_bundle_join(bundle, link, *nodep, link_num) == -1) {
			PPP_LOG(ppp_link_get_log(link), LOG_ERR,
			    "link failed to join bundle: %m");
			return (NULL);
		}

		/* Destroy link's node: it's no longer needed */
		ppp_node_destroy(nodep);
	}

	/* Link is no longer 'floating' as bundle now references it */
	ppp_engine_del_link(engine, link);

	/* Done */
	return (bundle);
}

void
ppp_engine_set_pptp_server(struct ppp_engine *engine, void *s)
{
	engine->pptp_server = s;
}

void *
ppp_engine_get_pptp_server(struct ppp_engine *engine)
{
	return (engine->pptp_server);
}

void
ppp_engine_set_l2tp_server(struct ppp_engine *engine, void *s)
{
	engine->l2tp_server = s;
}

void *
ppp_engine_get_l2tp_server(struct ppp_engine *engine)
{
	return (engine->l2tp_server);
}

struct pevent_ctx *
ppp_engine_get_ev_ctx(struct ppp_engine *engine)
{
	return (engine->ev_ctx);
}

pthread_mutex_t	*
ppp_engine_get_mutex(struct ppp_engine *engine)
{
	return (&engine->mutex);
}

struct ppp_log *
ppp_engine_get_log(struct ppp_engine *engine)
{
	return (engine->log);
}

/********************************************************************
		    MANAGER CALL-THROUGH FUNCTIONS
********************************************************************/

/*
 * Configure a bundle.
 */
void *
ppp_engine_bundle_config(struct ppp_engine *engine,
	struct ppp_link *link, struct ppp_bundle_config *conf)
{
	return (ppp_manager_bundle_config(engine->manager, link, conf));
}

/*
 * Plumb 'top' side of netgraph node.
 */
void *
ppp_engine_bundle_plumb(struct ppp_engine *engine,
	struct ppp_bundle *bundle, const char *path, const char *hook,
	struct in_addr *ips, struct in_addr *dns, struct in_addr *nbns,
	u_int mtu)
{
	return (ppp_manager_bundle_plumb(engine->manager, bundle,
	    path, hook, ips, dns, nbns, mtu));
}

/*
 * Disconnect 'top' side of netgraph node.
 */
void
ppp_engine_bundle_unplumb(struct ppp_engine *engine, void *arg,
	struct ppp_bundle *bundle)
{
	ppp_manager_bundle_unplumb(engine->manager, arg, bundle);
}

/*
 * Release an IP address for a peer.
 */
void
ppp_engine_release_ip(struct ppp_engine *engine,
	struct ppp_bundle *bundle, struct in_addr ip)
{
	ppp_manager_release_ip(engine->manager, bundle, ip);
}

