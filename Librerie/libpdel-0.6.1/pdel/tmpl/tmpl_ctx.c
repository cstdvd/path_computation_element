
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "tmpl_internal.h"

/*
 * Internal functions
 */
static void	tmpl_free_vars(struct tmpl_ctx *ctx);
static void	tmpl_free_funcs(struct tmpl_ctx *ctx);
static int	tmpl_userfunc_cmp(const void *v1, const void *v2);

/*
 * Create a template context.
 */
struct tmpl_ctx *
tmpl_ctx_create(void *arg, const char *mtype,
	tmpl_handler_t *handler, tmpl_errfmtr_t *errfmtr)
{
	struct tmpl_ctx *ctx;

	/* Initialize template file */
	if ((ctx = MALLOC(mtype, sizeof(*ctx))) == NULL)
		return (NULL);
	memset(ctx, 0, sizeof(*ctx));
	if (mtype != NULL) {
		strlcpy(ctx->mtype_buf, mtype, sizeof(ctx->mtype_buf));
		ctx->mtype = ctx->mtype_buf;
	}
	ctx->arg = arg;
	ctx->handler = handler;
	ctx->errfmtr = errfmtr;

	/* Done */
	return (ctx);
}

/*
 * Get context argument.
 */
void *
tmpl_ctx_get_arg(struct tmpl_ctx *ctx)
{
	return (ctx->arg);
}
        
/*
 * Get context memory type.
 */
const char *
tmpl_ctx_get_mtype(struct tmpl_ctx *ctx)
{
	return (ctx->mtype);
}

/*
 * Handler for handling a fixed list of user functions.
 */
char *
tmpl_list_handler(struct tmpl_ctx *ctx, const struct tmpl_func *userfuncs,
	u_int uflen, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	struct tmpl_func key;
	struct tmpl_func *f;
	char buf[256];
	char *s;
	int i;

	/* If no userfunc's supplied, bail */
	if (userfuncs == NULL) {
		*errmsgp = STRDUP(mtype, "unknown template function");
		goto error;
	}

	/* Find function in user-supplied list */
	key.name = av[0];
	if ((f = bsearch(&key, userfuncs, uflen,
	    sizeof(*userfuncs), tmpl_userfunc_cmp)) == NULL) {
		*errmsgp = STRDUP(mtype, "unknown template function");
		goto error;
	}

	/* Check number of arguments */
	if (ac - 1 < f->min_args) {
		ASPRINTF(mtype, errmsgp,
		    "at %s %d argument%s %s for \"@%s\"",
		    "least" _ f->min_args _ f->min_args == 1 ? "" : "s" _
		    "required" _ f->name);
		goto error;
	}
	if (ac - 1 > f->max_args) {
		ASPRINTF(mtype, errmsgp,
		    "at %s %d argument%s %s for \"@%s\"",
		    "most" _ f->max_args _ f->max_args == 1 ? "" : "s" _
		    "allowed" _ f->name);
		goto error;
	}

	/* Invoke handler */
	if ((s = (*f->handler)(ctx, errmsgp, ac, av)) != NULL)
		return (s);

error:
	/* Handle errors by showing the function that generated the error */
	snprintf(buf, sizeof(buf), "@%s(", av[0]);
	for (i = 1; i < ac; i++) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%s%s", (i > 1) ? ", " : "", av[i]);
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "): %s",
	    (*errmsgp != NULL) ? *errmsgp : strerror(errno));
	if (*errmsgp != NULL)
		FREE(mtype, *errmsgp);
	*errmsgp = STRDUP(mtype, buf);
	return (NULL);
}

/*
 * Destroy a template context.
 */
void
tmpl_ctx_destroy(struct tmpl_ctx **ctxp)
{
	struct tmpl_ctx *const ctx = *ctxp;

	/* Sanity check */
	if (ctx == NULL)
		return;
	*ctxp = NULL;

	/* Free run-time state */
	tmpl_ctx_reset(ctx);

	/* Free context */
	FREE(ctx->mtype, ctx);
}

/*
 * Do the things we need in order to reset a template context.
 */
void
tmpl_ctx_reset(struct tmpl_ctx *ctx)
{
	tmpl_free_vars(ctx);
	tmpl_free_funcs(ctx);
}

/*
 * Free up the vars part of a template context.
 */
static void
tmpl_free_vars(struct tmpl_ctx *ctx)
{
	int i;

	/* Free variables */
	for (i = 0; i < ctx->num_vars; i++) {
		struct exec_var *const var = &ctx->vars[i];

		FREE(ctx->mtype, var->name);
		FREE(ctx->mtype, var->value);
	}
	FREE(ctx->mtype, ctx->vars);

	ctx->num_vars = 0;
	ctx->vars = NULL;
}

/*
 * Free up the funcs part of a template context.
 */
static void
tmpl_free_funcs(struct tmpl_ctx *ctx)
{
	int i;

	/* Free run-time functions */
	for (i = 0; i < ctx->num_funcs; i++)
		_tmpl_free_func(ctx, &ctx->funcs[i]);
	FREE(ctx->mtype, ctx->funcs);

	ctx->num_funcs = 0;
	ctx->funcs = NULL;
}

/*
 * Compare two struct tmpl_func's.
 */
static int
tmpl_userfunc_cmp(const void *v1, const void *v2)
{
	const struct tmpl_func *const f1 = v1;
	const struct tmpl_func *const f2 = v2;

	return (strcmp(f1->name, f2->name));
}

