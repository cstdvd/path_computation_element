
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_XMLRPC_H_
#define _PDEL_HTTP_SERVLET_XMLRPC_H_

struct http_servlet_xmlrpc_method;
struct http_servlet_xmlrpc_servlet_info;
struct http_request;
struct structs_type;

/*
 * User-supplied handler for http_servlet_xmlrpc. Should return the
 * reply structure (and set *rtypep to its type) if successful.
 *
 * If a fault is to be returned, return a "struct structs_xmlrpc_fault",
 * set *typep to &structs_type_xmlrpc_fault, and set *fault to non-zero.
 *
 * If a system error is to be returned, return NULL and set errno.
 *
 * Parameters
 *
 *	arg		User-supplied cookie
 *	method		Name of XML-RPC method
 *	req		Incoming HTTP request structure (do not free)
 *	nparams		Number of parameters
 *	params		Parameters
 *	mtype		Memory type for allocating response
 *	rtypep		Upon return, response type or NULL for xmlrpc_value
 *	fault		Set *fault to non-zero if returning a fault
 */
typedef void	*http_servlet_xmlrpc_handler_t(void *arg, const char *method,
			struct http_request *req, u_int nparams,
			const void **params, const char *mtype,
			const struct structs_type **rtypep, int *fault);

/*
 * Information required by for one XML-RPC method.
 *
 *	name		Name of XML-RPC method, or empty string for wildcard
 *	handler		Handler function for method
 *	ptypes		Parameter types, or NULL for xmlrpc_value
 *	min_params	Minimum number of parameters accepted
 *	max_params	Maximum number of parameters accepted
 */
typedef struct http_servlet_xmlrpc_method {
	const char      		*name;		/* method name */
	http_servlet_xmlrpc_handler_t	*handler;	/* method handler */
	const struct structs_type	**ptypes;	/* parameter types */
	u_int             		min_params;	/* min # params */
	u_int             		max_params;	/* max # params */
} http_servlet_method;

/*
 * Information required by http_servlet_xmlrpc().
 *
 *	method		List of acceptable methods
 *	logger		Logging function
 */
typedef struct http_servlet_xmlrpc_info {
	const struct http_servlet_xmlrpc_method
					*methods;	/* methods (NULL trm)*/
	http_logger_t			*logger;	/* loggging function */
} http_servlet_xmlrpc_info;

__BEGIN_DECLS

/*
 * Create a new http_servlet_xmlrpc servlet.
 *
 * NOTE: "info" is not copied and so must remain valid for the
 * lifetime of the servlet.
 */
extern struct	http_servlet *http_servlet_xmlrpc_create(
			const struct http_servlet_xmlrpc_info *info,
			void *arg, void (*destroy)(void *));

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_XMLRPC_H_ */

