
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_ENGINE_H_
#define _PDEL_PPP_PPP_ENGINE_H_

struct ppp_engine;
struct ppp_manager;
struct ppp_bundle;
struct ppp_link;
struct ppp_node;
struct ppp_link_config;
struct ppp_bundle_config;
struct ppp_log;

__BEGIN_DECLS

/********************************************************************
			PUBLIC FUNCTIONS
********************************************************************/

extern struct	ppp_engine *ppp_engine_create(struct ppp_manager *manager,
			const pthread_attr_t *attr, struct ppp_log *log);
extern void	ppp_engine_destroy(struct ppp_engine **enginep, int wait);

extern int	ppp_engine_get_bundles(struct ppp_engine *engine,
			struct ppp_bundle ***listp, const char *mtype);

/********************************************************************
			PRIVATE FUNCTIONS
********************************************************************/

#ifdef _PDEL_PPP_PRIVATE_H_

extern int	ppp_engine_add_link(struct ppp_engine *engine,
			struct ppp_link *link);
extern void	ppp_engine_del_link(struct ppp_engine *engine,
			struct ppp_link *link);
extern struct	ppp_bundle *ppp_engine_join(struct ppp_engine *engine,
			struct ppp_link *link, struct ppp_node **nodep,
			u_int16_t *link_num);
extern int	ppp_engine_add_bundle(struct ppp_engine *engine,
			struct ppp_bundle *bundle);
extern void	ppp_engine_del_bundle(struct ppp_engine *engine,
                        struct ppp_bundle *bundle);
extern void	ppp_engine_set_pptp_server(struct ppp_engine *engine, void *s);
extern void	*ppp_engine_get_pptp_server(struct ppp_engine *engine);
extern void	ppp_engine_set_l2tp_server(struct ppp_engine *engine, void *s);
extern void	*ppp_engine_get_l2tp_server(struct ppp_engine *engine);
struct ppp_log	*ppp_engine_get_log(struct ppp_engine *engine);
extern struct	pevent_ctx *ppp_engine_get_ev_ctx(struct ppp_engine *engine);
extern pthread_mutex_t	*ppp_engine_get_mutex(struct ppp_engine *engine);

/* Functions that call through to the manager */
extern void	*ppp_engine_bundle_config(struct ppp_engine *engine,
			struct ppp_link *link, struct ppp_bundle_config *conf);
extern void	*ppp_engine_bundle_plumb(struct ppp_engine *engine,
			struct ppp_bundle *bundle,
			const char *path, const char *hook,
			struct in_addr *ips, struct in_addr *dns,
			struct in_addr *nbns, u_int mtu);
extern void	ppp_engine_bundle_unplumb(struct ppp_engine *engine,
			void *arg, struct ppp_bundle *bundle);
extern void	ppp_engine_release_ip(struct ppp_engine *engine,
			struct ppp_bundle *bundle, struct in_addr ip);

__END_DECLS

#endif	/* _PDEL_PPP_PRIVATE_H_ */

#endif	/* _PDEL_PPP_PPP_ENGINE_H_ */
