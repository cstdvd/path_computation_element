
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <paths.h>
#include <unistd.h>
#include <pthread.h>

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif
#include "pdel/pd_mem.h"

#include "structs/structs.h"
#include "structs/type/array.h"
#include "tmpl/tmpl.h"
#include "io/string_fp.h"
#include "util/typed_mem.h"
#include "util/string_quote.h"

#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

#ifndef TMPL_DEBUG
#define TMPL_DEBUG	0
#endif
#if TMPL_DEBUG != 0
#include <private/debug.h>
#ifdef PD_VA_MACRO_GNU
#define TDBG(fmt, args...) DBG(PDEL_DEBUG_TMPL, fmt, ## args)
#elif defined(PD_VA_MACRO_C99)
#define TDBG(fmt, args...) DBG(PDEL_DEBUG_TMPL, fmt, ## __VA_ARGS__)
#elif defined(PD_VA_MACRO_MSVC)
#define TDBG(fmt, ...) DBG(PDEL_DEBUG_TMPL, fmt, __VA_ARGS__)
#else /* workaround for compilers without variadic macros: last resort */
#define TDBG(fmt, args) DBG(PDEL_DEBUG_TMPL, fmt, args)
#endif
#else

#if defined(PD_VA_MACRO_GNU)
#define TDBG(fmt, args...)	do { } while (0)
#elif defined(PD_VA_MACRO_C99)
#define TDBG(fmt, ...)	do { } while (0)
#elif defined(PD_VA_MACRO_MSVC)
#define TDBG(fmt, ...)	do { } while (0)
#else /* workaround for compilers without variadic macros: last resort */
#define TDBG(fmt, args)	do { } while (0)
#endif

#endif

/* How much nesting is too much */
#define INFINITE_LOOP		128

/* Function types */
enum func_type {
	TY_NORMAL,
	TY_LOOP,
	TY_ENDLOOP,
	TY_WHILE,
	TY_ENDWHILE,
	TY_IF,
	TY_ELIF,
	TY_ELSE,
	TY_ENDIF,
	TY_DEFINE,
	TY_ENDDEF,
	TY_BREAK,
	TY_CONTINUE,
	TY_RETURN
};

/* Return values from _tmpl_execute_elems() */
enum exec_rtn {
	RTN_NORMAL,
	RTN_BREAK,
	RTN_CONTINUE,
	RTN_RETURN
};

/* Template element flags */
#define TMPL_ELEM_NL_WHITE	0x0001			/* text is nl+space */
#define TMPL_ELEM_MMAP_TEXT	0x0002			/* text is mmap()'d */

/* Function call */
struct func_call {
	enum func_type	type;				/* type of function */
	char		*funcname;			/* name of function */
	int		nargs;				/* number of args */
	struct func_arg	*args;				/* function arguments */
	tmpl_handler_t	*handler;			/* built-in: handler */
};

/* Function argument */
struct func_arg {
	u_char		is_literal;			/* literal vs. func */
	union {
	    struct func_call	call;			/* function call */
	    char 		*literal;		/* literal string */
	}		u;
};

/* This represents one parsed chunk of the template */
struct tmpl_elem {

	/* Lexical information */
	char			*text;		/* text, or NULL if function */
	u_int			len;		/* length of text */
	u_int16_t		flags;		/* element flags */

	/* Semantic information */
	struct func_call	call;		/* function and arguments */
	union {
	    struct {
		int			endloop;/* endloop element */
	    }			u_loop;		/* TY_LOOP */
	    struct {
		int			endwhile;/* endwhile element */
	    }			u_while;	/* TY_WHILE */
	    struct {
		int			elsie;	/* else element, or -1 */
		int			endif;	/* endif element */
	    }			u_if;		/* TY_IF */
	    struct {
		int			enddef;	/* enddef element */
	    }			u_define;	/* TY_DEFINE */
	}		u;
};

/* Run-time variables */
struct exec_var {
	char	*name;				/* variable name */
	char	*value;				/* variable value */
};

/* Run-time functions */
struct exec_func {
	char			*name;		/* function name */
	int			num_elems;	/* number of elements */
	struct tmpl_elem	*elems;		/* function elements */
	void			*eblock;	/* unified memory block */
};

/* Looping info */
struct loop_ctx {
	u_int			index;		/* current loop index */
	struct loop_ctx		*outer;		/* enclosing loop info */
};

/* Parsed template file */
struct tmpl {
	pd_mmap			mh;		/* pd_mmap HANDLE, or NULL */
	void			*mmap_addr;	/* "" address, or NULL	*/
	size_t			mmap_len;	/* "" length		*/
	int			num_elems;	/* number of parsed elements */
	struct tmpl_elem	*elems;		/* parsed template elements */
	void			*eblock;	/* unified memory block */
	char			*mtype;		/* memory type */
	char			mtype_buf[TYPED_MEM_TYPELEN];
};

/* Template context */
struct tmpl_ctx {
	FILE			*output;	/* current output */
	FILE			*orig_output;	/* original output */
	int			flags;		/* flags */
	u_char			close_output;	/* close 'output' when done */
	int			depth;		/* execution nesting level */
	void			*arg;		/* user function cookie */
	tmpl_handler_t		*handler;	/* user function handler */
	tmpl_errfmtr_t		*errfmtr;	/* user error formatter */
	struct loop_ctx		*loop;		/* innermost loop, if any */
	int			num_vars;	/* number of variables */
	struct exec_var		*vars;		/* runtime variables */
	int			num_funcs;	/* number of functions */
	struct exec_func	*funcs;		/* runtime functions */
	char			*mtype;		/* memory type */
	char			mtype_buf[TYPED_MEM_TYPELEN];
};

/*
 * Functions
 */

__BEGIN_DECLS

/* Parsing */
extern int	_tmpl_parse(struct tmpl *tmpl, FILE *input, int *num_errors);

/* Variable and function handling */
extern int	_tmpl_find_var(struct tmpl_ctx *ctx, const char *name);
extern int	_tmpl_find_func(struct tmpl_ctx *ctx, const char *name);
extern int	_tmpl_set_func(struct tmpl_ctx *ctx, const char *name,
			const struct tmpl_elem *elems, int count);

/* Memory management */
extern void	_tmpl_compact_elems(const char *mtype,
			struct tmpl_elem *elems, int num_elems,
			char *mem, size_t *bsize);
extern void	_tmpl_compact(const char *mtype, char *mem,
			void *ptrp, size_t len, size_t *bsize);
extern void	_tmpl_free_func(struct tmpl_ctx *ctx, struct exec_func *func);
extern void	_tmpl_free_elems(const char *mtype, void *eblock,
			struct tmpl_elem *elems, int num);
extern void	_tmpl_free_call(const char *mtype, struct func_call *call);
extern void	_tmpl_free_arg(const char *mtype, struct func_arg *arg);

/* Template execution */
extern enum	exec_rtn _tmpl_execute_elems(struct tmpl_ctx *ctx,
			struct tmpl_elem *const elems, int first_elem, int last_elem);
extern char	*_tmpl_invoke(struct tmpl_ctx *ctx,
			char **errmsgp, const struct func_call *call);

/* Truth determination */
extern int	_tmpl_true(const char *s);

#if TMPL_DEBUG
/* Debugging */
extern const	char *_tmpl_elemstr(const struct tmpl_elem *elem, int index);
#endif

__END_DECLS

