
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_tmpl.h"
#include "lws_tmpl_logs.h"

/***********************************************************************
			LOGS OBJECT
***********************************************************************/

#define LWS_LOGS_OBJECT_MTYPE		"lws_logs_object"

struct tinfo lws_tmpl_logs_tinfo
	= TINFO_INIT(&alog_history_type, LWS_LOGS_OBJECT_MTYPE, NULL);

/***********************************************************************
			LOGS TMPL FUNCTIONS
***********************************************************************/

static tmpl_handler_t	lws_tf_logs_clear;
static tmpl_handler_t	lws_tf_logs_load;

/* User-defined template function descriptor list */
const struct lws_tmpl_func lws_tmpl_logs_functions[] = {
    LWS_TMPL_FUNC(logs_clear, 0, 0, "",
	"Clears the web server persistent log history."),
    LWS_TMPL_FUNC(logs_load, 3, 5, "severity:maxnum:time:pattern:icase",
	"Loads the logs object by reading from the persistent log history"
"\n"	"and filtering based on the arguments: $1 specifies a minimum log"
"\n"	"severity; $2 specifies the maximum number of entries to retrieve;"
"\n"	"$3 specifies a maximum age (in seconds); $4, if supplied specifies"
"\n"	"an extended regular expression (see <code>re_format(7)</code>)"
"\n"	"that entries must match; $5, if supplied and non-zero, specifies"
"\n"	"that $4 should match case insensitively."),
	{ { NULL } }
};

/*
 * Clear the log history.
 */
static char *
lws_tf_logs_clear(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	if (alog_clear_history(0) == -1)
		return (NULL);
	alog(LOG_NOTICE, "log history cleared");
	return (STRDUP(mtype, ""));
}

/*
 * Load in some log history.
 *
 * Usage: @logs_load(severity, maxnum, time [, regexp [, icase]])
 */
static char *
lws_tf_logs_load(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	struct alog_history *history;
	regex_t *preg = NULL;
	char *rtn = NULL;
	regex_t reg;
	int sev;
	int max;
	int time;

	/* Parse parameters */
	if ((sev = alog_severity(av[1])) == -1) {
		errno = EINVAL;
		return (NULL);
	}
	if ((max = atoi(av[2])) <= 0) {
		errno = EINVAL;
		return (NULL);
	}
	if ((time = atoi(av[3])) <= 0) {
		errno = EINVAL;
		return (NULL);
	}
	if (ac >= 5) {
		int icase = (ac >= 6) && (atoi(av[5]) != 0);
		char ebuf[128];
		int err;

		if ((err = regcomp(&reg, av[4],
		    REG_EXTENDED | REG_NOSUB | (icase ? REG_ICASE : 0))) != 0) {
			regerror(err, &reg, ebuf, sizeof(ebuf));
			*errmsgp = STRDUP(mtype, ebuf);
			return (NULL);
		}
		preg = &reg;
	}

	/* Load in desired log entries */
	if ((history = MALLOC(LWS_LOGS_OBJECT_MTYPE, sizeof(*history))) == NULL)
		goto done;
	if ((alog_get_history(0, sev, max, time, preg, history)) == -1) {
		FREE(LWS_LOGS_OBJECT_MTYPE, history);
		goto done;
	}

	/* Save logs in thread-local storage */
	if (tinfo_set_nocopy(&lws_tmpl_logs_tinfo, history) == -1) {
		structs_free(&alog_history_type, NULL, history);
		FREE(LWS_LOGS_OBJECT_MTYPE, history);
		goto done;
	}

	/* OK */
	rtn = STRDUP(mtype, "");

done:
	/* Done */
	if (preg != NULL)
		regfree(preg);
	return (rtn);
}

