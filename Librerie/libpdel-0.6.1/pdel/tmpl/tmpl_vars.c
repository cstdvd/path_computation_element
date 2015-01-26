
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "tmpl_internal.h"

/*
 * Internal functions
 */
static int	tmpl_var_cmp(const void *v1, const void *v2);
static int	tmpl_func_cmp(const void *v1, const void *v2);
static struct	tmpl_elem *tmpl_copy_elems(const char *mtype,
			const struct tmpl_elem *oelems, int count);
static int	tmpl_copy_call(const char *mtype,
			const struct func_call *ocall, struct func_call *call);

/*
 * Get a variable.
 */
const char *
tmpl_ctx_get_var(struct tmpl_ctx *ctx, const char *name)
{
	int i;

	if ((i = _tmpl_find_var(ctx, name)) == -1) {
		errno = ENOENT;
		return (NULL);
	}
	return (ctx->vars[i].value);
}

/*
 * Set a variable.
 */
int
tmpl_ctx_set_var(struct tmpl_ctx *ctx, const char *name, const char *value)
{
	char *new_value;
	char *new_name;
	void *mem;
	int i;

	if ((new_value = STRDUP(ctx->mtype, value)) == NULL)
		return (-1);
	if ((i = _tmpl_find_var(ctx, name)) != -1) {
		struct exec_var *const var = &ctx->vars[i];

		FREE(ctx->mtype, var->value);
		var->value = new_value;
		return (0);
	}
	if ((new_name = STRDUP(ctx->mtype, name)) == NULL) {
		FREE(ctx->mtype, new_value);
		return (-1);
	}
	if ((mem = REALLOC(ctx->mtype, ctx->vars,
	    (ctx->num_vars + 1) * sizeof(*ctx->vars))) == NULL) {
		FREE(ctx->mtype, new_name);
		FREE(ctx->mtype, new_value);
		return (-1);
	}
	ctx->vars = mem;
	ctx->vars[ctx->num_vars].name = new_name;
	ctx->vars[ctx->num_vars].value = new_value;
	ctx->num_vars++;
	mergesort(ctx->vars, ctx->num_vars, sizeof(*ctx->vars), tmpl_var_cmp);
	return (0);
}

int
_tmpl_find_var(struct tmpl_ctx *ctx, const char *name)
{
	struct exec_var key;
	struct exec_var *var;

	key.name = (char *)name;
	if ((var = bsearch(&key, ctx->vars,
	    ctx->num_vars, sizeof(*ctx->vars), tmpl_var_cmp)) == NULL)
		return (-1);
	return (var - ctx->vars);
}

int
_tmpl_find_func(struct tmpl_ctx *ctx, const char *name)
{
	struct exec_func key;
	struct exec_func *func;

	key.name = (char *)name;
	if ((func = bsearch(&key, ctx->funcs,
	    ctx->num_funcs, sizeof(*ctx->funcs), tmpl_func_cmp)) == NULL)
		return (-1);
	return (func - ctx->funcs);
}

int
_tmpl_set_func(struct tmpl_ctx *ctx, const char *name,
	const struct tmpl_elem *oelems, int nelems)
{
	struct tmpl_elem *elems;
	char *new_name;
	void *eblock;
	size_t bsize;
	void *mem;
	int i;

	/* Copy function elements */
	if ((elems = tmpl_copy_elems(ctx->mtype, oelems, nelems)) == NULL)
		return (-1);

	/* Allocate and fill in compacted memory block */
	bsize = nelems * sizeof(*elems);
	_tmpl_compact_elems(ctx->mtype, elems, nelems, NULL, &bsize);
	if ((eblock = MALLOC(ctx->mtype, bsize)) == NULL) {
		_tmpl_free_elems(ctx->mtype, NULL, elems, nelems);
		return (-1);
	}
	bsize = 0;
	_tmpl_compact(ctx->mtype,
	    eblock, &elems, nelems * sizeof(*elems), &bsize);
	_tmpl_compact_elems(ctx->mtype, elems, nelems, eblock, &bsize);

	/* See if function is already defined */
	if ((i = _tmpl_find_func(ctx, name)) != -1) {
		struct exec_func *const func = &ctx->funcs[i];

		_tmpl_free_elems(ctx->mtype, func->eblock,
		    func->elems, func->num_elems);
		func->elems = eblock;
		func->num_elems = nelems;
		func->eblock = eblock;
		return (0);
	}

	/* Copy function name */
	if ((new_name = STRDUP(ctx->mtype, name)) == NULL) {
		FREE(ctx->mtype, eblock);
		return (-1);
	}

	/* Add new function to list */
	if ((mem = REALLOC(ctx->mtype, ctx->funcs,
	    (ctx->num_funcs + 1) * sizeof(*ctx->funcs))) == NULL) {
		FREE(ctx->mtype, new_name);
		FREE(ctx->mtype, eblock);
		return (-1);
	}
	ctx->funcs = mem;
	ctx->funcs[ctx->num_funcs].name = new_name;
	ctx->funcs[ctx->num_funcs].elems = eblock;
	ctx->funcs[ctx->num_funcs].num_elems = nelems;
	ctx->funcs[ctx->num_funcs].eblock = eblock;
	ctx->num_funcs++;
	mergesort(ctx->funcs,
	    ctx->num_funcs, sizeof(*ctx->funcs), tmpl_func_cmp);
	return (0);
}

static int
tmpl_var_cmp(const void *v1, const void *v2)
{
	const struct exec_var *const var1 = v1;
	const struct exec_var *const var2 = v2;

	return (strcmp(var1->name, var2->name));
}

static int
tmpl_func_cmp(const void *v1, const void *v2)
{
	const struct exec_func *const func1 = v1;
	const struct exec_func *const func2 = v2;

	return (strcmp(func1->name, func2->name));
}

/*
 * Copy a range of elements into a new array.
 */
static struct tmpl_elem *
tmpl_copy_elems(const char *mtype, const struct tmpl_elem *oelems, int count)
{
	struct tmpl_elem *elems;
	int i;

	if ((elems = MALLOC(mtype, count * sizeof(*elems))) == NULL)
		return (NULL);
	memset(elems, 0, count * sizeof(*elems));
	for (i = 0; i < count; i++) {
		const struct tmpl_elem *const oe = &oelems[i];
		struct tmpl_elem *const e = &elems[i];

		e->len = oe->len;
		e->flags = (oe->flags & ~TMPL_ELEM_MMAP_TEXT);
		if (oe->text != NULL) {
			if ((e->text = MALLOC(mtype, e->len)) == NULL)
				goto fail;
			memcpy(e->text, oe->text, e->len);
		} else if (tmpl_copy_call(mtype, &oe->call, &e->call) == -1)
			goto fail;
		e->u = oe->u;
	}
	return (elems);

fail:
	_tmpl_free_elems(mtype, NULL, elems, count);
	return (NULL);
}

/*
 * Copy a function call.
 */
static int
tmpl_copy_call(const char *mtype,
	const struct func_call *ocall, struct func_call *call)
{
	int i;

	memset(call, 0, sizeof(*call));
	call->type = ocall->type;
	call->handler = ocall->handler;
	if ((call->funcname = STRDUP(mtype, ocall->funcname)) == NULL)
		goto fail;
	if ((call->args = MALLOC(mtype,
	    ocall->nargs * sizeof(*call->args))) == NULL)
		goto fail;
	memset(call->args, 0, ocall->nargs * sizeof(*call->args));
	call->nargs = ocall->nargs;
	for (i = 0; i < ocall->nargs; i++) {
		struct func_arg *const oa = &ocall->args[i];
		struct func_arg *const a = &call->args[i];

		if ((a->is_literal = oa->is_literal)) {
			if ((a->u.literal = STRDUP(mtype,
			    oa->u.literal)) == NULL)
				goto fail;
		} else if (tmpl_copy_call(mtype, &oa->u.call, &a->u.call) == -1)
			goto fail;
	}
	return (0);

fail:
	_tmpl_free_call(mtype, call);
	return (-1);
}

