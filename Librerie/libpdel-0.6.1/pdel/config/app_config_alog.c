
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <netinet/in.h>

#include <stdio.h>
#include <assert.h>
#include <syslog.h>
#include <stdarg.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "util/typed_mem.h"
#include "config/app_config.h"
#include "sys/alog.h"

/************************************************************************
			ALOG LOGGING SUBSYSTEM
************************************************************************/

static app_ss_startup_t		app_alog_start;
static app_ss_shutdown_t	app_alog_shutdown;
static app_ss_willrun_t		app_alog_willrun;
static app_ss_changed_t		app_alog_changed;

const struct app_subsystem	app_config_alog_subsystem = {
	"alog",
	NULL,
	app_alog_start,
	app_alog_shutdown,
	app_alog_willrun,
	app_alog_changed,
	NULL
};

/*
 * Alog startup
 */
static int
app_alog_start(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const struct app_config_alog_info *const info = ss->arg;
	struct alog_config aconf;

	/* Get alog config */
	if (structs_get(app_config_get_type(ctx),
	    info->name, config, &aconf) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_get");
		return (-1);
	}

	/* Start up alog channel */
	if (alog_configure(info->channel, &aconf) == -1) {
		alog(LOG_ERR, "error configuring logging: %m");
		structs_free(&alog_config_type, NULL, &aconf);
		return (-1);
	}
	structs_free(&alog_config_type, NULL, &aconf);
	return (0);
}

/*
 * Alog shutdown
 */
static void
app_alog_shutdown(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const struct app_config_alog_info *const info = ss->arg;

	alog_shutdown(info->channel);
}

/*
 * Alog necessity check
 */
static int
app_alog_willrun(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const struct app_config_alog_info *const info = ss->arg;

	(void)info;				/* avoid compiler warning */
	assert(info->name != NULL);
	return (1);
}

/*
 * Alog configuration changed check
 */
static int
app_alog_changed(struct app_config_ctx *ctx,
	const struct app_subsystem *ss,
	const void *config1, const void *config2)
{
	const struct app_config_alog_info *const info = ss->arg;
	int ret;

	/* Compare configs */
	if ((ret = structs_equal(app_config_get_type(ctx),
	    info->name, config1, config2)) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_equal");
		return (-1);
	}
	return (!ret);
}

