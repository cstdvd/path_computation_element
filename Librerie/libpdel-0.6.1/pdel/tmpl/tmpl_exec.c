
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "tmpl_internal.h"

/*
 * Internal functions
 */
static char	*tmpl_invoke_defined(struct tmpl_ctx *ctx,
			struct exec_func *func, char **errmsgp,
			int argc, char **argv);
static int	tmpl_invoke_defined2(struct tmpl_ctx *ctx,
			struct exec_func *func, char **errmsgp,
			int argc, char **argv);
static void	tmpl_exec_cleanup(void *arg);
static char	*evaluate_arg(struct tmpl_ctx *ctx,
			char **errmsgp, const struct func_arg *arg);
static void	pr_err(struct tmpl_ctx *ctx, int errnum, const char *msg);
#if TMPL_DEBUG
static const	char *tmpl_rtnstr(enum exec_rtn rtn);
#endif

/*
 * Execute a template.
 */
int
tmpl_execute(struct tmpl *tmpl, struct tmpl_ctx *ctx, FILE *output, int flags)
{
	/* Avoid reentrant execution with the same context */
	if (ctx->output != NULL) {
		errno = EBUSY;
		return (-1);
	}

	/* Allow output to be NULL */
	if (output == NULL) {
		if ((output = fopen(_PATH_DEVNULL, "w")) == NULL)
			return (-1);
		ctx->close_output = 1;
	} else
		ctx->close_output = 0;

	/* Push cleanup hook */
	pthread_cleanup_push(tmpl_exec_cleanup, ctx);

	/* Set up context info for this run */
	ctx->output = output;
	ctx->orig_output = output;
	ctx->flags = flags;

	/* Execute template */
	_tmpl_execute_elems(ctx, tmpl->elems, 0, tmpl->num_elems);

	/* Finish up */
	pthread_cleanup_pop(1);

	/* Done */
	return (0);
}

/*
 * Clean up a context after execution.
 */
static void
tmpl_exec_cleanup(void *arg)
{
	struct tmpl_ctx *const ctx = arg;

	/* Close output if needed */
	if (ctx->close_output)
		fclose(ctx->output);
	ctx->output = NULL;
	ctx->orig_output = NULL;
	ctx->flags = 0;
}

/*
 * Invoke a single template function.
 */
int
tmpl_execute_func(struct tmpl_ctx *ctx, FILE *output,
	char **errmsgp, int argc, char **argv, int flags)
{
	struct tmpl *t;
	char *input;
	FILE *sbuf;
	int rtn;
	int i;

	/* Avoid reentrant execution with the same context */
	if (ctx->output != NULL) {
		errno = EBUSY;
		return (-1);
	}

	/* Initialize error string */
	*errmsgp = NULL;

	/* Sanity */
	if (argc < 1) {
		errno = EINVAL;
		return (-1);
	}

	/* Create template input containing the function call */
	if ((sbuf = string_buf_output(TYPED_MEM_TEMP)) == NULL)
		return (-1);
	fprintf(sbuf, "@%s(", argv[0]);
	for (i = 1; i < argc; i++) {
		char *qarg;

		/* Enquote and add argument */
		if ((qarg = string_enquote(argv[i], TYPED_MEM_TEMP)) == NULL) {
			fclose(sbuf);
			return (-1);
		}
		fprintf(sbuf, "%s%s", i > 1 ? "," : "", qarg);
		FREE(TYPED_MEM_TEMP, qarg);
	}
	fprintf(sbuf, ")");
	if ((input = string_buf_content(sbuf, 1)) == NULL) {
		fclose(sbuf);
		return (-1);
	}
	fclose(sbuf);

	/* Create a new template from the input string */
	if ((sbuf = string_buf_input(input, strlen(input), 0)) == NULL) {
		FREE(TYPED_MEM_TEMP, input);
		return (-1);
	}
	t = tmpl_create(sbuf, NULL, TYPED_MEM_TEMP);
	fclose(sbuf);
	FREE(TYPED_MEM_TEMP, input);
	if (t == NULL)
		return (-1);

	/* Execute template */
	rtn = tmpl_execute(t, ctx, output, flags);

	/* Clean up */
	tmpl_destroy(&t);
	return (rtn);
}

/*
 * Execute elements.
 */
enum exec_rtn
_tmpl_execute_elems(struct tmpl_ctx *ctx,
	struct tmpl_elem *const elems, int first_elem, int nelems)
{
#if TMPL_DEBUG
	char indent[80];
#endif
	enum exec_rtn rtn;
	int i;

	/* Increase nesting */
	ctx->depth++;

	/* Check for infinite loop */
	if (ctx->depth >= INFINITE_LOOP) {
		pr_err(ctx, 0, "too much recursion in template execution");
		rtn = RTN_NORMAL;
		goto done;
	}

	/* Debug */
#if TMPL_DEBUG
	*indent = '\0';
	for (i = 1; i < ctx->depth; i++)
		strlcat(indent, "  ", sizeof(indent));
	TDBG("%sExecuting %d elems starting at %d\n",
	    indent _ nelems _ first_elem);
#endif

	/* Execute elements in order */
	for (i = first_elem; i < first_elem + nelems; i++) {
		struct tmpl_elem *const elem = &elems[i];

		TDBG("%s%4d: %s\n", indent _ i _ _tmpl_elemstr(&elems[i] _ i));

		/* Handle normal text */
		if (elem->text != NULL) {

			/* Optionally skip initial NL + whitespace */
			if (((elem->flags & TMPL_ELEM_NL_WHITE) != 0
			    && ctx->flags & TMPL_SKIP_NL_WHITE) != 0)
				continue;

			/* Output text */
			fwrite(elem->text, 1, elem->len, ctx->output);
			continue;
		}

		/* Handle function call */
		switch (elem->call.type) {
		case TY_NORMAL:
		    {
			char *errmsg = NULL;
			char *result;

			if ((result = _tmpl_invoke(ctx,
			    &errmsg, &elem->call)) == NULL) {
				pr_err(ctx, errno, errmsg);
				FREE(ctx->mtype, errmsg);
				break;
			}
			TDBG("%s      --> \"%s\"\n", indent _ result);
			(void)fputs(result, ctx->output);
			FREE(ctx->mtype, result);
			break;
		    }

		case TY_LOOP:
		    {
			struct loop_ctx this;
			char *errmsg = NULL;
			long count;
			char *eptr;
			char *x;

			if (!(x = evaluate_arg(ctx,
			    &errmsg, &elem->call.args[0]))) {
				pr_err(ctx, errno, errmsg);
				FREE(ctx->mtype, errmsg);
				i += elem->u.u_loop.endloop;
				break;
			}
			count = strtoul(x, &eptr, 10);
			if (*x == '\0' || *eptr != '\0'
			    || count < 0 || count == LONG_MAX) {
				char buf[32];

				snprintf(buf, sizeof(buf),
				    "invalid loop count \"%s\"", x);
				pr_err(ctx, 0, buf);
				FREE(ctx->mtype, x);
				i += elem->u.u_loop.endloop;
				break;
			}
			FREE(ctx->mtype, x);
			this.outer = ctx->loop;
			ctx->loop = &this;
			for (this.index = 0; this.index < count; this.index++) {
				rtn = _tmpl_execute_elems(ctx, elems,
				    i + 1, elem->u.u_loop.endloop - 1);
				if (rtn == RTN_BREAK)
					break;
				if (rtn == RTN_RETURN) {
					ctx->loop = this.outer;
					goto done;
				}
			}
			ctx->loop = this.outer;
			i += elem->u.u_loop.endloop;
			break;
		    }

		case TY_WHILE:
			while (1) {
				char *errmsg = NULL;
				int truth;
				char *x;

				if (!(x = evaluate_arg(ctx,
				    &errmsg, &elem->call.args[0]))) {
					pr_err(ctx, errno, errmsg);
					FREE(ctx->mtype, errmsg);
					i += elem->u.u_while.endwhile;
					break;
				}
				truth = _tmpl_true(x);
				FREE(ctx->mtype, x);
				if (!truth)
					break;
				rtn = _tmpl_execute_elems(ctx, elems, i + 1,
				    elem->u.u_while.endwhile - 1);
				if (rtn == RTN_BREAK)
					break;
				if (rtn == RTN_RETURN)
					goto done;
			}
			i += elem->u.u_while.endwhile;
			break;

		case TY_IF:
		case TY_ELIF:
		    {
			char *errmsg = NULL;
			int first = -1;
			int num = 0;
			int truth;
			char *x;

			if (!(x = evaluate_arg(ctx,
			    &errmsg, &elem->call.args[0]))) {
				pr_err(ctx, errno, errmsg);
				FREE(ctx->mtype, errmsg);
				i += elem->u.u_if.endif;
				break;
			}
			truth = _tmpl_true(x);
			FREE(ctx->mtype, x);
			if (truth) {
				first = i + 1;
				num = (elem->u.u_if.elsie != -1) ?
				    elem->u.u_if.elsie - 1 :
				    elem->u.u_if.endif - 1;
			} else if (elem->u.u_if.elsie != -1) {
				first = i + elem->u.u_if.elsie;
				num = elem->u.u_if.endif - elem->u.u_if.elsie;
			}
			if (first != -1) {
				rtn = _tmpl_execute_elems(ctx,
				    elems, first, num);
				if (rtn == RTN_BREAK
				    || rtn == RTN_RETURN
				    || rtn == RTN_CONTINUE)
					goto done;
			}
			i += elem->u.u_if.endif;
			break;
		    }

		case TY_DEFINE:
		    {
			char *errmsg = NULL;
			char *name;

			if (!(name = evaluate_arg(ctx,
			    &errmsg, &elem->call.args[0]))) {
				pr_err(ctx, errno, errmsg);
				FREE(ctx->mtype, errmsg);
				i += elem->u.u_define.enddef;
				break;
			}
#ifdef TMPL_DEBUG
			TDBG("%s%6sDefining function \"%s\" as %d elems"
			     " starting at %d\n", indent _ "" _ name _
			     elem->u.u_define.enddef - 1 _ i + 1);
#endif
			if (_tmpl_set_func(ctx, name, elems + i + 1,
			    elem->u.u_define.enddef - 1) == -1) {
				pr_err(ctx, errno, NULL);
				FREE(ctx->mtype, name);
				i += elem->u.u_define.enddef;
				break;
			}
			FREE(ctx->mtype, name);
			i += elem->u.u_define.enddef;
			break;
		    }

		case TY_ENDLOOP:
		case TY_ENDWHILE:
		case TY_ELSE:
		case TY_ENDIF:
		case TY_ENDDEF:
			break;

		case TY_BREAK:
			rtn = RTN_BREAK;
			goto done;

		case TY_CONTINUE:
			rtn = RTN_CONTINUE;
			goto done;

		case TY_RETURN:
			rtn = RTN_RETURN;
			goto done;

		default:
			assert(0);
		}
	}

	/* If we finished all the elements, that's a normal return */
	rtn = RTN_NORMAL;

done:
	/* Debug */
#if TMPL_DEBUG
	TDBG("%sDone, return is %s\n", indent _ tmpl_rtnstr(rtn));
#endif

	/* Decrease nesting */
	ctx->depth--;

	/* Done */
	return (rtn);
}

/*
 * Invoke a function and return the result. The function is
 * assumed to not be a special built-in (eg, "@if").
 */
char *
_tmpl_invoke(struct tmpl_ctx *ctx, char **errmsgp, const struct func_call *call)
{
	char **args_copy = NULL;
	char **args = NULL;
	char *r = NULL;
	int i;

	/* Evaluate arguments; first function argument is the function name */
	if ((args = MALLOC(ctx->mtype,
	    (1 + call->nargs) * sizeof(*args))) == NULL)
		return (NULL);
	memset(args, 0, (1 + call->nargs) * sizeof(*args));
	if ((args[0] = STRDUP(ctx->mtype, call->funcname)) == NULL)
		goto fail;
	for (i = 0; i < call->nargs; i++) {
		if ((args[i + 1] = evaluate_arg(ctx,
		    errmsgp, &call->args[i])) == NULL)
			goto fail;
	}

	/* Save a copy of argument pointers so handlers can modify them */
	if ((args_copy = MALLOC(ctx->mtype,
	    (1 + call->nargs) * sizeof(*args))) == NULL)
		return (NULL);
	memcpy(args_copy, args, (1 + call->nargs) * sizeof(*args));

	/* Invoke function, either run-time defined, built-in, or user */
	assert(call->type == TY_NORMAL);
	if ((i = _tmpl_find_func(ctx, call->funcname)) != -1) {
		r = tmpl_invoke_defined(ctx, &ctx->funcs[i],
		    errmsgp, 1 + call->nargs, args_copy);
	} else if (call->handler != NULL)
		r = (*call->handler)(ctx, errmsgp, 1 + call->nargs, args_copy);
	else
		r = (*ctx->handler)(ctx, errmsgp, 1 + call->nargs, args_copy);

fail:
	for (i = 0; i < 1 + call->nargs; i++)
		FREE(ctx->mtype, args[i]);
	FREE(ctx->mtype, args);
	FREE(ctx->mtype, args_copy);
	return (r);
}

/*
 * Invoke a run-time defined function and return the resulting
 * output as a string.
 */
static char *
tmpl_invoke_defined(struct tmpl_ctx *ctx, struct exec_func *func,
	char **errmsgp, int argc, char **argv)
{
	FILE *const output_save = ctx->output;
	char *rtn = NULL;
	int esave;

	/* Create string output buffer to capture output */
	if ((ctx->output = string_buf_output(ctx->mtype)) == NULL)
		goto fail;

	/* Invoke function using existing context */
	if (tmpl_invoke_defined2(ctx, func, errmsgp, argc, argv) == -1)
		goto fail;

	/* Return the resulting output as a string */
	rtn = string_buf_content(ctx->output, 1);

fail:
	/* Clean up and return */
	esave = errno;
	if (ctx->output != NULL)
		fclose(ctx->output);
	ctx->output = output_save;
	errno = esave;
	return (rtn);
}

/*
 * Invoke a runtime-defined function.
 */
static int
tmpl_invoke_defined2(struct tmpl_ctx *ctx,
	struct exec_func *func, char **errmsgp, int argc, char **argv)
{
	char **saved_vars;
	int rtn = 0;
	int i;

	/* Save variables argc and arg0, arg1, ... */
	if ((saved_vars = MALLOC(ctx->mtype,
	    (argc + 1) * sizeof(*saved_vars))) == NULL)
		return (-1);
	memset(saved_vars, 0, (argc + 1) * sizeof(*saved_vars));
	for (i = 0; i <= argc; i++) {
		char namebuf[32];
		int j;

		if (i == argc)
			strlcpy(namebuf, "argc", sizeof(namebuf));
		else
			snprintf(namebuf, sizeof(namebuf), "arg%u", i);
		if ((j = _tmpl_find_var(ctx, namebuf)) != -1) {
			if ((saved_vars[i] = STRDUP(ctx->mtype,
			    ctx->vars[j].value)) == NULL) {
				while (--i >= 0)
					FREE(ctx->mtype, saved_vars[i]);
				FREE(ctx->mtype, saved_vars);
				return (-1);
			}
		}
	}

	/* Set new values for those variables */
	for (i = 0; i <= argc; i++) {
		char namebuf[32], valbuf[32];
		char *argval;

		if (i == argc) {
			strlcpy(namebuf, "argc", sizeof(namebuf));
			snprintf(valbuf, sizeof(valbuf), "%u", argc);
			argval = valbuf;
		} else {
			snprintf(namebuf, sizeof(namebuf), "arg%u", i);
			argval = argv[i];
		}
		if (tmpl_ctx_set_var(ctx, namebuf, argval) == -1) {
			rtn = -1;
			goto fail;
		}
	}

	/* Execute function elements */
	_tmpl_execute_elems(ctx, func->elems, 0, func->num_elems);

fail:
	/* Restore saved variables */
	for (i = 0; i <= argc; i++) {
		char argname[32];
		int j;

		if (i == argc)
			strlcpy(argname, "argc", sizeof(argname));
		else
			snprintf(argname, sizeof(argname), "arg%u", i);
		if ((j = _tmpl_find_var(ctx, argname)) == -1) {
			FREE(ctx->mtype, saved_vars[i]);
			continue; /* we get here only in the 'goto fail' case */
		}
		if (saved_vars[i] != NULL) {
			FREE(ctx->mtype, ctx->vars[j].value);
			ctx->vars[j].value = saved_vars[i];
		} else {
			FREE(ctx->mtype, ctx->vars[j].name);
			FREE(ctx->mtype, ctx->vars[j].value);
			memmove(ctx->vars + j, ctx->vars + j + 1,
			    (--ctx->num_vars - j) * sizeof(*ctx->vars));
		}
	}
	FREE(ctx->mtype, saved_vars);
	return (rtn);
}

/*
 * Evaluate a function argument.
 */
static char *
evaluate_arg(struct tmpl_ctx *ctx, char **errmsgp, const struct func_arg *arg)
{
	if (arg->is_literal)
		return (STRDUP(ctx->mtype, arg->u.literal));
	else
		return (_tmpl_invoke(ctx, errmsgp, &arg->u.call));
}

/*
 * Output an error string.
 *
 * If "msg" is not NULL, use that, otherwise use strerror(error).
 */
static void
pr_err(struct tmpl_ctx *ctx, int error, const char *msg)
{
	const int errno_save = errno;
	char *cooked;

	/* Use strerror() error string if none more specific provided */
	if (msg == NULL)
		msg = strerror(errno);

	/* Format error string (if possible) and output it */
	if (ctx->errfmtr != NULL
	    && (cooked = (*ctx->errfmtr)(ctx, msg)) != NULL) {
		(void)fputs(cooked, ctx->output);
		FREE(ctx->mtype, cooked);
	} else
		(void)fputs(msg, ctx->output);
	errno = errno_save;
}

/*
 * Determine truth of a string.
 */
int
_tmpl_true(const char *s)
{
	char *eptr;
	u_long val;

	val = strtol(s, &eptr, 0);
	return (*s != '\0' && *eptr == '\0' && val != 0);
}

#if TMPL_DEBUG
static const char *
tmpl_rtnstr(enum exec_rtn rtn)
{
	switch (rtn) {
	case RTN_NORMAL:
		return ("NORMAL");
	case RTN_BREAK:
		return ("BREAK");
	case RTN_CONTINUE:
		return ("CONTINUE");
	case RTN_RETURN:
		return ("RETURN");
	default:
		assert(0);
	}
	return (NULL);
}
#endif

