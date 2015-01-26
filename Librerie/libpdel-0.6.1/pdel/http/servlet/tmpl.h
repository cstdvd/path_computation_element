
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_TMPL_H_
#define _PDEL_HTTP_SERVLET_TMPL_H_

/* Info required for this servlet */

typedef void	http_servlet_tmpl_free_t(void *arg);

struct http_servlet_tmpl_tinfo {
	int			flags;		/* flags for tmpl_execute() */
	const char		*mtype;		/* tmpl string memory type */
	tmpl_handler_t		*handler;	/* tmpl user function handler */
	tmpl_errfmtr_t		*errfmtr;	/* tmpl error formatter */
	void			*arg;		/* opaque argument */
	http_servlet_tmpl_free_t *freer;	/* destructor for 'arg' */
};

struct http_servlet_tmpl_info {
	const char		*path;		/* pathname of template file */
	const char		*mime_type;	/* default output mime type */
	const char		*mime_encoding;	/* default output mime enc. */
	http_logger_t		*logger;	/* http error logger */
	struct http_servlet_tmpl_tinfo tinfo;	/* template info */
};

/* Argument that gets passed to template function handler */
struct http_servlet_tmpl_arg {
	void				*arg;	/* arg from 'tinfo' */
	struct http_request		*req;	/* http request */
	struct http_response		*resp;	/* http response */
};

__BEGIN_DECLS

/*
 * Create a new template servlet.
 */
extern struct	http_servlet *http_servlet_tmpl_create(
			struct http_servlet_tmpl_info *info);

/*
 * Built-in template user functions.
 */
extern tmpl_handler_t	http_servlet_tmpl_func_query;
extern tmpl_handler_t	http_servlet_tmpl_func_query_exists;
extern tmpl_handler_t	http_servlet_tmpl_func_query_string;
extern tmpl_handler_t	http_servlet_tmpl_func_get_header;
extern tmpl_handler_t	http_servlet_tmpl_func_set_header;
extern tmpl_handler_t	http_servlet_tmpl_func_remove_header;
extern tmpl_handler_t	http_servlet_tmpl_func_redirect;
extern tmpl_handler_t	http_servlet_tmpl_func_unbuffer;

/*
 * Internal use only.
 */
extern int	_http_servlet_tmpl_copy_tinfo(
			struct http_servlet_tmpl_tinfo *dst,
			const struct http_servlet_tmpl_tinfo *src);
extern void	_http_servlet_tmpl_free_tinfo(
			struct http_servlet_tmpl_tinfo *tinfo);

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_TMPL_H_ */
