
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "sls_global.h"
#include "sls_config.h"
#include "sls_server_subsystem.h"

/***********************************************************************
			APP_CONFIG INFO
***********************************************************************/

#define CONFIG_VERSION	0

/* Structs types for all versions of 'struct sls_config' */
static const struct structs_type *sls_config_types[CONFIG_VERSION + 1] = {
	&sls_config_type,
};

/* List of subsystems in this application for app_config(3) */
static struct app_subsystem sls_curconf_subsystem;
static struct app_subsystem sls_error_log_subsystem;
static struct app_subsystem sls_pidfile_subsystem;

static const struct app_subsystem *sls_config_subsystems[] = {
	&sls_curconf_subsystem,
	&sls_pidfile_subsystem,
	&sls_error_log_subsystem,
	&sls_server_subsystem,
	NULL
};

/* Internal functions for our 'struct app_config' */
static app_config_getnew_t	sls_config_getnew;
static app_config_checker_t	sls_config_checker;
static app_config_normalize_t	sls_config_normalize;

/* Information about this application for app_config(3) */
static const struct app_config sls_app_config = {
	CONFIG_VERSION,
	sls_config_types,
	sls_config_subsystems,
	NULL,
	sls_config_getnew,
	sls_config_checker,
	sls_config_normalize,
	NULL,
};

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/* Our app_config(3) configuration context */
struct app_config_ctx *sls_config_ctx;

/* Current configuration */
const struct	sls_config *const sls_curconf;

/*
 * Initialize configuration
 */
int
sls_config_init(struct pevent_ctx *ctx, const char *path)
{
	static const struct app_config_alog_info error_info = {
		"error_log", 0
	};

	/* Sanity check */
	if (sls_config_ctx != NULL) {
		errno = EBUSY;
		return (-1);
	}

	/* Initialize our versions of subsystems supplied by app_config(3) */
	sls_curconf_subsystem = app_config_curconf_subsystem;
	sls_curconf_subsystem.arg = (void *)&sls_curconf;
	sls_pidfile_subsystem = app_config_pidfile_subsystem;
	sls_pidfile_subsystem.arg = (void *)"pidfile";
	sls_error_log_subsystem = app_config_alog_subsystem;
	sls_error_log_subsystem.name = "error_log";
	sls_error_log_subsystem.arg = (void *)&error_info;

	/* Initialize application configuration framework */
	if ((sls_config_ctx = app_config_init(ctx,
	    &sls_app_config, ctx)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "app_config_init");
		return (-1);
	}

	/* Load configuration from specified config file */
	if (app_config_load(sls_config_ctx, path, 1) == -1) {
		alog(LOG_ERR, "can't load configuration from \"%s\": %m", path);
		return (-1);
	}

	/* Done */
	return (0);
}

/***********************************************************************
			APP_CONFIG METHODS
***********************************************************************/

/*
 * Create a new configuration for this machine
 */
static int
sls_config_getnew(struct app_config_ctx *ctx, void *data)
{
	alog(LOG_ERR, "config file must be supplied");
	errno = ENOENT;
	return -1;
}

/*
 * Validate a configuration object.
 */
static int
sls_config_checker(struct app_config_ctx *ctx, const void *data,
	char *ebuf, size_t emax)
{
	return (1);
}

/*
 * Normalize configuration.
 */
static void
sls_config_normalize(struct app_config_ctx *ctx, void *data)
{
	struct sls_config *const config = data;

	while (config->dns_servers.length > 2) {
		structs_array_delete(&sls_config_type,
		    "dns_servers", config->dns_servers.length - 1, config);
	}
	while (config->nbns_servers.length > 2) {
		structs_array_delete(&sls_config_type,
		    "nbns_servers", config->nbns_servers.length - 1, config);
	}
}

/***********************************************************************
			STRUCTS TYPES
***********************************************************************/

/* Structs type for 'struct sls_user' */
static const struct structs_field sls_user_fields[] = {
	STRUCTS_STRUCT_FIELD(sls_user, name, &structs_type_string),
	STRUCTS_STRUCT_FIELD(sls_user, password, &structs_type_string),
	STRUCTS_STRUCT_FIELD(sls_user, ip, &structs_type_ip4),
	{ NULL, NULL }
};
static const struct structs_type sls_user_type
	= STRUCTS_STRUCT_TYPE(sls_user, sls_user_fields);

/* Structs type for 'struct ip_addr_ary' */
static const struct structs_type ip_addr_ary_type
	= STRUCTS_ARRAY_TYPE(&structs_type_ip4, "struct ip_addr_ary", "ip");

/* Structs type for 'struct sls_user_ary' */
static const struct structs_type sls_user_ary_type
	= STRUCTS_ARRAY_TYPE(&sls_user_type, "struct sls_user_ary", "user");

/* Structs type for 'struct sls_config' */
static const struct structs_field sls_config_fields[] = {
	STRUCTS_STRUCT_FIELD(sls_config, pidfile, &structs_type_string),
	STRUCTS_STRUCT_FIELD(sls_config, error_log, &alog_config_type),
	STRUCTS_STRUCT_FIELD(sls_config, bind_ip, &structs_type_ip4),
	STRUCTS_STRUCT_FIELD(sls_config, bind_port, &structs_type_uint16),
	STRUCTS_STRUCT_FIELD(sls_config, inside_ip, &structs_type_ip4),
	STRUCTS_STRUCT_FIELD(sls_config, dns_servers, &ip_addr_ary_type),
	STRUCTS_STRUCT_FIELD(sls_config, nbns_servers, &ip_addr_ary_type),
	STRUCTS_STRUCT_FIELD(sls_config, ip_pool_start, &structs_type_ip4),
	STRUCTS_STRUCT_FIELD(sls_config, ip_pool_size, &structs_type_uint),
	STRUCTS_STRUCT_FIELD(sls_config, users, &sls_user_ary_type),
	{ NULL, NULL }
};
const struct structs_type sls_config_type
	= STRUCTS_STRUCT_TYPE(sls_config, sls_config_fields);

