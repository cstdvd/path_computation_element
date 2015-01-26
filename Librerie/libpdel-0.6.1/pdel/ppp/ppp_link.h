
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_LINK_H_
#define _PDEL_PPP_PPP_LINK_H_

/************************************************************************
			PUBLIC DECLARATIONS
************************************************************************/

struct ppp_link;
struct ppp_bundle;
struct ppp_engine;
struct ppp_auth_config;
struct ppp_lcp_req;
struct ppp_channel;

/* Link states */
enum ppp_link_state {
	PPP_LINK_DOWN = 0,
	PPP_LINK_AUTH,
	PPP_LINK_UP,
};

/*
 * PPP link configuration
 */
struct ppp_link_config {
	struct ppp_auth_config	auth;		/* auth config */
	u_int16_t		min_peer_mru;	/* min peer mru */
	u_int16_t		max_self_mru;	/* max self mru */
	u_int16_t		min_peer_mrru;	/* min peer mrru */
	u_int16_t		max_self_mrru;	/* max self mrru */
	u_char			multilink;	/* enable multilink */
	struct ppp_eid		eid;		/* endpoint discriminator */
};

__BEGIN_DECLS

/* Functions */
extern int	ppp_link_create(struct ppp_engine *engine,
			struct ppp_channel *device,
			struct ppp_link_config *conf, struct ppp_log *log);
extern void	ppp_link_destroy(struct ppp_link **linkp);
extern void	ppp_link_close(struct ppp_link *link);
extern const	char *ppp_link_get_authname(struct ppp_link *link, int dir);
extern void	ppp_link_get_eid(struct ppp_link *link,
			int dir, struct ppp_eid *eid);
extern struct	ppp_channel *ppp_link_get_device(struct ppp_link *link);
extern int	ppp_link_get_origination(struct ppp_link *link);
extern enum	ppp_link_state ppp_link_get_state(struct ppp_link *link);
extern struct	ppp_bundle *ppp_link_get_bundle(struct ppp_link *link);
extern void	ppp_link_get_lcp_req(struct ppp_link *link,
			struct ppp_lcp_req *req);

__END_DECLS

/************************************************************************
			PRIVATE DECLARATIONS
************************************************************************/

#ifdef _PDEL_PPP_PRIVATE_H_

struct ppp_auth_cred;

typedef void	ppp_link_auth_finish_t(void *arg,
			const struct ppp_auth_cred *creds,
			const struct ppp_auth_resp *resp);

__BEGIN_DECLS

extern void	ppp_link_get_mppe(struct ppp_link *link, int dir,
			union ppp_auth_mppe *mppe);
extern void	ppp_link_write(struct ppp_link *link,
			u_int16_t proto, const void *data, size_t len);
extern const	struct ppp_auth_config *ppp_link_auth_get_config(
			struct ppp_link *link);
extern const	struct ppp_auth_type *ppp_link_get_auth(
			struct ppp_link *link, int dir);
extern int	ppp_link_authorize(struct ppp_link *link, int dir,
			const struct ppp_auth_cred *cred,
			ppp_link_auth_finish_t *finish);
extern int	ppp_link_auth_in_progress(struct ppp_link *link, int dir);
extern void	ppp_link_auth_complete(struct ppp_link *link, int dir,
			const struct ppp_auth_cred *cred,
			const union ppp_auth_mppe *mppe);
extern void	ppp_link_recv_bypass(struct ppp_link *link,
			u_int16_t proto, u_char *data, size_t len);
extern struct	ppp_log *ppp_link_get_log(struct ppp_link *link);

__END_DECLS

#endif	/* _PDEL_PPP_PRIVATE_H_ */

#endif	/* _PDEL_PPP_PPP_LINK_H_ */
