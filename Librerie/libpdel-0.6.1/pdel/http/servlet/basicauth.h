
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_AUTH_H_
#define _PDEL_HTTP_SERVLET_AUTH_H_

/*
 * Authorization checker: returns NULL if allowed, otherwise realm.
 *
 * A pointer to such a function is the info required by this servlet.
 */
typedef const	char *http_servlet_basicauth_t(void *arg,
			struct http_request *req, const char *username,
			const char *password);

__BEGIN_DECLS

/* Functions */
extern struct	http_servlet *http_servlet_basicauth_create(
			http_servlet_basicauth_t *auth, void *arg,
			void (*destroy)(void *));

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_AUTH_H_ */
