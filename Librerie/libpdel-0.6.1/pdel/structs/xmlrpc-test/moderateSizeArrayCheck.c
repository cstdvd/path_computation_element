
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

DEFINE_STRUCTS_ARRAY(string_array, char *);

static const struct structs_type string_array_type
	= STRUCTS_ARRAY_TYPE(&structs_type_string, "string_array", "string");

static const struct structs_type *moderateSizeArrayCheck_ptypes[] = {
	&string_array_type
};

static http_servlet_xmlrpc_handler_t	moderateSizeArrayCheck_handler;

const struct	http_servlet_xmlrpc_method moderateSizeArrayCheck_method = {
	"validator1.moderateSizeArrayCheck",
	moderateSizeArrayCheck_handler,
	moderateSizeArrayCheck_ptypes,
	1, 1
};

static void *
moderateSizeArrayCheck_handler(void *arg, const char *method,
	struct http_request *req, u_int nparams, const void **params,
	const char *mtype, const struct structs_type **rtypep, int *faulted)
{
	const struct string_array *const a = params[0];
	char **cat;

	alog(LOG_INFO, "method \"%s\" invoked", method);
	if (a->length < 2) {
		errno = EINVAL;
		return (NULL);
	}
	if ((cat = MALLOC(mtype, sizeof(*cat))) == NULL)
		return (NULL);
	ASPRINTF(STRUCTS_TYPE_STRING_MTYPE, cat,
	    "%s%s", a->elems[0], a->elems[a->length - 1]);
	*rtypep = &structs_type_string;
	return (cat);
}

