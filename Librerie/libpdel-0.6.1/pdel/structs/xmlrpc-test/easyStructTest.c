
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

static const struct structs_type *easyStructTest_ptypes[] = {
	&three_stooges_type
};

static http_servlet_xmlrpc_handler_t	easyStructTest_handler;

const struct	http_servlet_xmlrpc_method easyStructTest_method = {
	"validator1.easyStructTest",
	easyStructTest_handler,
	easyStructTest_ptypes,
	1, 1
};

static void *
easyStructTest_handler(void *arg, const char *method, struct http_request *req,
	u_int nparams, const void **params, const char *mtype,
	const struct structs_type **rtypep, int *faulted)
{
	const struct three_stooges *const m = params[0];
	int32_t *sum;

	alog(LOG_INFO, "method \"%s\" invoked", method);
	if ((sum = MALLOC(mtype, sizeof(*sum))) == NULL)
		return (NULL);
	*sum = m->moe + m->larry + m->curly;
	*rtypep = &structs_type_int32;
	return (sum);
}

