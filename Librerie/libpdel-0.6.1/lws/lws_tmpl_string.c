
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"
#include "lws_tmpl.h"
#include "lws_tmpl_string.h"

/***********************************************************************
			STRING TMPL FUNCTIONS
***********************************************************************/

/* Our user-defined template functions */
static tmpl_handler_t	lws_tf_length;
static tmpl_handler_t	lws_tf_indexof;
static tmpl_handler_t	lws_tf_substring;
static tmpl_handler_t	lws_tf_strftime;
static tmpl_handler_t	lws_tf_regex_match;
static tmpl_handler_t	lws_tf_regex_escape;

/* User-defined template function descriptor list */
const struct lws_tmpl_func lws_tmpl_string_functions[] = {
    LWS_TMPL_FUNC(length, 1, 1, "string",
	"Returns the length of $1."),
    LWS_TMPL_FUNC(indexof, 2, 2, "string:substring",
	"Returns the position of $2 in $1, or \"-1\" if $2 does not"
"\n"	"occur in $1."),
    LWS_TMPL_FUNC(substring, 2, 3, "string:startpos:endpos",
	"Returns the substring of $1 starting at position $2 and ending"
"\n"	"at position $3, or at the end of $1 if $3 is omitted."),
    LWS_TMPL_FUNC(strftime, 2, 2, "time:format",
	"Formats the absolute time value $1 according to the $2 string"
"\n"	"a la <code>strftime(3)</code>."),
    LWS_TMPL_FUNC(regex_match, 2, 4, "pattern:input:substring:icase",
	"Match $2 against the extended regular expression $1 (see"
"\n"	"<code>re_format(7)</code>) and return $2 if it matches, otherwise"
"\n"	"return the empty string.  If an optional $3 index is supplied,"
"\n"	"only return that substring if $2 matches. Matching is case-sensitive,"
"\n"	"unless $4 is supplied and is non-zero."),
    LWS_TMPL_FUNC(regex_escape, 1, 1, "string",
	"Returns $1 after all characters that are significant in an"
"\n"	"extended regular expression (see <code>re_format(7)</code>)"
"\n"	"have been escaped with backslashes."),
    { { NULL } }
};

/*
 * Get string length.
 *
 * Usage: @length(string)
 */
static char *
lws_tf_length(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[32];

	snprintf(buf, sizeof(buf), "%d", strlen(av[1]));
	return (STRDUP(mtype, buf));
}

/*
 * Get position of substring within larger string, or -1 if not found.
 *
 * Usage: @indexof(string, substring)
 */
static char *
lws_tf_indexof(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[32];
	char *s;
	int i;

	i = ((s = strstr(av[1], av[2])) == NULL) ? -1 : (s - av[1]);
	snprintf(buf, sizeof(buf), "%d", i);
	return (STRDUP(mtype, buf));
}

/*
 * Get a substring of a string.
 *
 * Usage: @substring(string, startpos [, endpos])
 */
static char *
lws_tf_substring(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	size_t len = strlen(av[1]);
	u_long start;
	u_long end;
	char *s;

	/* Get start and end values, clipping to sane values */
	start = strtoul(av[2], NULL, 10);
	if (start > len)
		start = len;
	end = (ac >= 4) ? strtoul(av[3], NULL, 10) : INT_MAX;
	if (end > len)
		end = len;
	if (end < start)
		end = start;

	/* Return substring */
	len = end - start;
	if ((s = MALLOC(mtype, len + 1)) == NULL)
		return (NULL);
	memcpy(s, av[1] + start, len);
	s[len] = '\0';
	return (s);
}

/*
 * Format a time value a la strftime(3).
 *
 * Usage: @strftime(time, format)
 */
static char *
lws_tf_strftime(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[128];
	time_t when;
	char *eptr;

	/* Get time in seconds since the epoch */
	when = (time_t)strtoul(av[1], &eptr, 10);
	if (eptr == av[1] || *eptr != '\0') {
		errno = EINVAL;
		return (NULL);
	}

	/* Format time */
	strftime(buf, sizeof(buf), av[2], localtime(&when));

	/* Done */
	return (STRDUP(mtype, buf));
}

/*
 * Match using an extended regular expression.
 *
 * Usage: @regex_match(regex, input [, subexpression [, ignorecase]])
 */
static char *
lws_tf_regex_match(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	regmatch_t pmatch[64];
	char *rtn = NULL;
	char ebuf[128];
	int icase = 0;
	int sube = 0;
	regex_t preg;
	int err;

	/* Get optional parameters */
	switch (ac) {
	case 5:
		icase = atoi(av[4]);
		/* FALL THROUGH */
	case 4:
		sube = atoi(av[3]);
		/* FALL THROUGH */
	default:
		break;
	}

	/* Sanity check */
	if (sube < 0 || sube >= sizeof(pmatch) / sizeof(*pmatch)) {
		errno = ERANGE;
		goto done;
	}

	/* Compile pattern */
	if ((err = pd_regcomp(&preg, av[1], REG_EXTENDED
	    | (icase ? REG_ICASE : 0))) != 0) {
		pd_regerror(err, &preg, ebuf, sizeof(ebuf));
		*errmsgp = STRDUP(mtype, ebuf);
		return (NULL);
	}

	/* Try to match */
	memset(&pmatch, 0, sizeof(pmatch));
	if ((err = pd_regexec(&preg, av[2],
	    sizeof(pmatch) / sizeof(*pmatch), pmatch, 0)) != 0) {
		if (err == REG_NOMATCH) {
			rtn = STRDUP(mtype, "");
			goto done;
		}
		pd_regerror(err, &preg, ebuf, sizeof(ebuf));
		*errmsgp = STRDUP(mtype, ebuf);
		goto done;
	}

	/* Return desired subexpression */
	if (pmatch[sube].rm_so == -1) {
		rtn = STRDUP(mtype, "");
		goto done;
	}
	av[2][pmatch[sube].rm_eo] = '\0';
	rtn = STRDUP(mtype, av[2] + pmatch[sube].rm_so);

done:
	/* Done */
	pd_regfree(&preg);
	return (rtn);
}

/*
 * Escape an extended regular expression.
 *
 * Usage: @regex_escape(string)
 */
static char *
lws_tf_regex_escape(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	static const char *special = "|*+?{}()[]\\.^$";
	const char *s;
	char *buf;
	char *t;

	if ((buf = MALLOC(mtype, strlen(av[1]) * 2 + 1)) == NULL)
		return (NULL);
	for (s = av[1], t = buf; *s != '\0'; s++) {
		if (strchr(special, *s) != NULL)
			*t++ = '\\';
		*t++ = *s;
	}
	*t = '\0';
	return (buf);
}

