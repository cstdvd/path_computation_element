
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_tmpl.h"
#include "lws_tmpl_http.h"

/***********************************************************************
			HTTP TMPL FUNCTIONS
***********************************************************************/

/* Our user-defined template functions */
static tmpl_handler_t	lws_tf_http_path;
static tmpl_handler_t	lws_tf_query_count;
static tmpl_handler_t	lws_tf_query_name;
static tmpl_handler_t	lws_tf_query_value;
static tmpl_handler_t	lws_tf_http_remote_ip;
static tmpl_handler_t	lws_tf_http_remote_port;

#define SERVLET_TMPL_FUNC(name, min, max, params, desc)		\
	LWS_TMPL_FUNC2(name, http_servlet_tmpl_func_, min, max, params, desc)

const struct lws_tmpl_func lws_tmpl_http_functions[] = {
    SERVLET_TMPL_FUNC(query, 1, 1, "fieldname",
	"Returns the value of the HTML GET or POST form field $1, or the"
"\n"	"empty string if no such field was submitted."),
    SERVLET_TMPL_FUNC(query_exists, 1, 1, "fieldname",
	"Returns \"1\" if an HTML GET or POST form field named $1 was"
"\n"	"submitted, otherwise \"0\"."),
    SERVLET_TMPL_FUNC(query_string, 0, 0, "",
	"Returns the query string from the HTTP request."),
    LWS_TMPL_FUNC(query_count, 0, 0, "",
	"Returns the number of form fields submitted by the browser."),
    LWS_TMPL_FUNC(http_path, 0, 0, "",
	"Returns the path component of the requested URL."),
    LWS_TMPL_FUNC(query_name, 1, 1, "index",
	"Returns the name of the $1'th field submitted by the browser."),
    LWS_TMPL_FUNC(query_value, 1, 1, "index",
	"Returns the value of the $1'th field submitted by the browser."),
    SERVLET_TMPL_FUNC(get_header, 1, 1, "name",
	"Returns the value of the HTTP request header named $1."),
    SERVLET_TMPL_FUNC(set_header, 2, 2, "name:value",
	"Sets the value of the HTTP response header named $1 to $2."),
    SERVLET_TMPL_FUNC(remove_header, 1, 1, "name",
	"Removes the HTTP response header named $1."),
    SERVLET_TMPL_FUNC(redirect, 1, 1, "url",
	"Sends back an HTTP redirect response, redirecting the requesting"
"\n"	"browser to the URL $1."),
    SERVLET_TMPL_FUNC(unbuffer, 0, 0, "",
	"Unbuffers the HTTP response body. This causes the HTTP response"
"\n"	"headers to be sent and means the response body does not have to be"
"\n"	"all gathered up in memory being sent to the browser."),
    LWS_TMPL_FUNC(http_remote_ip, 0, 0, "",
	"Returns the IP address of the requesting HTTP client."),
    LWS_TMPL_FUNC(http_remote_port, 0, 0, "",
	"Returns the TCP port of the requesting HTTP client."),
    { { NULL } }
};

/************************************************************************
			    HTTP STUFF
************************************************************************/

/* 
 * Get field count.
 *
 * Usage: @query_count()
 */
static char *
lws_tf_query_count(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[32];

	snprintf(buf, sizeof(buf),
	    "%d", http_request_get_num_values(targ->req));
	return (STRDUP(mtype, buf));
}

/* 
 * Get field name by index.
 *
 * Usage: @query_name(index)
 */
static char *
lws_tf_query_name(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *name;

	if (http_request_get_value_by_index(targ->req,
	    atoi(av[1]), &name, NULL) == -1)
		return (NULL);
	return (STRDUP(mtype, name));
}

/* 
 * Get field value by index.
 *
 * Usage: @query_value(index)
 */
static char *
lws_tf_query_value(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *value;

	if (http_request_get_value_by_index(targ->req,
	    atoi(av[1]), NULL, &value) == -1)
		return (NULL);
	return (STRDUP(mtype, value));
}

/* 
 * Get peer's remote IP address.
 *
 * Usage: @http_remote_ip()
 */
static char *
lws_tf_http_remote_ip(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype,
	    inet_ntoa(http_request_get_remote_ip(targ->req))));
}

/* 
 * Get peer's remote TCP port.
 *
 * Usage: @http_remote_port()
 */
static char *
lws_tf_http_remote_port(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[16];

	snprintf(buf, sizeof(buf),
	    "%u", http_request_get_remote_port(targ->req));
	return (STRDUP(mtype, buf));
}

/* 
 * Get path.
 *
 * Usage: @http_path()
 */
static char *
lws_tf_http_path(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const path = http_request_get_path(targ->req);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype, path));
}

