
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "xmlrpc_test.h"

struct entities {
	int32_t	ctLeftAngleBrackets;
	int32_t	ctRightAngleBrackets;
	int32_t	ctAmpersands;
	int32_t	ctApostrophes;
	int32_t	ctQuotes;
};

static const struct structs_field entities_fields[] = {
	STRUCTS_STRUCT_FIELD(entities, ctLeftAngleBrackets,
		&structs_type_int32),
	STRUCTS_STRUCT_FIELD(entities, ctRightAngleBrackets,
		&structs_type_int32),
	STRUCTS_STRUCT_FIELD(entities, ctAmpersands, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(entities, ctApostrophes, &structs_type_int32),
	STRUCTS_STRUCT_FIELD(entities, ctQuotes, &structs_type_int32),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type entities_type
	= STRUCTS_STRUCT_TYPE(entities, entities_fields);

static const struct structs_type *countTheEntities_ptypes[] = {
	&structs_type_string
};

static http_servlet_xmlrpc_handler_t	countTheEntities_handler;

const struct	http_servlet_xmlrpc_method countTheEntities_method = {
	"validator1.countTheEntities",
	countTheEntities_handler,
	countTheEntities_ptypes,
	1, 1
};

static void *
countTheEntities_handler(void *arg, const char *method,
	struct http_request *req, u_int nparams, const void **params,
	const char *mtype, const struct structs_type **rtypep, int *faulted)
{
	const char *const string = *((char **)params[0]);
	struct entities *e;
	const char *s;

	alog(LOG_INFO, "method \"%s\" invoked", method);
	if ((e = MALLOC(mtype, sizeof(*e))) == NULL)
		return (NULL);
	memset(e, 0, sizeof(*e));
	for (s = string; *s != '\0'; s++) {
		switch (*s) {
		case '<':
			e->ctLeftAngleBrackets++;
			break;
		case '>':
			e->ctRightAngleBrackets++;
			break;
		case '&':
			e->ctAmpersands++;
			break;
		case '\'':
			e->ctApostrophes++;
			break;
		case '"':
			e->ctQuotes++;
			break;
		default:
			break;
		}
	}
	*rtypep = &entities_type;
	return (e);
}


