
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_MSOFT_H_
#define _PDEL_PPP_PPP_MSOFT_H_

__BEGIN_DECLS

extern void	ppp_msoft_nt_challenge_response(const u_char *chal,
			const char *password, u_char *hash);

extern void	ppp_msoft_nt_password_hash(const char *password, u_char *hash);
extern void	ppp_msoft_lm_password_hash(const char *password, u_char *hash);

extern void	ppp_msoft_generate_nt_response(const u_char *authchal,
			const u_char *peerchal, const char *username,
			const char *password, u_char *hash);
extern void	ppp_msoft_generate_authenticator_response(const char *password,
			const u_char *ntresp, const u_char *peerchal,
			const u_char *authchal, const char *username,
			u_char *authresp);
extern int	ppp_msoft_check_authenticator_response(const char *password,
			const u_char *ntresp, const u_char *peerchal,
			const u_char *authchal, const char *username,
			const char *rechash);

extern void	ppp_msoft_get_key(const u_char *h, u_char *h2, int len);
extern void	ppp_msoft_get_start_key(const u_char *chal, u_char *h);

extern void	ppp_msoft_get_master_key(const u_char *resp, u_char *h);
extern void	ppp_msoft_get_asymetric_start_key(int which,
			const u_char *master, u_char *key);

extern void	ppp_msoft_init_key_v1(int e128, const char *pass,
			const u_char *chal, u_char *key);
extern void	ppp_msoft_init_key_v2(int which, const char *pass,
			const u_char *resp, u_char *key);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_MSOFT_H_ */
