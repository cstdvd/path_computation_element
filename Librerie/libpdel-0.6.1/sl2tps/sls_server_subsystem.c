
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "sls_global.h"
#include "sls_config.h"
#include "sls_server_subsystem.h"

#define MEM_TYPE		"sls_server"

/* Subsystem methods */
static app_ss_startup_t		sls_server_start;
static app_ss_shutdown_t	sls_server_stop;
static app_ss_changed_t		sls_server_changed;

/* Subsystem definition */
const struct app_subsystem sls_server_subsystem = {
	"sls_server",
	NULL,
	sls_server_start,
	sls_server_stop,
	NULL,
	sls_server_changed,
	NULL
};

/* Assigned IP addresses */
u_int32_t	*ip_pool;

/*
 * Start server.
 */
static int
sls_server_start(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *data)
{
	const struct sls_config *const config = data;
	static int not_first;

	/* Log message */
	if (!not_first) {
		alog(LOG_NOTICE, "process %lu server started", (u_long)pid);
		not_first = 1;
	}

	/* Allocate bits for IP address pool */
	if ((ip_pool = MALLOC(MEM_TYPE, (config->ip_pool_size + 31) / 32
	    * sizeof(*ip_pool))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		return (-1);
	}

	/* Start server */
	if (sls_l2tp_start(engine) == -1) {
		alogf(LOG_ERR, "failed to start L2TP server");
		return (-1);
	}

	/* Done */
	return (0);
}

static void
sls_server_stop(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *data)
{
	/* Stop server */
	sls_l2tp_stop(engine);

	/* Free IP pool */
	FREE(MEM_TYPE, ip_pool);
	ip_pool = NULL;
}

static int
sls_server_changed(struct app_config_ctx *ctx, const struct app_subsystem *ss,
	const void *config1, const void *config2)
{
	return !structs_equal(&sls_config_type, NULL, config1, config2);
}

