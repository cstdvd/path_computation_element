
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

static http_servlet_xmlrpc_handler_t	faultTest_handler;

const struct	http_servlet_xmlrpc_method faultTest_method = {
	"faultTest",
	faultTest_handler,
	NULL,			/* this handler wants "exploded" parameters */
	0, INT_MAX
};

static void *
faultTest_handler(void *arg, const char *method, struct http_request *req,
	u_int nparams, const void **params, const char *mtype,
	const struct structs_type **rtypep, int *faulted)
{
	struct xmlrpc_compact_fault *fault;

	alog(LOG_INFO, "method \"%s\" invoked", method);

	/* Create fault */
	if ((fault = MALLOC(mtype, sizeof(*fault))) == NULL)
		return (NULL);
	if (structs_init(&structs_type_xmlrpc_compact_fault,
	    NULL, fault) == -1) {
		FREE(mtype, fault);
		return NULL;
	}

	/* Set fault code and fault message string */
	fault->faultCode = 12345678;
	if (structs_set_string(&structs_type_xmlrpc_compact_fault,
	    "faultString", "This is the fault message", fault, NULL, 0) == -1) {
		FREE(mtype, fault);
		return NULL;
	}

	/* Return fault */
	*rtypep = &structs_type_xmlrpc_compact_fault;
	*faulted = 1;
	return fault;
}

