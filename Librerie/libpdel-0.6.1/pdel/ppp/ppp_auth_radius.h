
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_AUTH_RADIUS_H_
#define _PDEL_PPP_AUTH_RADIUS_H_

/*
 * Support for PPP authentication via RADIUS.
 */

/************************************************************************
			PUBLIC DECLARATIONS
************************************************************************/

struct ppp_auth_cred;
struct ppp_auth_resp;

/* Attributes returned by server (or NULL if not present in server reply) */
struct ppp_auth_radius_info {
	struct in_addr	*ip;
	struct in_addr	*netmask;
	char		*filter_id;
	u_int32_t	*session_timeout;
	u_int32_t	*mtu;
	u_int32_t	*routing;
	u_int32_t	*vjc;
	char		**routes;		/* NULL terminated list */
	char		*reply_message;
	char		*mschap_error;
	char		*mschap2_success;
	u_int32_t	*mppe_policy;
	u_int32_t	*mppe_types;
};

__BEGIN_DECLS

/*
 * Perform RADIUS authentication using the supplied 'struct rad_handle',
 * which must already be configured (see libradius(3)).
 *
 * 'log' may be NULL, otherwise it's used for logging.
 *
 * If 'rip' is non-NULL, then all fields in *rip will be initialized to NULL
 * and then filled in with whichever attributes are returned by the RADIUS
 * server. The fields in 'rip' should eventually be free'd by calling the
 * ppp_auth_radius_info_reset() function. Note: 'rip' fields are filled in
 * even if the server rejects authentication.
 *
 * This function is async cancel-safe.
 *
 * Returns:
 *	 0	Credentials are valid
 *	-1	Credentials can't be validated; resp->errmsg contains message
 *		and 'rip' has not been modified.
 */
extern int	ppp_auth_radius_check(struct rad_handle *rad,
			struct ppp_log *log, const struct ppp_auth_cred *creds,
			struct ppp_auth_resp *resp,
			struct ppp_auth_radius_info *rip);

/*
 * Free memory allocated for fields in a 'struct ppp_auth_radius_info'
 * and set them all to NULL.
 */
extern void	ppp_auth_radius_info_reset(struct ppp_auth_radius_info *rip);

__END_DECLS

#endif	/* _PDEL_PPP_AUTH_RADIUS_H_ */
