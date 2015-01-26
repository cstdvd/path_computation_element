
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "tmpl_internal.h"
#ifdef __FreeBSD__
#include <machine/param.h>
#endif
#ifndef WIN32
#include <sys/termios.h>
#endif
#include <signal.h>

/* Not all platforms have TCSASOFT - we bit or (|) it in, so just use 0 */
#ifndef TCSASOFT
#define TCSASOFT 0
#endif

#define isidchar(c)	(isalnum(c) || (c) == '_')

/* Parsing */
static int	parse_function(struct tmpl *tmpl, FILE *input,
			struct func_call *call, int special_ok);
static int	parse_argument(struct tmpl *tmpl, FILE *input,
			struct func_arg *arg);
static int	set_text(struct tmpl *tmpl, FILE *input, int nl_white,
			struct tmpl_elem *elem, off_t start, off_t end);
static int	add_elem(struct tmpl *tmpl);

/* Semantic analysis */
static int	scan_elements(struct tmpl *tmpl,
			int start, int in_loop, int *num_errors);
static int	set_error(struct tmpl *tmpl, struct func_call *call,
			const char *fmt, ...) __printflike(3, 4);

/* Memory management */
static void	_tmpl_compact_func(const char *mtype,
			struct func_call *call, char *mem, size_t *bsize);
static void	_tmpl_compact_arg(const char *mtype,
			struct func_arg *arg, char *mem, size_t *bsize);

/* Built-in function handlers */
static tmpl_handler_t	add_func;
static tmpl_handler_t	addu_func;
static tmpl_handler_t	and_func;
static tmpl_handler_t	atsign_func;
static tmpl_handler_t	bitval_func;
static tmpl_handler_t	bitand_func;
static tmpl_handler_t	bitor_func;
static tmpl_handler_t	bitadd_func;
static tmpl_handler_t	bitsub_func;
static tmpl_handler_t	bitnot_func;
static tmpl_handler_t	cat_func;
static tmpl_handler_t	div_func;
static tmpl_handler_t	equal_func;
static tmpl_handler_t	error_func;
static tmpl_handler_t	eval_func;
static tmpl_handler_t	flush_func;
static tmpl_handler_t	ge_func;
static tmpl_handler_t	get_func;
static tmpl_handler_t	gt_func;
static tmpl_handler_t	htmlencode_func;
static tmpl_handler_t	input_func;
static tmpl_handler_t	invoke_func;
static tmpl_handler_t	le_func;
static tmpl_handler_t	loopindex_func;
static tmpl_handler_t	lt_func;
static tmpl_handler_t	mod_func;
static tmpl_handler_t	mul_func;
static tmpl_handler_t	not_func;
static tmpl_handler_t	or_func;
static tmpl_handler_t	output_func;
static tmpl_handler_t	set_func;
static tmpl_handler_t	sub_func;
static tmpl_handler_t	subu_func;
static tmpl_handler_t	urlencode_func;

#if TMPL_DEBUG
/* Debugging */
static char	*funcstr(const char *mtype, const struct func_call *call);
static char	*argstr(const char *mtype, const struct func_arg *arg);
static const	char *typestr(const struct tmpl_elem *elem);
#endif /* TMPL_DEBUG */

/*
 * Built-in functions
 */
struct builtin_info {
	const char	*name;
	enum func_type	type;
	tmpl_handler_t	*handler;
	int		min_args;
	int		max_args;
};

static const char tmpl_function_string[] = { TMPL_FUNCTION_CHARACTER, '\0' };

static const struct builtin_info builtin_funcs[] = {
	{ "loop",	TY_LOOP,	NULL,			1, 1	},
	{ "endloop",	TY_ENDLOOP,	NULL,			0, 0	},
	{ "while",	TY_WHILE,	NULL,			1, 1	},
	{ "endwhile",	TY_ENDWHILE,	NULL,			0, 0	},
	{ "if",		TY_IF,		NULL,			1, 1	},
	{ "elif",	TY_ELIF,	NULL,			1, 1	},
	{ "else",	TY_ELSE,	NULL,			0, 0	},
	{ "endif",	TY_ENDIF,	NULL,			0, 0	},
	{ "define",	TY_DEFINE,	NULL,			1, 1	},
	{ "enddef",	TY_ENDDEF,	NULL,			0, 0	},
	{ "continue",	TY_CONTINUE,	NULL,			0, 0	},
	{ "break",	TY_BREAK,	NULL,			0, 0	},
	{ "return",	TY_RETURN,	NULL,			0, 1	},
	{ "equal",	TY_NORMAL,	equal_func,		2, 2	},
	{ "not",	TY_NORMAL,	not_func,		1, 1	},
	{ "and",	TY_NORMAL,	and_func,		1, INT_MAX },
	{ "or",		TY_NORMAL,	or_func,		1, INT_MAX },
	{ "bitval",	TY_NORMAL,	bitval_func,		1, 1 },
	{ "bitand",	TY_NORMAL,	bitand_func,		2, 2 },
	{ "bitor",	TY_NORMAL,	bitor_func,		2, 2 },
	{ "bitadd",	TY_NORMAL,	bitadd_func,		2, 2 },
	{ "bitsub",	TY_NORMAL,	bitsub_func,		2, 2 },
	{ "bitnot",	TY_NORMAL,	bitnot_func,		1, 1 },
	{ "add",	TY_NORMAL,	add_func,		1, INT_MAX },
	{ "addu",	TY_NORMAL,	addu_func,		1, INT_MAX },
	{ "sub",	TY_NORMAL,	sub_func,		1, INT_MAX },
	{ "subu",	TY_NORMAL,	subu_func,		1, INT_MAX },
	{ "mul",	TY_NORMAL,	mul_func,		2, INT_MAX },
	{ "div",	TY_NORMAL,	div_func,		2, 2 },
	{ "mod",	TY_NORMAL,	mod_func,		2, 2 },
	{ "lt",		TY_NORMAL,	lt_func,		2, 2 },
	{ "gt",		TY_NORMAL,	gt_func,		2, 2 },
	{ "le",		TY_NORMAL,	le_func,		2, 2 },
	{ "ge",		TY_NORMAL,	ge_func,		2, 2 },
	{ "eval",	TY_NORMAL,	eval_func,		1, 1	},
	{ "invoke",	TY_NORMAL,	invoke_func,		0, 0	},
	{ "cat",	TY_NORMAL,	cat_func,		0, INT_MAX },
	{ "error",	TY_NORMAL,	error_func,		1, 1	},
	{ "loopindex",	TY_NORMAL,	loopindex_func,		0, 1	},
	{ "get",	TY_NORMAL,	get_func,		1, 1	},
	{ "set",	TY_NORMAL,	set_func,		2, 2	},
	{ "urlencode",	TY_NORMAL,	urlencode_func,		1, 1	},
	{ "htmlencode",	TY_NORMAL,	htmlencode_func,	1, 1	},
	{ "flush",	TY_NORMAL,	flush_func,		0, 0	},
	{ "input",	TY_NORMAL,	input_func,		0, 1	},
	{ "output",	TY_NORMAL,	output_func,		1, 1	},
	{ tmpl_function_string,
			TY_NORMAL,	atsign_func,		0, 0	},
	{ NULL,		0,		NULL,			0, 0	}
};

/************************************************************************
 *				PARSING					*
 ************************************************************************/

/* Cleanup info */
struct tmpl_parse_info {
	struct tmpl		*tmpl;
	struct func_call	call;
};

/* Internal functions */
static void	tmpl_parse_cleanup(void *arg);

/*
 * Parse an input stream.
 */
int
_tmpl_parse(struct tmpl *tmpl, FILE *input, int *num_errors)
{
	struct tmpl_parse_info info;
	struct tmpl_elem *elem = NULL;
	int nl_white = 0;
	off_t start = 0;
	int rtn = -1;
	size_t bsize;
	off_t pos;
	int ch;
#if TMPL_DEBUG
	int i;
#endif

	/* Set up cleanup in case of thread cancellation */
	info.tmpl = tmpl;
	memset(&info.call, 0, sizeof(info.call));
	pthread_cleanup_push(tmpl_parse_cleanup, &info);

	/* Save starting position */
	if ((pos = ftello(input)) == -1)
		goto fail;

	/* Read lines and parse input into elements */
	*num_errors = 0;
	while ((ch = getc(input)) != EOF) {
		off_t end_func;

		/* Create a new element if necessary */
		if (elem == NULL) {
			if (add_elem(tmpl) == -1)
				goto fail;
			elem = &tmpl->elems[tmpl->num_elems - 1];
			start = pos;
			nl_white = 1;
		}

		/* Look for the initial character of a function invocation */
		if (ch != TMPL_FUNCTION_CHARACTER) {
			if (nl_white && (!isspace(ch)
			    || (pos == start && ch != '\n')))
				nl_white = 0;
			pos++;				/* more normal text */
			continue;
		}

		/* Try to parse a function */
		if (parse_function(tmpl, input, &info.call, 1) == -1) {
			if (errno == EINVAL) {
				(*num_errors)++;
				nl_white = 0;
				pos++;
				continue;		/* ignore parse error */
			}
			goto fail;
		}

		/* Remember input position just past the end of the function */
		if ((end_func = ftello(input)) == -1) {
			_tmpl_free_call(tmpl->mtype, &info.call);
			goto fail;
		}

		/*
		 * We parsed a function. Terminate the previous text
		 * element (if not empty) and add the function element.
		 */
		if (start != pos) {
			if (set_text(tmpl, input,
			    nl_white, elem, start, pos) == -1) {
				_tmpl_free_call(tmpl->mtype, &info.call);
				goto fail;
			}
			if (add_elem(tmpl) == -1) {
				_tmpl_free_call(tmpl->mtype, &info.call);
				goto fail;
			}
			elem = &tmpl->elems[tmpl->num_elems - 1];
			nl_white = 1;
		}
		elem->call = info.call;
		memset(&info.call, 0, sizeof(info.call));

		/* Reset input position just past the end of the function */
		pos = end_func;
		if (fseeko(input, pos, SEEK_SET) == -1)
			goto fail;

		/* Force a new text element to start */
		elem = NULL;
	}

	/* Terminate last text element, if any */
	if (elem != NULL
	    && set_text(tmpl, input, nl_white, elem, start, pos) == -1)
		goto fail;

#if TMPL_DEBUG
	TDBG("Initial parse results: %d elements\n", tmpl->num_elems);
	for (i = 0; i < tmpl->num_elems; i++)
		TDBG("%4d: %s\n", i, _tmpl_elemstr(&tmpl->elems[i], i));
#endif

	/* Fill in semantic data */
	if (scan_elements(tmpl, -1, 0, num_errors) == -1)
		goto fail;

#if TMPL_DEBUG
	TDBG("After semantic check:\n");
	for (i = 0; i < tmpl->num_elems; i++)
		TDBG("%4d: %s\n", i, _tmpl_elemstr(&tmpl->elems[i], i));
#endif

	/* Compress all static information into a single memory block */
	bsize = tmpl->num_elems * sizeof(*tmpl->elems);
	_tmpl_compact_elems(tmpl->mtype,
	    tmpl->elems, tmpl->num_elems, NULL, &bsize);
	if ((tmpl->eblock = MALLOC(tmpl->mtype, bsize)) == NULL)
		goto fail;
	bsize = 0;
	_tmpl_compact(tmpl->mtype, tmpl->eblock, &tmpl->elems,
	    tmpl->num_elems * sizeof(*tmpl->elems), &bsize);
	_tmpl_compact_elems(tmpl->mtype, tmpl->elems,
	    tmpl->num_elems, tmpl->eblock, &bsize);

	/* Success */
	rtn = 0;

fail:;
	/* Done */
	pthread_cleanup_pop(0);
	return (rtn);
}

/*
 * Cleanup for _tmpl_parse()
 */
static void
tmpl_parse_cleanup(void *arg)
{
	struct tmpl_parse_info *const info = arg;

	_tmpl_free_call(info->tmpl->mtype, &info->call);
}

/*
 * Traverse parsed elements and size/move all allocated memory.
 */
void
_tmpl_compact_elems(const char *mtype, struct tmpl_elem *elems,
	int num_elems, char *mem, size_t *bsize)
{
	int i;

	for (i = 0; i < num_elems; i++) {
		struct tmpl_elem *const elem = &elems[i];

		if (elem->text != NULL) {
			if ((elem->flags & TMPL_ELEM_MMAP_TEXT) == 0) {
				_tmpl_compact(mtype, mem,
				    &elem->text, elem->len, bsize);
			}
		} else {
			*bsize = ALIGN(*bsize);
			_tmpl_compact_func(mtype, &elem->call, mem, bsize);
		}
	}
}

/*
 * Traverse function call and size/move all allocated memory.
 */
static void
_tmpl_compact_func(const char *mtype,
	struct func_call *call, char *mem, size_t *bsize)
{
	int i;

	/* Do call function name */
	_tmpl_compact(mtype, mem,
	    &call->funcname, strlen(call->funcname) + 1, bsize);

	/* Do call arguments array */
	*bsize = ALIGN(*bsize);
	_tmpl_compact(mtype, mem, &call->args,
	    call->nargs * sizeof(*call->args), bsize);

	/* Do call arguments */
	for (i = 0; i < call->nargs; i++)
		_tmpl_compact_arg(mtype, &call->args[i], mem, bsize);
}

/*
 * Traverse function call argument and size/move all allocated memory.
 */
static void
_tmpl_compact_arg(const char *mtype,
	struct func_arg *arg, char *mem, size_t *bsize)
{
	if (arg->is_literal) {
		_tmpl_compact(mtype, mem, &arg->u.literal,
		    strlen(arg->u.literal) + 1, bsize);
	} else
		_tmpl_compact_func(mtype, &arg->u.call, mem, bsize);
}

/*
 * Compact a region of memory
 */
void
_tmpl_compact(const char *mtype, char *mem,
	void *ptrp, size_t len, size_t *bsize)
{
	void **const pp = ptrp;

	if (mem != NULL) {
		memcpy(mem + *bsize, *pp, len);
		FREE(mtype, *pp);
		*pp = mem + *bsize;
	}
	*bsize += len;
}

/*
 * Finalize a text parse element.
 */
static int
set_text(struct tmpl *tmpl, FILE *input, int nl_white,
	struct tmpl_elem *elem, off_t start, off_t end)
{
	const u_int len = end - start;
	int ch;
	int i;

	assert(elem->flags == 0);
	if (tmpl->mmap_addr != NULL) {
		elem->text = (char *)tmpl->mmap_addr + start;
		elem->flags |= TMPL_ELEM_MMAP_TEXT;
	} else {
		if ((elem->text = MALLOC(tmpl->mtype, len)) == NULL)
			return (-1);
		if (fseeko(input, start, SEEK_SET) == -1)
			return (-1);
		for (i = 0; i < len; i++) {
			if ((ch = getc(input)) == EOF) {
				if (!ferror(input))
					errno = EIO;
				return (-1);
			}
			elem->text[i] = (char)ch;
		}
	}
	if (nl_white)
		elem->flags |= TMPL_ELEM_NL_WHITE;
	elem->len = len;
	return (0);
}

/*
 * Grow an array of elements by one. Secretly we allocate in
 * chunks of size 'chunk'.
 */
static int
add_elem(struct tmpl *tmpl)
{
	static const int chunk = 128;
	void *new_array;

	if (tmpl->num_elems % chunk != 0) {
		tmpl->num_elems++;
		return (0);
	}
	if ((new_array = REALLOC(tmpl->mtype, tmpl->elems,
	    (tmpl->num_elems + chunk) * sizeof(*tmpl->elems))) == NULL)
		return (-1);
	tmpl->elems = new_array;
	memset(&tmpl->elems[tmpl->num_elems], 0, chunk * sizeof(*tmpl->elems));
	tmpl->num_elems++;
	return (0);
}

/*
 * Parse a function call. The input stream is assumed to be pointing
 * just past the '@'. Upon return it will be pointing to the character
 * after the function call if successful, otherwise it will be reset
 * to where it started from.
 *
 * If errno is EINVAL upon return, then there was a parse error.
 */
static int
parse_function(struct tmpl *tmpl, FILE *input,
	struct func_call *call, int special_ok)
{
	const struct builtin_info *builtin;
	off_t start_pos;
	int errno_save;
	int noargs = 0;
	void *mem;
	int len;
	int ch;

	/* Save starting position */
	memset(call, 0, sizeof(*call));
	if ((start_pos = ftello(input)) == -1)
		return (-1);

	/* Get function name; special case for "@@" */
	if ((ch = getc(input)) == TMPL_FUNCTION_CHARACTER)
		len = 1;
	else {
		for (len = 0; ch != EOF && isidchar(ch); len++)
			ch = getc(input);
		if (len == 0)
			goto parse_err;
	}

	/* Re-scan function name into malloc'd buffer */
	if ((call->funcname = MALLOC(tmpl->mtype, len + 1)) == NULL)
		goto parse_err;
	if (fseeko(input, start_pos, SEEK_SET) == -1)
		goto sys_err;
	if (fread(call->funcname, 1, len, input) != len) {
		if (!ferror(input))
			goto parse_err;
		goto sys_err;
	}
	call->funcname[len] = '\0';

	/* Check for built-in function; some do not take arguments */
	for (builtin = builtin_funcs; builtin->name != NULL; builtin++) {
		if (strcmp(call->funcname, builtin->name) == 0) {
			if (builtin->min_args == 0)
				noargs = 1;
			break;
		}
	}
	ch = getc(input);
	if (noargs && ch != '(') {
		if (ch != EOF)
			ungetc(ch, input);
		goto got_args;
	}

	/* Find opening parenthesis */
	while (isspace(ch))
		ch = getc(input);
	if (ch != '(')
		goto parse_err;

	/* Parse function call arguments */
	while (1) {
		ch = getc(input);
		while (isspace(ch))			/* skip white space */
			ch = getc(input);
		if (ch == ')')				/* no more arguments */
			break;
		if (call->nargs > 0) {
			if (ch != ',')			/* eat comma */
				goto parse_err;
			ch = getc(input);
			while (isspace(ch))		/* skip white space */
				ch = getc(input);
		}
		ungetc(ch, input);
		if ((mem = REALLOC(tmpl->mtype, call->args,
		    (call->nargs + 1) * sizeof(*call->args))) == NULL)
			goto sys_err;
		call->args = mem;
		if (parse_argument(tmpl, input, &call->args[call->nargs]) == -1)
			goto sys_err;
		call->nargs++;
	}

got_args:
	/* Set up function call for this function */
	if (builtin->name != NULL) {
		if (call->nargs < builtin->min_args) {
			if (set_error(tmpl, call,
			    "at %s %d argument%s %s for \"%c%s\"",
			    "least", builtin->min_args,
			    builtin->min_args == 1 ? "" : "s",
			    "required", TMPL_FUNCTION_CHARACTER,
			    builtin->name) == -1)
				goto sys_err;
			return (0);
		}
		if (call->nargs > builtin->max_args) {
			if (set_error(tmpl, call,
			    "at %s %d argument%s %s for \"%c%s\"",
			    "most", builtin->max_args,
			    builtin->max_args == 1 ? "" : "s",
			    "allowed", TMPL_FUNCTION_CHARACTER,
			    builtin->name) == -1)
				goto sys_err;
			return (0);
		}
		if (!special_ok && builtin->handler == NULL) {
			if (set_error(tmpl, call, "illegal nested \"%c%s\"",
			    TMPL_FUNCTION_CHARACTER, builtin->name) == -1)
				goto sys_err;
			return (0);
		}
		call->type = builtin->type;
		call->handler = builtin->handler;
	} else {
		call->type = TY_NORMAL;
		call->handler = NULL;
	}
	return (0);

	/* Error */
parse_err:
	errno = EINVAL;
sys_err:
	_tmpl_free_call(tmpl->mtype, call);
	errno_save = errno;
	if (fseeko(input, start_pos, SEEK_SET) == -1)
		return (-1);
	errno = errno_save;
	return (-1);
}

/*
 * Parse a function argument. The input stream is assumed to be pointing
 * at the first non-white space character. Upon return it will be
 * pointing to the character after the argument if successful,
 * otherwise it will be reset to where it started.
 *
 * If errno is EINVAL upon return, then there was a parse error.
 */
static int
parse_argument(struct tmpl *tmpl, FILE *input, struct func_arg *arg)
{
	off_t start_pos;

	/* Save starting position */
	memset(arg, 0, sizeof(*arg));
	if ((start_pos = ftello(input)) == -1)
		return (-1);

	/* Parse argument */
	switch (getc(input)) {
	case '"':
		arg->is_literal = 1;
		if ((arg->u.literal = string_dequote(input,
		    tmpl->mtype)) == NULL)
			return (-1);
		break;
	case TMPL_FUNCTION_CHARACTER:
		arg->is_literal = 0;
		if (parse_function(tmpl, input, &arg->u.call, 0) == -1)
			return (-1);
		break;
	default:
		if (fseeko(input, start_pos, SEEK_SET) != -1)
			errno = EINVAL;
		return (-1);
	}
	return (0);
}

/************************************************************************
 *			SEMANTIC ANALYSIS				*
 ************************************************************************/

/*
 * Scan an element list. Returns -1 if error.
 */
static int
scan_elements(struct tmpl *tmpl, int start, int in_loop, int *num_errors)
{
	struct tmpl_elem *const elem = start == -1 ? NULL : &tmpl->elems[start];
	int ret = 0;
	int i;

	assert (elem == NULL || elem->call.type != TY_NORMAL);
	for (i = start + 1; i < tmpl->num_elems; i++) {
		struct tmpl_elem *const elem2 = &tmpl->elems[i];

		if (elem2->text != NULL)
			continue;
		switch (elem2->call.type) {
		case TY_NORMAL:
			break;
		case TY_LOOP:
			if ((i = scan_elements(tmpl, i, 1, num_errors)) == -1)
				return (-1);
			break;
		case TY_WHILE:
			if ((i = scan_elements(tmpl, i, 1, num_errors)) == -1)
				return (-1);
			break;
		case TY_IF:
			elem2->u.u_if.elsie = -1;
			if ((i = scan_elements(tmpl,
			    i, in_loop, num_errors)) == -1)
				return (-1);
			break;
		case TY_DEFINE:
			if ((i = scan_elements(tmpl, i, 0, num_errors)) == -1)
				return (-1);
			break;
		case TY_ENDLOOP:
			if (!elem || elem->call.type != TY_LOOP)
				goto unexpected;
			elem->u.u_loop.endloop = i - start;
			return (i);
		case TY_ENDWHILE:
			if (!elem || elem->call.type != TY_WHILE)
				goto unexpected;
			elem->u.u_while.endwhile = i - start;
			return (i);
		case TY_ELSE:
		case TY_ELIF:
			if (!elem
			    || (elem->call.type != TY_IF
			      && elem->call.type != TY_ELIF))
				goto unexpected;
			if (elem->u.u_if.elsie != -1)
				goto unexpected;
			elem->u.u_if.elsie = i - start;
			if (elem2->call.type == TY_ELIF) {
				elem2->u.u_if.elsie = -1;
				if ((i = scan_elements(tmpl,
				    i, in_loop, num_errors)) == -1)
					return (-1);
				if (tmpl->elems[i].call.type == TY_ENDIF) {
					elem->u.u_if.endif = i - start;
					return (i);
				}
			}
			break;
		case TY_ENDIF:
			if (!elem
			    || (elem->call.type != TY_IF
			      && elem->call.type != TY_ELIF
			      && elem->call.type != TY_ELSE))
				goto unexpected;
			elem->u.u_if.endif = i - start;
			return (i);
		case TY_ENDDEF:
			if (!elem || elem->call.type != TY_DEFINE)
				goto unexpected;
			elem->u.u_define.enddef = i - start;
			return (i);
		case TY_BREAK:
		case TY_CONTINUE:
			if (!in_loop)
				goto unexpected;
			break;
		case TY_RETURN:
			break;
		default:
			assert(0);
		}
		continue;
unexpected:
		{
			char *fname;

			(*num_errors)++;
			if ((fname = STRDUP(tmpl->mtype,
			    elem2->call.funcname)) == NULL)
				return (-1);
			if (set_error(tmpl, &elem2->call, "unexpected %c%s",
			    TMPL_FUNCTION_CHARACTER, fname) == -1) {
				FREE(tmpl->mtype, fname);
				return (-1);
			}
			FREE(tmpl->mtype, fname);
		}
	}

	/* We ran out of elements */
	if (elem == NULL)
		return (0);
	(*num_errors)++;
	switch (elem->call.type) {
	case TY_LOOP:
		ret = set_error(tmpl, &elem->call, "%cloop without %cendloop",
		    TMPL_FUNCTION_CHARACTER, TMPL_FUNCTION_CHARACTER);
		break;
	case TY_WHILE:
		ret = set_error(tmpl, &elem->call, "%cwhile without %cendwhile",
		    TMPL_FUNCTION_CHARACTER, TMPL_FUNCTION_CHARACTER);
		break;
	case TY_IF:
	case TY_ELSE:
	case TY_ELIF:
		ret = set_error(tmpl, &elem->call, "%cif without %cendif",
		    TMPL_FUNCTION_CHARACTER, TMPL_FUNCTION_CHARACTER);
		break;
	case TY_DEFINE:
		ret = set_error(tmpl, &elem->call, "%cdefine without %cenddef",
		    TMPL_FUNCTION_CHARACTER, TMPL_FUNCTION_CHARACTER);
		break;
	case TY_NORMAL:				/* set_error() already called */
		break;
	default:
		assert(0);
	}
	return (ret);
}

/************************************************************************
 *			ERROR HANDLING					*
 ************************************************************************/

/*
 * Set up a function call that prints an error.
 * Replaces the previuous call.
 */
static int
set_error(struct tmpl *tmpl, struct func_call *call, const char *fmt, ...)
{
	char *string;
	va_list args;
	int slen;

	_tmpl_free_call(tmpl->mtype, call);
	if ((call->funcname = STRDUP(tmpl->mtype, "error")) == NULL)
		return (-1);
	if ((call->args = MALLOC(tmpl->mtype, sizeof(*call->args))) == NULL) {
		_tmpl_free_call(tmpl->mtype, call);
		return (-1);
	}
	va_start(args, fmt);
	slen = vsnprintf(NULL, 0, fmt, args);		/* just get length */
	if ((string = MALLOC(tmpl->mtype, slen + 1)) != NULL)
		vsnprintf(string, slen + 1, fmt, args);
	va_end(args);
	if (string == NULL) {
		_tmpl_free_call(tmpl->mtype, call);
		return (-1);
	}
	call->nargs = 1;
	call->args[0].is_literal = 1;
	call->args[0].u.literal = string;
	call->handler = error_func;
	call->type = TY_NORMAL;
	return (0);
}

/************************************************************************
 *			BUILT-IN FUNCTIONS				*
 ************************************************************************/

/*
 * @error()
 */
static char *
error_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	*errmsgp = STRDUP(ctx->mtype, av[1]);
	return (NULL);
}

/*
 * @equal()
 */
static char *
equal_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	return (STRDUP(ctx->mtype, strcmp(av[1], av[2]) == 0 ? "1" : "0"));
}

/*
 * @not()
 */
static char *
not_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	return (STRDUP(ctx->mtype, _tmpl_true(av[1]) ? "0" : "1"));
}

/*
 * @and()
 */
static char *
and_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	int is_true = 1;
	int i;

	for (i = 1; i < ac; i++)
		is_true &= _tmpl_true(av[i]);
	return (STRDUP(ctx->mtype, is_true ? "1" : "0"));
}

/*
 * @or()
 */
static char *
or_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	int is_true = 0;
	int i;

	for (i = 1; i < ac; i++)
		is_true |= _tmpl_true(av[i]);
	return (STRDUP(ctx->mtype, is_true ? "1" : "0"));
}

/*
 * @bitval()
 */
static char *
bitval_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];
	long x;

	x = strtol(av[1], NULL, 0);
	x = (1L << (x));
	snprintf(buf, sizeof(buf), "%ld", x);
	return (STRDUP(ctx->mtype, buf));
}


/*
 * @bitand()
 */
static char *
bitand_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];
	long x, y;

	x = strtol(av[1], NULL, 0);
	y = strtol(av[2], NULL, 0);
	snprintf(buf, sizeof(buf), "%d", !(!(x & y)));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @bitor()
 */
static char *
bitor_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];
	long x, y;

	x = strtol(av[1], NULL, 0);
	y = strtol(av[2], NULL, 0);
	snprintf(buf, sizeof(buf), "%d", !(!(x | y)));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @bitadd()
 */
static char *
bitadd_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];
	long x, y;

	x = strtol(av[1], NULL, 0);
	y = strtol(av[2], NULL, 0);
	snprintf(buf, sizeof(buf), "%ld", x | y);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @bitsub()
 */
static char *
bitsub_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];
	long x, y;

	x = strtol(av[1], NULL, 0);
	y = strtol(av[2], NULL, 0);
	snprintf(buf, sizeof(buf), "%ld", x & ~(y));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @bitnot()
 */
static char *
bitnot_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];
	long x;

	x = strtol(av[1], NULL, 0);
	snprintf(buf, sizeof(buf), "%ld", ~x);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @add()
 */
static char *
add_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	long long sum = 0;
	char buf[32];
	int i;

	for (i = 1; i < ac; i++)
		sum += strtoll(av[i], NULL, 0);
	snprintf(buf, sizeof(buf), "%lld", sum);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @addu()
 */
static char *
addu_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	unsigned long long sum = 0;
	char buf[32];
	int i;

	for (i = 1; i < ac; i++)
		sum += strtoull(av[i], NULL, 0);
	snprintf(buf, sizeof(buf), "%lld", sum);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @sub()
 */
static char *
sub_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	long long result;
	char buf[32];
	int i;

	result = strtoll(av[1], NULL, 0);
	for (i = 2; i < ac; i++)
		result -= strtoll(av[i], NULL, 0);
	snprintf(buf, sizeof(buf), "%lld", result);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @subu()
 */
static char *
subu_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	unsigned long long result;
	char buf[32];
	int i;

	result = strtoll(av[1], NULL, 0);
	for (i = 2; i < ac; i++)
		result -= strtoull(av[i], NULL, 0);
	snprintf(buf, sizeof(buf), "%lld", result);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @mul()
 */
static char *
mul_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	long long product = 1;
	char buf[32];
	int i;

	for (i = 1; i < ac; i++)
		product *= strtoll(av[i], NULL, 0);
	snprintf(buf, sizeof(buf), "%lld", product);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @div()
 */
static char *
div_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	long long x, y;
	char buf[32];

	x = strtoll(av[1], NULL, 0);
	y = strtoll(av[2], NULL, 0);
	if (y == 0)
		return (STRDUP(ctx->mtype, "divide by zero"));
	snprintf(buf, sizeof(buf), "%lld", x / y);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @mod()
 */
static char *
mod_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	long long x, y;
	char buf[32];

	x = strtoll(av[1], NULL, 0);
	y = strtoll(av[2], NULL, 0);
	if (y == 0)
		return (STRDUP(ctx->mtype, "divide by zero"));
	snprintf(buf, sizeof(buf), "%lld", x % y);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @lt()
 */
static char *
lt_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%d",
	    strtoll(av[1], NULL, 0) < strtoll(av[2], NULL, 0));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @gt()
 */
static char *
gt_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%d",
	    strtoll(av[1], NULL, 0) > strtoll(av[2], NULL, 0));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @le()
 */
static char *
le_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%d",
	    strtoll(av[1], NULL, 0) <= strtoll(av[2], NULL, 0));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @ge()
 */
static char *
ge_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%d",
	    strtoll(av[1], NULL, 0) >= strtoll(av[2], NULL, 0));
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @eval()
 */
static char *
eval_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	FILE *const output_save = ctx->output;
	struct tmpl *tmpl = NULL;
	char *rtn = NULL;
	FILE *input = NULL;
	int esave;

	/* Create string output buffer to capture output */
	if ((ctx->output = string_buf_output(ctx->mtype)) == NULL)
		goto fail;

	/* Create new template using string argument as input */
	if ((input = string_buf_input(av[1], strlen(av[1]), 0)) == NULL)
		goto fail;
	if ((tmpl = tmpl_create(input, NULL, ctx->mtype)) == NULL)
		goto fail;

	/* Execute parsed template using existing context */
	_tmpl_execute_elems(ctx, tmpl->elems, 0, tmpl->num_elems);

	/* Get the resulting output as a string */
	rtn = string_buf_content(ctx->output, 1);

fail:
	/* Clean up and return */
	esave = errno;
	if (input != NULL)
		fclose(input);
	tmpl_destroy(&tmpl);
	if (ctx->output != NULL)
		fclose(ctx->output);
	ctx->output = output_save;
	errno = esave;
	return (rtn);
}

/*
 * @invoke()
 */
static char *
invoke_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct func_call call;
	char *rtn;
	int i;

	/* Initialize function call structure */
	memset(&call, 0, sizeof(call));
	call.type = TY_NORMAL;

	/* Get argument count */
	if ((i = _tmpl_find_var(ctx, "argc")) == -1
	    || (call.nargs = strtol(ctx->vars[i].value, NULL, 0)) <= 0) {
		ASPRINTF(ctx->mtype, errmsgp,
			 "%cinvoke(): \"argc\" must be greater than zero",
			 TMPL_FUNCTION_CHARACTER);
		return (NULL);
	}
	call.nargs--;

	/* Get function and arguments */
	if ((call.args = MALLOC(ctx->mtype,
	    call.nargs * sizeof(*call.args))) == NULL)
		return (NULL);
	for (i = 0; i <= call.nargs; i++) {
		const char *value;
		char name[16];
		int j;

		/* Get argN */
		snprintf(name, sizeof(name), "arg%d", i);
		value = ((j = _tmpl_find_var(ctx, name)) != -1) ?
		    ctx->vars[j].value : "";

		/* Set argN */
		if (i == 0)
			call.funcname = (char *)value;
		else {
			call.args[i - 1].is_literal = 1;
			call.args[i - 1].u.literal = (char *)value;
		}
	}

	/* Invoke function */
	rtn = _tmpl_invoke(ctx, errmsgp, &call);

	/* Done */
	FREE(ctx->mtype, call.args);
	return (rtn);
}

/*
 * @cat()
 */
static char *
cat_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	int len, slen;
	char *cat;
	int i;

	for (len = 0, i = 1; i < ac; i++)
		len += strlen(av[i]);
	if ((cat = MALLOC(ctx->mtype, len + 1)) == NULL)
		return (NULL);
	for (len = 0, i = 1; i < ac; i++) {
		slen = strlen(av[i]);
		memcpy(cat + len, av[i], slen);
		len += slen;
	}
	cat[len] = '\0';
	return (cat);
}

/*
 * @get()
 */
static char *
get_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	int i;

	if ((i = _tmpl_find_var(ctx, av[1])) == -1)
		return (STRDUP(ctx->mtype, ""));
	return (STRDUP(ctx->mtype, ctx->vars[i].value));
}

/*
 * @set()
 */
static char *
set_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	if (tmpl_ctx_set_var(ctx, av[1], av[2]) == -1)
		return (NULL);
	return (STRDUP(ctx->mtype, ""));
}

/*
 * @urlencode()
 */
static char *
urlencode_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char *s = av[1];
	char *enc;
	char *t;

	if ((enc = MALLOC(ctx->mtype, (strlen(s) * 3) + 1)) == NULL)
		return (NULL);
	for (t = enc; *s != '\0'; s++) {
		if (!isalnum(*s) && strchr("$-_.+!*'(),:@&=~", *s) == NULL)
			t += sprintf(t, "%%%02x", (u_char)*s);
		else
			*t++ = *s;
	}
	*t = '\0';
	return (enc);
}

/*
 * @htmlencode()
 */
static char *
htmlencode_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	char *s = av[1];
	char *esc;
	char *t;

	if ((esc = MALLOC(ctx->mtype, (strlen(s) * 6) + 1)) == NULL)
		return (NULL);
	for (t = esc; *s != '\0'; s++) {
		switch (*s) {
		case '<':
			t += sprintf(t, "&lt;");
			break;
		case '>':
			t += sprintf(t, "&gt;");
			break;
		case '"':
			t += sprintf(t, "&quot;");
			break;
		case '&':
			t += sprintf(t, "&amp;");
			break;
		default:
			if ((u_char)*s >= 0x7e)
				t += sprintf(t, "&#%d;", (u_char)*s);
			else
				*t++ = *s;
			break;
		}
	}
	*t = '\0';
	return (esc);
}

/*
 * @flush()
 */
static char *
flush_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	fflush(ctx->output);
	if (ctx->orig_output != ctx->output)
		fflush(ctx->orig_output);
	return (STRDUP(ctx->mtype, ""));
}

/*
 * @input()
 */
static char *
input_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	FILE *fp = ctx->orig_output;
	
	int quiet = 0;
	int unquiet = 0;
	int fd; 
	int gc;
#ifndef WIN32
	sigset_t sigs, old_sigs;
	struct termios term, oterm;
#endif
	char line[1024];
	size_t size = sizeof(line);
	int result = 0;

	if (ac == 2)
		quiet = strtol(av[1], NULL, 0);
	
	/* 
	 * Have to figure out if fp is valid input.
	 * Only way to do this portably is to try to getchar and then unget it.
	 */
	if (fp == NULL) {
		return(NULL);
	}
	fflush(fp);
	if (EOF == (gc = fgetc(fp))) {
		if (feof(fp)) {
			return(STRDUP(ctx->mtype, ""));
		}
		return(NULL);
	}
	ungetc(gc, fp);

	/* 
	 * Get the file descriptor for setting terminal attributes 
	 * If it's -1 it means we're dealing with a
	 * "virtual" FILE like string_fp or ssl_fp
	 * so don't bother with terminal stuff.
	 */
	fd = fileno(fp);
	
	/* Turn echo off if requested */
#ifndef WIN32
	if (fd >= 0 && quiet) {
	        sigemptyset(&sigs);
	        sigaddset(&sigs, SIGINT);
	        sigaddset(&sigs, SIGTSTP);
	        sigprocmask(SIG_BLOCK, &sigs, &old_sigs);

	        unquiet = 1;

		/* 
		 * No we no longer disable SIGQUIT so we can get the signal
		 * from the monitor - instead we clear VQUIT to be safe.
		 */

	        (void)tcgetattr(fd, &oterm);
	        term = oterm;
	        term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOKE);
		term.c_cc[VQUIT] = _POSIX_VDISABLE;
	        if (tcsetattr(fd, TCSADRAIN|TCSASOFT, &term) == -1)
			unquiet = 0;
	}
#endif
	/* Get the response */
	line[size-1] = '\0';
	if (fgets(line, size, fp) == NULL)
	        result = errno;

	/* Restore old terminal attributes */
	if (unquiet) {
	        fprintf(fp, "\n");
	        fflush(fp);
#ifndef WIN32
	        (void)tcsetattr(fd, TCSADRAIN|TCSASOFT, &oterm);
#endif
	}

	/* Restore old signal mask */
#ifndef WIN32
	if (fd >= 0 && quiet)
	        sigprocmask(SIG_SETMASK, &old_sigs, NULL);
#endif

	/* Trim trailing whitespace */
	while (strlen(line) > 0
	    && (line[strlen(line) - 1] == '\n'
	      || line[strlen(line) - 1] == '\r'))
	        line[strlen(line) - 1] = '\0';

	if (result != 0)
    		return (NULL);

	return (STRDUP(ctx->mtype, line));
}

/*
 * @output()
 */
static char *
output_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	fputs(av[1], ctx->orig_output);
	return (STRDUP(ctx->mtype, ""));
}

/*
 * @loopindex()
 */
static char *
loopindex_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct loop_ctx *loop;
	long level;
	char buf[64];
	char *eptr;
	int i;

	if (ac == 1)
		level = 0;
	else {
		level = strtol(av[1], &eptr, 0);
		if (*av[1] == '\0' || *eptr != '\0'
		    || level < 0 || level == LONG_MAX)
			return (STRDUP(ctx->mtype, "-1"));
	}
	for (i = 0, loop = ctx->loop;
	    i < level && loop != NULL;
	    i++, loop = loop->outer);
	if (loop == NULL) {
		snprintf(buf, sizeof(buf), "%c%s called not within any loop",
		    TMPL_FUNCTION_CHARACTER, av[0]);
		*errmsgp = STRDUP(ctx->mtype, buf);
		return (NULL);
	}
	snprintf(buf, sizeof(buf), "%d", loop->index);
	return (STRDUP(ctx->mtype, buf));
}

/*
 * @@
 */
static char *
atsign_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	return (STRDUP(ctx->mtype, tmpl_function_string));
}

/************************************************************************
 *				DEBUGGING				*
 ************************************************************************/

#if TMPL_DEBUG

#define BUFSIZE		128

/*
 * Return a text version of a template element.
 */
const char *
_tmpl_elemstr(const struct tmpl_elem *elem, int index)
{
	static char buf[BUFSIZE];
	char *s;

	snprintf(buf, sizeof(buf), "at 0x%04lx-0x%04lx type=%s",
	    (u_long)elem->start, (u_long)elem->start + elem->len,
	    typestr(elem));
	if (elem->text != NULL) {
		int i;

		strlcat(buf, " text=\"", sizeof(buf));
		for (i = 0; i < elem->len; i++) {
			switch (elem->text[i]) {
			case '\n':
				strlcat(buf, "\\n", sizeof(buf));
				break;
			case '\r':
				strlcat(buf, "\\r", sizeof(buf));
				break;
			case '\t':
				strlcat(buf, "\\t", sizeof(buf));
				break;
			default:
				if (isprint(elem->text[i])) {
					snprintf(buf + strlen(buf),
					    sizeof(buf) - strlen(buf),
					    "%c", elem->text[i]);
				} else {
					snprintf(buf + strlen(buf),
					    sizeof(buf) - strlen(buf),
					    "\\x%02x", (u_char)elem->text[i]);
				}
			}
		}
		strlcat(buf, "\"", sizeof(buf));
		return (buf);
	}
	s = funcstr(TYPED_MEM_TEMP, &elem->call);
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s", s);
	FREE(TYPED_MEM_TEMP, s);
	switch (elem->call.type) {
	case TY_LOOP:
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " end=%d", index + elem->u.u_loop.endloop);
		break;
	case TY_IF:
	case TY_ELIF:
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " else=%d endif=%d",
		    index + elem->u.u_if.elsie, index + elem->u.u_if.endif);
		break;
	case TY_WHILE:
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " end=%d", index + elem->u.u_while.endwhile);
		break;
	case TY_DEFINE:
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " end=%d", index + elem->u.u_define.enddef);
		break;
	default:
		break;
	}
	return (buf);
}

/*
 * Returns a malloc'd string
 */
static char *
funcstr(const char *mtype, const struct func_call *call)
{
	char buf[BUFSIZE];
	char *s;
	int i;

	snprintf(buf, sizeof(buf), "%c%s(",
	    TMPL_FUNCTION_CHARACTER, call->funcname);
	for (i = 0; i < call->nargs; i++) {
		s = argstr(mtype, &call->args[i]);
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%s%s", s, (i < call->nargs - 1) ? ", " : "");
		FREE(mtype, s);
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ")");
	return (STRDUP(mtype, buf));
}

/*
 * Returns a malloc'd string
 */
static char *
argstr(const char *mtype, const struct func_arg *arg)
{
	char buf[BUFSIZE];

	if (arg->is_literal) {
		snprintf(buf, sizeof(buf), "\"%s\"", arg->u.literal);
		return (STRDUP(mtype, buf));
	} else
		return (funcstr(mtype, &arg->u.call));
}

static const char *
typestr(const struct tmpl_elem *elem)
{
	if (elem->text != NULL)
		return ("TEXT");
	switch (elem->call.type) {
	case TY_NORMAL:	
		return (elem->call.handler != NULL ? "BUILTIN" : "USER");
	case TY_LOOP:		return ("LOOP");
	case TY_ENDLOOP:	return ("ENDLOOP");
	case TY_WHILE:		return ("WHILE");
	case TY_ENDWHILE:	return ("ENDWHILE");
	case TY_IF:		return ("IF");
	case TY_ELSE:		return ("ELSE");
	case TY_ELIF:		return ("ELIF");
	case TY_ENDIF:		return ("ENDIF");
	case TY_DEFINE:		return ("DEFINE");
	case TY_ENDDEF:		return ("ENDDEF");
	case TY_BREAK:		return ("BREAK");
	case TY_CONTINUE:	return ("CONTINUE");
	case TY_RETURN:		return ("RETURN");
	}
	return ("???");
}
#endif /* TMPL_DEBUG */
