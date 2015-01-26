
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_REDIRECT_H_
#define _PDEL_HTTP_SERVLET_REDIRECT_H_

/* Whether and how to append the original URL as query string */
#define HTTP_SERVLET_REDIRECT_NO_APPEND		0	/* no append */
#define HTTP_SERVLET_REDIRECT_APPEND_URL	1	/* append URL as qs */
#define HTTP_SERVLET_REDIRECT_APPEND_URI	2	/* preserve URI */
#define HTTP_SERVLET_REDIRECT_APPEND_QUERY	3	/* append qs */

__BEGIN_DECLS

/* Functions */
extern struct	http_servlet *http_servlet_redirect_create(const char *url,
			int append_url);

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_REDIRECT_H_ */
