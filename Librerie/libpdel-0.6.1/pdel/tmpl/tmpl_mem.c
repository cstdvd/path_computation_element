
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "tmpl_internal.h"

/*
 * Free an array of parse elements.
 */
void
_tmpl_free_elems(const char *mtype, void *eblock,
	struct tmpl_elem *elems, int num)
{
	int i;

	if (eblock != NULL) {
		FREE(mtype, eblock);
		return;
	}
	for (i = 0; i < num; i++) {
		if ((elems[i].flags & TMPL_ELEM_MMAP_TEXT) == 0)
			FREE(mtype, elems[i].text);
		_tmpl_free_call(mtype, &elems[i].call);
	}
	FREE(mtype, elems);
}

/*
 * Free a run-time function structure.
 */
void
_tmpl_free_func(struct tmpl_ctx *ctx, struct exec_func *func)
{
	/* Free name */
	FREE(ctx->mtype, func->name);

	/* Free elements */
	_tmpl_free_elems(ctx->mtype, func->eblock, func->elems, func->num_elems);
}

/*
 * Free a parsed function call structure.
 */
void
_tmpl_free_call(const char *mtype, struct func_call *call)
{
	int i;

	for (i = 0; i < call->nargs; i++)
		_tmpl_free_arg(mtype, &call->args[i]);
	FREE(mtype, call->args);
	FREE(mtype, call->funcname);
	memset(call, 0, sizeof(*call));
}

/*
 * Free a parsed function argument.
 */
void
_tmpl_free_arg(const char *mtype, struct func_arg *arg)
{
	if (arg->is_literal) {
		FREE(mtype, arg->u.literal);
		arg->u.literal = NULL;
	} else
		_tmpl_free_call(mtype, &arg->u.call);
	memset(arg, 0, sizeof(*arg));
}

