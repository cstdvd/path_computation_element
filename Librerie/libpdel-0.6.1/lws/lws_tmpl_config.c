
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"
#include "lws_tmpl.h"
#include "lws_tmpl_config.h"

/***********************************************************************
			CONFIG OBJECT
***********************************************************************/

static tinfo_init_t	lws_config_object_init;

struct tinfo lws_tmpl_config_tinfo = TINFO_INIT(&lws_config_type,
	"tmpl_config", lws_config_object_init);

/*
 * Initialize this thread's configuration object.
 */
static int
lws_config_object_init(struct tinfo *t, void *data)
{
	void *config;

	if ((config = app_config_get(lws_config_ctx, 1)) == NULL)
		return (-1);
	if (structs_get(t->type, NULL, config, data) == -1) {
		app_config_free(lws_config_ctx, &config);
		return (-1);
	}
	app_config_free(lws_config_ctx, &config);
	return (0);
}

/***********************************************************************
			CONFIG TMPL FUNCTIONS
***********************************************************************/

/* Our user-defined template functions */
static tmpl_handler_t	lws_tf_config_apply;
static tmpl_handler_t	lws_tf_config_server;
static tmpl_handler_t	lws_tf_config_vhost;
static tmpl_handler_t	lws_tf_config_servlet;

/* User-defined template function descriptor list */
const struct lws_tmpl_func lws_tmpl_config_functions[] = {
    LWS_TMPL_FUNC(config_apply, 1, 1, "delay",
	"Make the current configuration object the new configuration after"
"\n"	"a delay of $1 seconds. Returns the empty string if successful,"
"\n"	"otherwise an error message is returned."),
    LWS_TMPL_FUNC(config_server, 0, 0, "",
	"Returns the config index of the server that the currently executing"
"\n"	"servlet is running in."),
    LWS_TMPL_FUNC(config_vhost, 0, 0, "",
	"Returns the config index of the virtual host that the currently"
"\n"	"executing servlet is running in."),
    LWS_TMPL_FUNC(config_servlet, 0, 0, "",
	"Returns the config index of the currently executing servlet."),
	{ { NULL } }
};

/*
 * Apply this thread's configuration object.
 */
static char *
lws_tf_config_apply(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const void *config;
	char ebuf[512];

	/* Get config structure */
	if ((config = tinfo_get(&lws_tmpl_config_tinfo)) == NULL)
		return (NULL);

	/* Apply it */
	if (app_config_set(lws_config_ctx, config,
	      atoi(av[1]), ebuf, sizeof(ebuf)) == -1) {
		if (errno != EINVAL)
			return (NULL);
		return (STRDUP(mtype, ebuf));
	}

	/* Done */
	return (STRDUP(mtype, ""));
}

/*
 * Get servlet's server index.
 */
static char *
lws_tf_config_server(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const struct lws_tmpl_info *const tfi = targ->arg;
	char buf[32];

	snprintf(buf, sizeof(buf), "%d", tfi->sidx);
	return (STRDUP(mtype, buf));
}

/*
 * Get servlet's virtual host index.
 */
static char *
lws_tf_config_vhost(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const struct lws_tmpl_info *const tfi = targ->arg;
	char buf[32];

	snprintf(buf, sizeof(buf), "%d", tfi->vidx);
	return (STRDUP(mtype, buf));
}

/*
 * Get servlet's servlet index.
 */
static char *
lws_tf_config_servlet(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const struct lws_tmpl_info *const tfi = targ->arg;
	char buf[32];

	snprintf(buf, sizeof(buf), "%d", tfi->lidx);
	return (STRDUP(mtype, buf));
}

