
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
#include <unistd.h>
#include <stdarg.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "util/typed_mem.h"
#include "config/app_config.h"
#include "sys/alog.h"

/************************************************************************
			DIRECTORY SUBSYSTEM
************************************************************************/

static app_ss_startup_t		app_directory_start;
static app_ss_willrun_t		app_directory_willrun;
static app_ss_changed_t		app_directory_changed;

const struct app_subsystem	app_config_directory_subsystem = {
	"directory",
	NULL,
	app_directory_start,
	NULL,
	app_directory_willrun,
	app_directory_changed,
	NULL
};

/*
 * Directory startup
 */
static int
app_directory_start(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const char *const name = ss->arg;
	char *dir;
	int r;

	/* Get directory */
	if ((dir = structs_get_string(app_config_get_type(ctx),
	    name, config, TYPED_MEM_TEMP)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_get_string");
		return (-1);
	}

	/* Change into it */
	if ((r = chdir(dir)) == -1)
		alog(LOG_ERR, "can't change to directory \"%s\": %m", dir);
	FREE(TYPED_MEM_TEMP, dir);
	return (r);
}

/*
 * Directory necessity check
 */
static int
app_directory_willrun(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const char *const name = ss->arg;
	char *dir;
	int r;

	/* Get directory */
	assert(name != NULL);
	if ((dir = structs_get_string(app_config_get_type(ctx),
	    name, config, TYPED_MEM_TEMP)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_get_string");
		return (-1);
	}

	/* Check if not NULL */
	r = (*dir != '\0');
	FREE(TYPED_MEM_TEMP, dir);
	return (r);
}

/*
 * Directory configuration changed check
 */
static int
app_directory_changed(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config1,
	const void *config2)
{
	const char *const name = ss->arg;
	int ret;

	/* Compare directory names */
	if ((ret = structs_equal(app_config_get_type(ctx),
	    name, config1, config2)) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_equal");
		return (-1);
	}
	return (!ret);
}

