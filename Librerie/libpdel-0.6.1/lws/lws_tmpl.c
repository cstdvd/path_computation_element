
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"
#include "lws_tmpl.h"
#include "lws_tmpl_auth.h"
#include "lws_tmpl_config.h"
#include "lws_tmpl_http.h"
#include "lws_tmpl_logs.h"
#include "lws_tmpl_misc.h"
#include "lws_tmpl_object.h"
#include "lws_tmpl_passwd.h"
#include "lws_tmpl_string.h"

/***********************************************************************
			TMPL FUNCTION SETUP
***********************************************************************/

#define LWS_TMPL_FUNCS_MTYPE	"lws_tmpl_functions"

static const struct lws_tmpl_func *lws_tmpl_builtins[] = {
	lws_tmpl_auth_functions,
	lws_tmpl_config_functions,
	lws_tmpl_http_functions,
	lws_tmpl_logs_functions,
	lws_tmpl_misc_functions,
	lws_tmpl_object_functions,
	lws_tmpl_string_functions,
	NULL
};

static int	lws_tmpl_func_cmp(const void *v1, const void *v2);

static int	lws_tmpl_nfuncs;
static struct	lws_tmpl_func *lws_tmpl_descriptors;
static struct	tmpl_func *lws_tmpl_functions;

/*
 * Initialize user-defined template function list.
 *
 * XXX In the future, we could load user functions from shared objects
 * XXX using dlopen(), etc.
 */
int
lws_tmpl_funcs_init(void)
{
	int i;
	int j;
	int k;

	/* Sanity check */
	assert(lws_tmpl_functions == NULL);
	assert(lws_tmpl_descriptors == NULL);
	assert(lws_tmpl_nfuncs == 0);

	/* Count the number of built-in functions */
	for (lws_tmpl_nfuncs = i = 0; lws_tmpl_builtins[i] != NULL; i++) {
		for (j = 0; lws_tmpl_builtins[i][j].func.name != NULL; j++);
		lws_tmpl_nfuncs += j;
	}

	/* Allocate arrays */
	if ((lws_tmpl_functions = MALLOC(LWS_TMPL_FUNCS_MTYPE,
	    lws_tmpl_nfuncs * sizeof(*lws_tmpl_functions))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		return (-1);
	}
	if ((lws_tmpl_descriptors = MALLOC(LWS_TMPL_FUNCS_MTYPE,
	    lws_tmpl_nfuncs * sizeof(*lws_tmpl_descriptors))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		FREE(LWS_TMPL_FUNCS_MTYPE, lws_tmpl_functions);
		lws_tmpl_functions = NULL;
		return (-1);
	}

	/* Fill in arrays */
	for (k = i = 0; lws_tmpl_builtins[i] != NULL; i++) {
		for (j = 0; lws_tmpl_builtins[i][j].func.name != NULL; j++) {
			lws_tmpl_descriptors[k] = lws_tmpl_builtins[i][j];
			lws_tmpl_functions[k] = lws_tmpl_descriptors[k].func;
			k++;
		}
	}
	assert(k == lws_tmpl_nfuncs);

	/* Sort the arrays by function name */
	qsort(lws_tmpl_functions, lws_tmpl_nfuncs,
	    sizeof(*lws_tmpl_functions), lws_tmpl_func_cmp);
	qsort(lws_tmpl_descriptors, lws_tmpl_nfuncs,
	    sizeof(*lws_tmpl_descriptors), lws_tmpl_func_cmp);

	/* Done */
	return (0);
}

void
lws_tmpl_funcs_uninit(void)
{
	FREE(LWS_TMPL_FUNCS_MTYPE, lws_tmpl_functions);
	lws_tmpl_functions = NULL;
	FREE(LWS_TMPL_FUNCS_MTYPE, lws_tmpl_descriptors);
	lws_tmpl_descriptors = NULL;
	lws_tmpl_nfuncs = 0;
}

static int
lws_tmpl_func_cmp(const void *v1, const void *v2)
{
	const struct tmpl_func *const func1 = v1;
	const struct tmpl_func *const func2 = v2;

	return (strcmp(func1->name, func2->name));
}

/***********************************************************************
			TMPL HANDLER FUNCTIONS
***********************************************************************/

/*
 * Template user-defined function handler.
 */
char *
lws_tmpl_handler(struct tmpl_ctx *ctx, char **errmsgp, int argc, char **argv)
{
	return (tmpl_list_handler(ctx, lws_tmpl_functions,
	    lws_tmpl_nfuncs, errmsgp, argc, argv));
}

/*
 * Template error formatter.
 *
 * Format the error message in HTML.
 */
char *
lws_tmpl_errfmtr(struct tmpl_ctx *ctx, const char *errmsg)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char *s;

	ASPRINTF(mtype, &s,
	    "<font color=\"#ff0000\"><strong>Error: %s</strong></font>",
	    errmsg);
	return (s);
}

/***********************************************************************
			TMPL FUNCTION LIST OBJECT
***********************************************************************/

#define LWS_TMPL_FUNCLIST_MTYPE		"lws_tmpl_funclist"

/* Structs type for a 'struct tmpl_func' */
static const struct structs_field tmpl_func_fields[] = {
	STRUCTS_STRUCT_FIELD(tmpl_func, name, &structs_type_string),
	STRUCTS_STRUCT_FIELD(tmpl_func, min_args, &structs_type_uint),
	STRUCTS_STRUCT_FIELD(tmpl_func, max_args, &structs_type_uint),
	{ NULL }
};
static const struct structs_type tmpl_func_type
	= STRUCTS_STRUCT_TYPE(tmpl_func, &tmpl_func_fields);

/* Structs type for a 'struct lws_tmpl_func' */
static const struct structs_field lws_tmpl_func_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_tmpl_func, func, &tmpl_func_type),
	STRUCTS_STRUCT_FIELD(lws_tmpl_func, params, &structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_tmpl_func, desc, &structs_type_string),
	{ NULL }
};
static const struct structs_type lws_tmpl_func_type
	= STRUCTS_STRUCT_TYPE(lws_tmpl_func, &lws_tmpl_func_fields);

/* Structs type for 'struct lws_tmpl_funclist': array of struct lws_tmpl_func */
DEFINE_STRUCTS_ARRAY(lws_tmpl_funclist, struct lws_tmpl_func);

static const struct structs_type lws_tmpl_funclist_type
	= STRUCTS_ARRAY_TYPE(&lws_tmpl_func_type,
	    LWS_TMPL_FUNCLIST_MTYPE, "func");

static tinfo_init_t	lws_tmpl_funclist_init;

struct tinfo lws_tmpl_funclist_tinfo = TINFO_INIT(&lws_tmpl_funclist_type,
	LWS_TMPL_FUNCLIST_MTYPE, lws_tmpl_funclist_init);

/*
 * Initialize this thread's list of template functions by copying the
 * 'lws_tmpl_descriptors' array already built for us at startup time.
 */
static int
lws_tmpl_funclist_init(struct tinfo *t, void *data)
{
	struct lws_tmpl_funclist *const funclist = data;
	struct structs_array ary;

	memset(funclist, 0, sizeof(*funclist));
	ary.elems = lws_tmpl_descriptors;
	ary.length = lws_tmpl_nfuncs;
	if (structs_set(t->type, &ary, NULL, data) == -1)
		return (-1);
	return (0);
}

