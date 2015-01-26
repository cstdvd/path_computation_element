
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
			CURCONF SUBSYSTEM
************************************************************************/

static app_ss_startup_t		app_curconf_start;
static app_ss_shutdown_t	app_curconf_shutdown;
static app_ss_changed_t		app_curconf_changed;

const struct app_subsystem	app_config_curconf_subsystem = {
	"curconf",
	NULL,
	app_curconf_start,
	app_curconf_shutdown,
	NULL,
	app_curconf_changed,
	NULL
};

static int
app_curconf_start(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const void **const curconfp = ss->arg;

	/* Copy configuration and store it in 'curconf' */
	if ((*curconfp = app_config_copy(ctx, config)) == NULL) {
		alog(LOG_ERR, "%s: failed to copy config: %m", __FUNCTION__);
		return (-1);
	}
	return (0);
}

static void
app_curconf_shutdown(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const void **const curconfp = ss->arg;

	/* Free and NULL-ify 'curconf' */
	app_config_free(ctx, (void **)curconfp);
}

static int
app_curconf_changed(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config1,
	const void *config2)
{
	return (1);
}

