
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_BUNDLE_H_
#define _PDEL_PPP_PPP_BUNDLE_H_

/************************************************************************
			PUBLIC DECLARATIONS
************************************************************************/

struct ppp_eid;
struct ppp_node;
struct ppp_link;
struct ppp_bundle;
struct ppp_ipcp_req;
struct ppp_ccp_req;

#define PPP_BUNDLE_MAX_LOGNAME	32

/*
 * Bundle configuration structure
 *
 * If ip[PPP_SELF] is 0.0.0.0, we will accept whatever IP address the
 * peer requests for itself.  If the peer requests 0.0.0.0 and
 * ip[PPP_PEER] is 0.0.0.0, we close the bundle.
 */
struct ppp_bundle_config {
	struct in_addr		ip[2];		/* ip addrs, or 0.0.0.0 */
	struct in_addr		dns_servers[2];	/* dns servers for peer */
	struct in_addr		nbns_servers[2];/* nbns servers for peer */
	u_char			vjc;		/* enable vj compression */
	u_char			mppe_40;	/* enable 40 bit mppe */
	u_char			mppe_56;	/* enable 56 bit mppe */
	u_char			mppe_128;	/* enable 128 bit mppe */
	u_char			mppe_reqd;	/* ms encryption req'd */
	u_char			mppe_stateless;	/* stateless encryption */
	char			logname[PPP_BUNDLE_MAX_LOGNAME];
};

/************************************************************************
			PUBLIC FUNCTIONS
************************************************************************/

__BEGIN_DECLS

extern void	ppp_bundle_close(struct ppp_bundle *bundle);
extern void	ppp_bundle_destroy(struct ppp_bundle **bundlep);
extern const	char *ppp_bundle_get_authname(struct ppp_bundle *bundle,
			int dir);
extern void	ppp_bundle_get_eid(struct ppp_bundle *bundle,
			int dir, struct ppp_eid *eid);
extern int	ppp_bundle_get_multilink(struct ppp_bundle *bundle);
extern int	ppp_bundle_get_ipcp(struct ppp_bundle *bundle,
			struct ppp_ipcp_req *ipcp, int *is_up);
extern int	ppp_bundle_get_ccp(struct ppp_bundle *bundle,
			struct ppp_ccp_req *ccp, int *is_up);
extern int	ppp_bundle_get_links(struct ppp_bundle *bundle,
			struct ppp_link **list, int max);
extern void	*ppp_bundle_get_cookie(struct ppp_bundle *bundle);

#ifdef _PDEL_PPP_PRIVATE_H_

/************************************************************************
			PRIVATE FUNCTIONS
************************************************************************/

extern struct	ppp_bundle *ppp_bundle_create(struct ppp_engine *engine,
			struct ppp_link *link, struct ppp_node *node);
extern int	ppp_bundle_join(struct ppp_bundle *bundle,
			struct ppp_link *link, struct ppp_node *node,
			u_int16_t *link_num);
extern void	ppp_bundle_unjoin(struct ppp_bundle **bundlep,
                        struct ppp_link *link);
extern void	ppp_bundle_protorej(struct ppp_bundle *bundle, u_int16_t proto);
extern int	ppp_bundle_write(struct ppp_bundle *bundle, u_int link_num,
			u_int16_t proto, const void *data, size_t len);

#endif	/* _PDEL_PPP_PRIVATE_H_ */

__END_DECLS

#endif	/* _PDEL_PPP_PPP_BUNDLE_H_ */
