
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

static http_servlet_xmlrpc_handler_t	manyTypesTest_handler;

const struct	http_servlet_xmlrpc_method manyTypesTest_method = {
	"validator1.manyTypesTest",
	manyTypesTest_handler,
	NULL,			/* this handler wants "exploded" parameters */
	6, 6
};

static void *
manyTypesTest_handler(void *arg, const char *method, struct http_request *req,
	u_int nparams, const void **params, const char *mtype,
	const struct structs_type **rtypep, int *faulted)
{
	const struct structs_type *const vtype = &structs_type_xmlrpc_value;
	struct xmlrpc_value_union *v;
	int i;

	alog(LOG_INFO, "method \"%s\" invoked", method);

	/* Initialize "exploded" XML-RPC return value */
	if ((v = MALLOC(mtype, sizeof(*v))) == NULL)
		return (NULL);
	if (structs_init(vtype, NULL, v) == -1)
		goto fail1;

	/* Set returned value to be an array */
	if (structs_union_set(vtype, NULL, v, "array") == -1)
		goto fail2;

	/* Copy input parameters into the output array */
	for (i = 0; i < nparams; i++) {
		char name[32];

		if (structs_array_insert(vtype, "array.data", i, v) == -1)
			goto fail2;
		snprintf(name, sizeof(name), "array.data.%d", i);
		if (structs_set(vtype, params[i], name, v) == -1)
			goto fail2;
	}

	/* Return "raw" result */
	*rtypep = NULL;
	return (v);

	/* Clean up after failure */
fail2:	structs_free(vtype, NULL, v);
fail1:	FREE(mtype, v);
	return (NULL);
}

