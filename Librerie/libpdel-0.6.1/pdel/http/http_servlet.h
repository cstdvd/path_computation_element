
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_HTTP_SERVLET_H_
#define _PDEL_HTTP_HTTP_SERVLET_H_

/*
 * HTTP servlets
 */

struct http_request;
struct http_response;
struct http_servlet;
struct http_servlet_hook;

typedef struct http_servlet		*http_servlet_h;
typedef struct http_servlet_hook	*http_servlet_hook_h;

/*
 * Run method
 *
 * Should return "1" if further servlets should be tried, else "0" to
 * stop and send the output to the peer.
 *
 * If the servlet requires single threading, it must handle that itself.
 */
typedef	int	http_servlet_run_t(struct http_servlet *servlet,
			struct http_request *req, struct http_response *resp);

/*
 * Destroy method
 *
 * Cleanup and free a servlet structure. It is guaranteed that no threads
 * will execute in this servlet's "run" method when this is called.
 */
typedef	void	http_servlet_destroy_t(struct http_servlet *servlet);

/*
 * Servlet structure
 */
typedef struct http_servlet {
	void 				*arg;		/* servlet cookie */
	struct http_servlet_hook	*hook;		/* server info */
	http_servlet_run_t		*run;		/* execute method */
	http_servlet_destroy_t		*destroy;	/* destructor */
} http_servlet;

#endif	/* _PDEL_HTTP_HTTP_SERVLET_H_ */

