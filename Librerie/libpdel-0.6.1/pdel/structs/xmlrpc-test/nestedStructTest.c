
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

static http_servlet_xmlrpc_handler_t	nestedStructTest_handler;

const struct	http_servlet_xmlrpc_method nestedStructTest_method = {
	"validator1.nestedStructTest",
	nestedStructTest_handler,
	NULL,			/* this handler wants "exploded" parameters */
	1, 1
};

static void *
nestedStructTest_handler(void *arg, const char *method,
	struct http_request *req, u_int nparams, const void **params,
	const char *mtype, const struct structs_type **rtypep, int *faulted)
{
	const char *struct_names[] = { "2000", "04", "01", NULL };
	const struct xmlrpc_value_union *v;
	const struct xmlrpc_struct *s;
	int32_t *result;
	int32_t sum;
	int i;
	int j;

	alog(LOG_INFO, "method \"%s\" invoked", method);

	/* Find the nested structure */
	for (i = 0, v = params[0]; struct_names[i] != NULL; i++) {
		for (j = 0; ; j++) {
			const struct xmlrpc_member *member;
			const void *data = v;
			char name[32];

			snprintf(name, sizeof(name), "struct.%d", j);
			if (structs_find(&structs_type_xmlrpc_value,
			    name, (void **)&data, 0) == NULL)
				return (NULL);
			member = data;
			if (strcmp(member->name, struct_names[i]) == 0) {
				v = &member->value;		/* found it */
				break;
			}
		}
	}

	/* Make sure we're now looking at a structure */
	if (strcmp(v->field_name, "struct") != 0) {
		errno = EINVAL;
		return (NULL);
	}
	s = &v->un->struct_;

	/* Add together the three_stooges fields */
	for (sum = i = 0; i < s->length; i++) {
		if (strcmp(s->elems[i].name, "moe") != 0
		    && strcmp(s->elems[i].name, "larry") != 0
		    && strcmp(s->elems[i].name, "curly") != 0)
			continue;
		if (strcmp(s->elems[i].value.field_name, "i4") != 0) {
			errno = EINVAL;
			return (NULL);
		}
		sum += s->elems[i].value.un->i4;
	}

	/* Copy result into malloc'd buffer */
	if ((result = MALLOC(mtype, sizeof(*result))) == NULL)
		return (NULL);
	*result = sum;

	/* Return result */
	*rtypep = &structs_type_int;
	return (result);
}

