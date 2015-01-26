
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

DEFINE_STRUCTS_ARRAY(three_stooges_array, struct three_stooges);

static const struct structs_field three_stooges_fields[] = {
	STRUCTS_STRUCT_FIELD(three_stooges, moe, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(three_stooges, larry, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(three_stooges, curly, &structs_type_int32),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type three_stooges_type
	= STRUCTS_STRUCT_TYPE(three_stooges, three_stooges_fields);
static const struct structs_type three_stooges_array_type
	= STRUCTS_ARRAY_TYPE(&three_stooges_type, MEM_TYPE, "element");

static const struct structs_type *arrayOfStructsTest_ptypes[] = {
	&three_stooges_array_type,
};

static http_servlet_xmlrpc_handler_t	arrayOfStructsTest_handler;

const struct	http_servlet_xmlrpc_method arrayOfStructsTest_method = {
	"validator1.arrayOfStructsTest",
	arrayOfStructsTest_handler,
	arrayOfStructsTest_ptypes,
	1, 1
};

static void *
arrayOfStructsTest_handler(void *arg, const char *method,
	struct http_request *req, u_int nparams, const void **params,
	const char *mtype, const struct structs_type **rtypep, int *faulted)
{
	const struct three_stooges_array *const array = params[0];
	int32_t *sum;
	int i;

	alog(LOG_INFO, "method \"%s\" invoked", method);
	if ((sum = MALLOC(mtype, sizeof(*sum))) == NULL)
		return (NULL);
	for (*sum = i = 0; i < array->length; i++)
		*sum += array->elems[i].curly;
	*rtypep = &structs_type_int32;
	return (sum);
}


