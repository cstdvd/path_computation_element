
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_MANAGER_H_
#define _PDEL_PPP_PPP_MANAGER_H_

/*
 * This is an interface that the PPP library expects to be
 * implemented by the code using the library.
 */

struct ppp_manager;
struct ppp_bundle;
struct ppp_link;
struct ppp_bundle_config;

/********************************************************************
			MANAGER METHODS
********************************************************************/

/*
 * A new bundle is being created for a new link. The link didn't match
 * any existing bundles. The PPP engine needs to know the configuration
 * for the new bundle.
 *
 * The link has already reached the OPENED state and authenticated,
 * so the local and remote authentication names, etc. are available.
 *
 * This function will be called in a thread that may be canceled at
 * any cancellation point. It should be prepared to clean up if so.
 *
 * If successful, the IP address contained in conf->ip[PPP_PEER] is
 * compared to 0.0.0.0. If not equal, then it is guaranteed that
 * the 'release_ip' function will be called exactly once in the future
 * with the with the same bundle and IP address.
 *
 * This method should return NULL on error, otherwise it should return
 * a non-NULL cookie which will be available from ppp_bundle_get_cookie().
 */
typedef void	*ppp_manager_bundle_config_t(struct ppp_manager *manager,
			struct ppp_link *link, struct ppp_bundle_config *conf);

/*
 * A bundle that was successfully configured by the 'bundle_config' callback
 * has reached the IPCP OPENED state and the IP traffic netgraph hook
 * needs to be plumbed. If successful, this method should return a non-NULL
 * cookie, and then the 'bundle_unplumb' method is guaranteed to be called
 * exactly once.
 *
 * 'ips' points to the local and remote IP address negotiated for the link.
 * 'dns' points to the 2 peer-supplied DNS IP addresses (or 0.0.0.0).
 * 'nbns' points to the 2 peer-supplied NBNS IP addresses (or 0.0.0.0).
 * 'mtu' is the MTU that should be configured for the interface
 */
typedef void	*ppp_manager_bundle_plumb_t(struct ppp_manager *manager,
			struct ppp_bundle *bundle,
			const char *path, const char *hook,
			struct in_addr *ips, struct in_addr *dns,
			struct in_addr *nbns, u_int mtu);

/*
 * A bundle that was successfully plumbed by the 'bundle_plumb' callback
 * is now being shutdown. The 'arg' parameter was returned by the
 * 'bundle_plumb' callback.
 *
 * Note: calling 'ppp_engine_destroy' can cause this method to be called.
 */
typedef void	ppp_manager_bundle_unplumb_t(struct ppp_manager *manager,
			void *arg, struct ppp_bundle *bundle);

/*
 * Callback to release an IP address assigned to the remote peer for a bundle.
 *
 * Note: calling 'ppp_engine_destroy' can cause this method to be called.
 */
typedef void	ppp_manager_release_ip_t(struct ppp_manager *manager,
			struct ppp_bundle *bundle, struct in_addr ip);

/* PPP manager methods */
struct ppp_manager_meth {
	ppp_manager_bundle_config_t	*bundle_config;
	ppp_manager_bundle_plumb_t	*bundle_plumb;
	ppp_manager_bundle_unplumb_t	*bundle_unplumb;
	ppp_manager_release_ip_t	*release_ip;
};

/* PPP manager structure */
struct ppp_manager {
	struct ppp_manager_meth	*meth;
	void			*priv;
};

__BEGIN_DECLS

/* Functions */
extern ppp_manager_bundle_config_t	ppp_manager_bundle_config;
extern ppp_manager_bundle_plumb_t	ppp_manager_bundle_plumb;
extern ppp_manager_bundle_unplumb_t	ppp_manager_bundle_unplumb;
extern ppp_manager_release_ip_t		ppp_manager_release_ip;

__END_DECLS

#endif	/* _PDEL_PPP_PPP_MANAGER_H_ */
