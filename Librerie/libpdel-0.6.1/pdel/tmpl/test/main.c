
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <termios.h>
#include <limits.h>
#include <errno.h>
#include <err.h>

#include <pdel/structs/structs.h>
#include <pdel/structs/type/array.h>
#include <pdel/tmpl/tmpl.h>
#include <pdel/util/typed_mem.h>

#define TMPL_MEM_TYPE	"tmpl_test.tmpl"
#define CTX_MEM_TYPE	"tmpl_test.ctx"

#define EOF_STRING	"<<EOF>>"

#ifndef TCSASOFT
#define TCSASOFT	0
#endif

struct private_info {
	FILE	*input;
};

/*
 * Internal functions
 */
static tmpl_handler_t	readfile_func;
static tmpl_handler_t	display_func;
static tmpl_handler_t	error_func;
static tmpl_handler_t	input_func;

static tmpl_handler_t	handler;
static tmpl_errfmtr_t	errfmtr;

/*
 * Internal variables
 */
static struct	tmpl_func function_list[] = {
	{ "display", 		0, INT_MAX,	display_func	},
	{ "error_test",		0, INT_MAX,	error_func	},
	{ "func1", 		0, INT_MAX,	display_func	},
	{ "func2", 		0, INT_MAX,	display_func	},
	{ "func3", 		0, INT_MAX,	display_func	},
	{ "input", 		0, 1,		input_func	},
	{ "readfile", 		1, 1,		readfile_func	},
};

int
main(int ac, char **av)
{
	int flags = TMPL_SKIP_NL_WHITE;
	struct private_info priv;
	struct tmpl_ctx *ctx;
	struct tmpl *tmpl;
	int show_stats = 0;
	int num_errors;
	char *errmsg;
	FILE *fp;
	int ch;

	/* Parse command line arguments */
	while ((ch = getopt(ac, av, "ns")) != -1) {
		switch (ch) {
		case 'n':
			flags &= ~TMPL_SKIP_NL_WHITE;
			break;
		case 's':
			show_stats = 1;
			break;
		default:
			goto usage;
		}
	}
	ac -= optind;
	av += optind;

	if (ac < 1)
usage:		errx(1, "usage: tmpl [-n] filename [ func arg1 arg2 ...]");

	/* Enable typed memory */
	if (typed_mem_enable() == -1)
		err(1, "typed_mem_enable");

	/* Read input file to create template */
	if ((fp = fopen(av[0], "r")) == NULL)
		err(1, "%s", av[0]);
	if ((tmpl = tmpl_create(fp, &num_errors, TMPL_MEM_TYPE)) == NULL)
		err(1, "tmpl_create: %s", av[0]);
	fclose(fp);
	if (num_errors != 0)
		fprintf(stderr, "There were %d parse errors.\n", num_errors);

	/* Create template context */
	memset(&priv, 0, sizeof(priv));
	priv.input = stdin;
	if ((ctx = tmpl_ctx_create(&priv,
	    CTX_MEM_TYPE, handler, errfmtr)) == NULL)
		err(1, "tmpl_ctx_create");

	/* Execute template */
	if (tmpl_execute(tmpl, ctx, stdout, flags) == -1)
		err(1, "tmpl_execute");

	/* Execute user defined function */
	ac--;
	av++;
	if (ac > 0 && tmpl_execute_func(ctx,
	    stdout, &errmsg, ac, av, 0) == -1) {
		errx(1, "tmpl_execute_func: %s",
		    errmsg ? errmsg : strerror(errno));
	}

	/* Free template and context */
	tmpl_ctx_destroy(&ctx);
	tmpl_destroy(&tmpl);

	/* Done */
	if (show_stats)
		typed_mem_dump(stderr);
	return (0);
}

static char *
handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	return (tmpl_list_handler(ctx, function_list,
	    sizeof(function_list) / sizeof(*function_list), errmsgp, ac, av));
}

static char *
errfmtr(struct tmpl_ctx *ctx, const char *errmsg)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char *string;

	ASPRINTF(mtype, &string, "[ ERROR: %s ]", errmsg);
	return (string);
}

static char *
error_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	*errmsgp = STRDUP(mtype, "Sample error message here");
	return (NULL);
}

static char *
display_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[1024];
	int i;

	/* Normal handler output */
	snprintf(buf, sizeof(buf), "[ %s(", av[0]);
	for (i = 1; i < ac; i++) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%s%s", av[i], (i == ac - 1) ? "" : ", ");
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ") ]");
	return (STRDUP(mtype, buf));
}

static char *
readfile_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	int errno_save;
	char *s, *t;
	FILE *fp;
	int slen;
	int nr;

	/* Read file into memory */
	if ((fp = fopen(av[1], "r")) == NULL)
		return (NULL);
	for (s = NULL, slen = 0; 1; slen += nr) {
		if ((t = REALLOC(mtype, s, slen + 1024)) == NULL) {
			errno_save = errno;
			FREE(mtype, s);
			s = NULL;
			errno = errno_save;
			break;
		}
		s = t;
		if ((nr = fread(s + slen, 1, 1024, fp)) == 0) {
			s[slen] = '\0';
			break;
		}
	}

	/* Close file and return string */
	errno_save = errno;
	fclose(fp);
	errno = errno_save;
	return (s);
}

/*
 * @input()
 *
 * Reads one line of input from the input stream and returns it.
 * If an optional first argument is supplied and its numeric value
 * is non-zero, then the input is assumed to be a terminal device
 * and echo will be disabled (useful for inputting passwords).
 *
 * If no input stream was provided, or EOF is detected, an empty
 * string is returned and "input_eof" is set to "1" (otherwise it
 * will be set to "0").
 */
static char *
input_func(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const struct private_info *const priv = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	int quiet = (ac > 1 && strtol(av[1], NULL, 0) != 0);
	struct termios term, oterm;
	char *rtn = NULL;
	char buf[1024];

	/* If no input supplied, act as if EOF */
	if (priv->input == NULL) {
		quiet = 0;
		if (tmpl_ctx_set_var(ctx, "input_eof", "1"))
			goto fail;
		rtn = STRDUP(mtype, "");
		goto fail;
	}

	/* Turn echo off if requested */
	if (quiet) {
	        if (tcgetattr(fileno(priv->input), &oterm) == -1)
			quiet = 0;
		else {
			term = oterm;
			term.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHOKE);
			if (tcsetattr(fileno(priv->input),
			    TCSADRAIN|TCSASOFT, &term) == -1)
				quiet = 0;
		}
	}

	/* Read one line of input */
	if (tmpl_ctx_set_var(ctx, "input_eof", "0"))
		goto fail;
	if (fgets(buf, sizeof(buf), priv->input) == NULL) {
		if (ferror(priv->input))
			goto fail;
		if (tmpl_ctx_set_var(ctx, "input_eof", "1"))
			goto fail;
		rtn = STRDUP(mtype, "");
		goto fail;
	}

	/* Trim trailing newline */
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	/* Copy it to use the right memory type */
	if ((rtn = STRDUP(mtype, buf)) == NULL)
		goto fail;

fail:
	/* Clean up and return */
	if (quiet)
	        tcsetattr(fileno(priv->input), TCSADRAIN|TCSASOFT, &oterm);
	return (rtn);
}

