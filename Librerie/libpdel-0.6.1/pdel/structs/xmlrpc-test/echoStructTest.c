
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

static http_servlet_xmlrpc_handler_t	echoStructTest_handler;

const struct	http_servlet_xmlrpc_method echoStructTest_method = {
	"validator1.echoStructTest",
	echoStructTest_handler,
	NULL,			/* this handler wants "exploded" parameters */
	1, 1
};

static void *
echoStructTest_handler(void *arg, const char *method, struct http_request *req,
	u_int nparams, const void **params, const char *mtype,
	const struct structs_type **rtypep, int *faulted)
{
	struct xmlrpc_value_union *v;

	alog(LOG_INFO, "method \"%s\" invoked", method);

	/* Create raw "exploded" XML-RPC return value */
	if ((v = MALLOC(mtype, sizeof(*v))) == NULL)
		return (NULL);
	if (structs_init(&structs_type_xmlrpc_value, NULL, v) == -1) {
		FREE(mtype, v);
		return (NULL);
	}

	/* Copy in the value sent as the first parameter */
	if (structs_set(&structs_type_xmlrpc_value, params[0], NULL, v) == -1) {
		FREE(mtype, v);
		return (NULL);
	}

	/* Return "raw" result */
	*rtypep = NULL;
	return (v);
}

