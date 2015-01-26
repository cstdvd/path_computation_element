
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_COOKIEAUTH_H_
#define _PDEL_HTTP_SERVLET_COOKIEAUTH_H_

/*
 * Function that should return non-zero if access to
 * the supplied URL path requires a valid login cookie.
 */
typedef int	http_servlet_cookieauth_reqd_t(void *arg,
			struct http_request *req);

__BEGIN_DECLS

/*
 * Create a new cookieauth servlet that will redirect any requests
 * not having a valid cookie to the "redirect" URL (presumably a
 * login page); "append" functions as with http_servlet_redirect_create().
 *
 * "privkey" should point to the PEM-encoded RSA private key file.
 *
 * The opaque "id" must uniquely identify this server. Cookies created
 * with different id's (e.g., different machines) are incompatible.
 */
extern struct	http_servlet *http_servlet_cookieauth_create(
			const char *redirect, int append,
			http_servlet_cookieauth_reqd_t *authreqd,
			void *arg, void (*destroy)(void *),
			const char *privkey, const void *id, size_t idlen,
			const char *cookiename);

/*
 * Add a cookie that will cause the servlet to not redirect.
 * That is, make the browser appear logged in as "username".
 *
 * The servlet returned by http_servlet_cookieauth_create() must be
 * supplied.
 *
 * The cookie remains valid until time "expire". If "session_only"
 * is set, the browser is instructed to delete the cookie when it
 * exits (though from a security point of view you can't trust the
 * browser to actually do that; use "expire" as a backup).
 *
 * If "linger" is non zero, force a re-login if the last time the servlet
 * was run was longer than "linger" seconds ago.
 *
 * "path" and "domain" may be NULL to omit (i.e., leave as default).
 *
 * Returns zero if success, -1 if error.
 */
extern int	http_servlet_cookieauth_login(struct http_response *resp,
			const char *privkey, const char *username,
			u_int max_linger, time_t expire, int session_only,
			const u_char *id, size_t idlen, const char *cookiename,
			const char *path, const char *domain, int secure);

/*
 * Invalidate authorization cookie.
 */
extern int	http_servlet_cookieauth_logout(const char *cookiename,
			const char *path, const char *domain,
			struct http_response *resp);

/*
 * Get the username from the authorization cookie.
 *
 * Returns the username, or NULL if not logged in (EACCES) or error.
 */
extern char	*http_servlet_cookieauth_user(const char *privkey,
			const void *id, size_t idlen, const char *cookiename,
			struct http_request *req, const char *mtype);

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_COOKIEAUTH_H_ */
