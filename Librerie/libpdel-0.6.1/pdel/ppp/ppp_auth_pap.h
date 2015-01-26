
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_AUTH_PAP_H_
#define _PDEL_PPP_PPP_AUTH_PAP_H_

#ifndef _PDEL_PPP_PRIVATE_H_
#error "This header is only for use by the ppp library."
#endif

__BEGIN_DECLS

/* Auth type methods */
extern ppp_authtype_start_t	ppp_auth_pap_start;
extern ppp_authtype_cancel_t	ppp_auth_pap_cancel;
extern ppp_authtype_input_t	ppp_auth_pap_input;

__END_DECLS

#endif	/* _PDEL_PPP_PPP_AUTH_PAP_H_ */
