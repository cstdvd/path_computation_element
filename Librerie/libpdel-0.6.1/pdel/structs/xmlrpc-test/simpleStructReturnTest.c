
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

struct powers10 {
	int32_t		times10;
	int32_t		times100;
	int32_t		times1000;
};

static const struct structs_field powers10_fields[] = {
	STRUCTS_STRUCT_FIELD(powers10, times10, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(powers10, times100, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(powers10, times1000, &structs_type_int32),
	STRUCTS_STRUCT_FIELD_END
};
const struct structs_type powers10_type
	= STRUCTS_STRUCT_TYPE(powers10, powers10_fields);

static const struct structs_type *simpleStructReturnTest_ptypes[] = {
	&structs_type_int
};

static http_servlet_xmlrpc_handler_t	simpleStructReturnTest_handler;

const struct	http_servlet_xmlrpc_method simpleStructReturnTest_method = {
	"validator1.simpleStructReturnTest",
	simpleStructReturnTest_handler,
	simpleStructReturnTest_ptypes,
	1, 1
};

static void *
simpleStructReturnTest_handler(void *arg, const char *method,
	struct http_request *req, u_int nparams, const void **params,
	const char *mtype, const struct structs_type **rtypep, int *faulted)
{
	const int32_t num = *((int32_t *)params[0]);
	struct powers10 *p;

	alog(LOG_INFO, "method \"%s\" invoked", method);
	if ((p = MALLOC(mtype, sizeof(*p))) == NULL)
		return (NULL);
	p->times10 = num * 10;
	p->times100 = num * 100;
	p->times1000 = num * 1000;
	*rtypep = &powers10_type;
	return (p);
}


