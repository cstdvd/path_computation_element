
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#ifndef _LWS_TMPL_H_
#define _LWS_TMPL_H_

/* Information passed to tmpl servlet user-defined functions */
struct lws_tmpl_info {
	const struct lws_server		*server;
	const struct lws_file_info	*fileinfo;
	int				sidx;		/* server index */
	int				vidx;		/* vhost index */
	int				lidx;		/* servlet index */
};

/* Descriptor for an LWS template function */
struct lws_tmpl_func {
	struct tmpl_func	func;		/* the usual info */
	const char		*params;	/* parameter names */
	const char		*desc;		/* description */
};

#define LWS_TMPL_FUNC2(name, prefix, min, max, params, desc)	\
	{ { (#name), (min), (max), (prefix ## name) }, (params), (desc) }
#define LWS_TMPL_FUNC(name, min, max, params, desc)	\
	LWS_TMPL_FUNC2(name, lws_tf_, min, max, params, desc)

/* Template function list per-thread object */
extern struct	tinfo lws_tmpl_funclist_tinfo;

/* Tmpl servlet handler and error formatter functions */
extern tmpl_handler_t	lws_tmpl_handler;
extern tmpl_errfmtr_t	lws_tmpl_errfmtr;

/* Template function list setup/teardown */
extern int	lws_tmpl_funcs_init(void);
extern void	lws_tmpl_funcs_uninit(void);

#endif	/* !_LWS_TMPL_H_ */
