
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_TMPL_TMPL_H_
#define _PDEL_TMPL_TMPL_H_

/*
 * This code supports a simple text template language. An input file
 * is scanned for special function tags. A function tag is an at sign
 * ('@') followed by a contiguous sequence of letters, digits, and
 * underscores, followed by parentheses containing zero or more
 * function arguments.
 *
 * Function arguments may be either other function tags (the argument
 * to the outer function is the result of the inner function invocation),
 * or constant literal strings in double quotes (the argument is the
 * value of the string). Note this means that all function arguments
 * begin with either an at sign or a double quote character. Function
 * arguments are enclosed in parentheses, separated by commas, and
 * may have whitespace around them.
 *
 * When processed, function tags result in a function invocation
 * which returns some text which is then substituted into the output
 * stream. Special control flow function tags control input processing
 * and return the empty string.
 *
 * Non-function tag text input is passed through unaltered.
 *
 * Constant literal strings (enclosed in double quotes) respect the
 * normal C backslash escapes.
 *
 * Built-in functions that take no arguments do not require parentheses,
 * but parentheses may be included for separation purposes.
 *
 * User-supplied functions may return NULL and set errno to indicate
 * an error, in which case the corresponding error string is substituted,
 * after being formatted by the caller-supplied error formatter.
 *
 * The predefined functions are:
 *
 *   @while(x) ... @endwhile
 *	The text in between is repeated as long as x is true
 *	(x is true if it is an integer whose value is non-zero).
 *
 *   @loop(numloops) ... @endloop
 *	The text in between is repeated 'numloops' times, which
 *	must be an integer value.
 *
 *   @loopindex(i)
 *	Returns the loop index (counting from zero) of the loop
 *	that is 'i' loops out from the innermost loop, else -1.
 *	If argument is omitted it is assumed to be zero.
 *
 *   @if(x) ... [ @elif ... [ @elif ... ] ] [ @else ... ] @endif
 *	Conditional execution depending on the truth value of x.
 *
 *   @define(name) ... @enddef
 *	Defines a run-time function. The text in between is executed
 *	whenever @name(...) is subsequently invoked. During this execution,
 *	the variables "argc" and "arg0", "arg1", ... are set to the
 *	function argument count and arguments, respectively (the function
 *	name itself is always the value of "arg0".
 *
 *   @break
 *	Break out of the nearest enclosing @loop or @while loop.
 *
 *   @continue
 *	Continue with the next iteration of the nearest enclosing
 *	@loop or @while loop.
 *
 *   @return
 *	Return from within a run-time function.
 *
 *   @equal(x,y)
 *	Returns "1" if x and y are identical, else "0".
 *
 *   @not(x)
 *	Returns "1" unless x is an integer whose value is non-zero.
 *
 *   @and(arg1, arg2, ...)
 *	Returns logical "and" of the truth values of the arguments.
 *
 *   @or(arg1, arg2, ...)
 *	Returns logical "or" of the truth values of the arguments.
 *
 *   @add(arg1, ...)
 *	Returns sum of the numerical values of the arguments.
 *
 *   @mul(arg1, ...)
 *	Returns product of the numerical values of the arguments.
 *
 *   @div(arg1, arg2)
 *	Returns the first argument divided by the second.
 *
 *   @mod(arg1, arg2)
 *	Returns the first argument modulo by the second.
 *
 *   @lt(arg1, arg2)
 *   @gt(arg1, arg2)
 *   @le(arg1, arg2)
 *   @ge(arg1, arg2)
 *	Returns "0" or "1" as arg1 is <, >, <=, or >= arg2.
 *
 *   @sub(arg1, ...)
 *	Returns subtraction of the numerical values of the second
 *	and subsequent arguments from the first.
 *
 *   @error(arg)
 *	Output the argument using the caller's error formatting.
 *
 *   @eval(arg)
 *	The argument itself is processed. Embedded function tags
 *	are processed normally, while the rest is passed unaltered.
 *
 *   @funcname(arg1, arg2, ...)
 *	Invoke the generic user-supplied function handler.
 *
 *   @@
 *	Expands to ``@''
 *
 *   @cat(arg1, arg2, ...)
 *	Returns concatentated arguments.
 *
 *   @set(var, value)
 *	Sets run-time variable 'var' to have value 'value'. Variables
 *	are global and live until tmpl_process() returns. Returns the
 *	empty string.
 *
 *   @get(var)
 *	Retrieves the value of run-time variable 'var'.
 *	Returns the empty string if 'var' is not set.
 *
 *   @urlencode(arg)
 *	Expands and URL-encodes the argument.
 *
 *   @htmlencode(arg)
 *	Expands and HTML-encodes the first argument.
 *
 *   @output(arg)
 *	Outputs the argument directly to the output stream. That is,
 *	if this function is invoked from within a user-defined function,
 *	the argument goes directly to the template output rather than
 *	being concatenated to the return value of the function.
 *
 *   @flush()
 *	Flushes the template output.
 *
 *   @foobar(...)
 *	If "foobar" is not defined as a run-time function, then this
 *	invokes the supplied handler for user-defined functions.
 *	Here "foobar" is any name other than the above function names,
 *	consisting of only letters, digits, and underscores.
 *
 */

struct tmpl;
struct tmpl_ctx;

#define TMPL_FUNCTION_CHARACTER		'@'

/***********************************************************************
			CALLBACK FUNCTION TYPES
***********************************************************************/

/*
 * Generic function handler type. Should return a malloc'd string (allocated
 * with the same memory type as passed to tmpl_ctx_create()), or else return
 * NULL and either set errno or set *errmsgp to point to a malloc'd string
 * (same memory type) containing an error message. *errmsgp will be NULL
 * initially.
 *
 * The first argument is the function name. Subsequent arguments are the
 * (evaluated) arguments passed to the function. Note that this means "argc"
 * is always >= 1. argv[argc] is NOT guaranteed to be NULL.
 */
typedef char	*tmpl_handler_t(struct tmpl_ctx *ctx, char **errmsgp,
			int argc, char **argv);

/*
 * Error formatter type. This should format the error message string
 * using whatever formatting is desired and return the result in a
 * malloc'd buffer (note: just returning 'errmsg' is wrong; use
 * strdup(3) instead). Return NULL and set errno if there is an error.
 * The "arg" parameter is the same "arg" passed to tmpl_ctx_create().
 */
typedef char	*tmpl_errfmtr_t(struct tmpl_ctx *ctx, const char *errmsg);

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

__BEGIN_DECLS

/*
 * Create template object by parsing "input". The input stream must
 * be seekable, and is not used again after this function returns.
 *
 * If num_errors != NULL, it will be set to the number of parse
 * errors encountered while parsing the input.
 *
 * Returns NULL and sets errno on failure.
 */
extern struct	tmpl *tmpl_create(FILE *input,
			int *num_errors, const char *mtype);

/*
 * Similar to tmpl_create(), but memory map the file so that its
 * contents don't have to be stored in heap memory. However, the
 * file contents must not change or else subsequent executions will
 * give garbled output.
 */
extern struct	tmpl *tmpl_create_mmap(const char *path,
			int *num_errors, const char *mtype);

/*
 * Destroy and free a template file created by tmpl_create().
 *
 * Upon return, *tmplp is set to NULL. If *tmplp is already NULL,
 * this does nothing.
 */
extern void	tmpl_destroy(struct tmpl **tmplp);

/*
 * Create a template execution context.
 *
 * For all functions that are not predefined, the function is looked up
 * in the 'userfuncs' array (which is not copied and so must remain
 * valid for the life of the exectution context). This array must be
 * sorted by name and terminated with an entry having a NULL name.
 * All userfuncs will have 'arg' passed as their first argument.
 *
 * All strings returned by "handler" MUST be dynamically allocated
 * using memory type 'mtype'.
 *
 * The 'errfmtr' is called to specially format any generated error
 * messages; if it is NULL, error messages are output as plain text.
 */
extern struct	tmpl_ctx *tmpl_ctx_create(void *arg, const char *mtype,
			tmpl_handler_t *handler, tmpl_errfmtr_t *errfmtr);

/*
 * Get context argument.
 */
extern void	*tmpl_ctx_get_arg(struct tmpl_ctx *ctx);

/*
 * Get context memory type.
 */
extern const	char *tmpl_ctx_get_mtype(struct tmpl_ctx *ctx);

/*
 * Execute a template and output the result to 'output'.
 *
 * If a system error is encountered, execution halts and -1
 * is returned with 'errno' set. Otherwise, zero is returned.
 *
 * Flags:
 *
 *   TMPL_SKIP_NL_WHITE		Skip inter-function text consisting only of
 *				one or more newlines followed by whitespace.
 */
extern int	tmpl_execute(struct tmpl *tmpl, struct tmpl_ctx *ctx,
			FILE *output, int flags);

#define TMPL_SKIP_NL_WHITE	0x0001
#define TMPL_BUILTIN_EXTRA	0x0002

/*
 * Destroy and free a template context created by tmpl_ctx_create().
 *
 * Upon return, *ctxp is set to NULL. If *ctxp is already NULL,
 * this does nothing.
 */
extern void	tmpl_ctx_destroy(struct tmpl_ctx **ctxp);

/*
 * Functions to get and set variables associated with a context.
 */
extern const	char *tmpl_ctx_get_var(struct tmpl_ctx *ctx, const char *name);
extern int	tmpl_ctx_set_var(struct tmpl_ctx *ctx,
			const char *name, const char *value);

/*
 * Invoke a template function and write the output to "output".
 *
 * Returns zero if successful, otherwise returns -1 and either
 * sets *errmsgp to NULL and errno to the error code, or else
 * sets *errmsgp to some appropriate error message.
 */
extern int	tmpl_execute_func(struct tmpl_ctx *ctx, FILE *output,
			char **errmsgp, int argc, char **argv,
			int flags);

/*
 * Reset a template context.
 * 
 * This undoes the result of any variable assignments and run-time
 * function definitions from a previous execution using the context.
 * That is, the context is returned to original state as returned by
 * tmpl_ctx_create().
 */
extern void	tmpl_ctx_reset(struct tmpl_ctx *ctx);

/*
 * Built-in handler for handling a fixed list of user functions.
 *
 * The 'userfuncs' array must be sorted lexicographically by name.
 */

/* Structure describing a user-supplied function */
struct tmpl_func {
	const char	*name;		/* function name, null to end list */
	u_int		min_args;	/* min # args (not counting name) */
	u_int		max_args;	/* max # args (not counting name) */
	tmpl_handler_t	*handler;	/* handler for function */
};

extern char	*tmpl_list_handler(struct tmpl_ctx *ctx,
			const struct tmpl_func *userfuncs, u_int uflen,
			char **errmsgp, int argc, char **argv);

__END_DECLS

#endif	/* _PDEL_TMPL_TMPL_H_ */
