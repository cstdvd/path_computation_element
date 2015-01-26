
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"
#include "lws_server_subsystem.h"

/***********************************************************************
			APP_CONFIG INFO
***********************************************************************/

#define CONFIG_VERSION	0

/* Structs types for all versions of 'struct lws_config' */
static const struct structs_type *lws_config_types[CONFIG_VERSION + 1] = {
	&lws_config_type,
};

/* List of subsystems in this application for app_config(3) */
static struct app_subsystem lws_curconf_subsystem;
static struct app_subsystem lws_error_log_subsystem;
static struct app_subsystem lws_pidfile_subsystem;

static const struct app_subsystem *lws_config_subsystems[] = {
	&lws_curconf_subsystem,
	&lws_pidfile_subsystem,
	&lws_error_log_subsystem,
	&lws_server_subsystem,
	NULL
};

/* Internal functions for our 'struct app_config' */
static app_config_getnew_t	lws_config_getnew;
static app_config_checker_t	lws_config_checker;
static app_config_normalize_t	lws_config_normalize;

/* Information about this application for app_config(3) */
static const struct app_config lws_app_config = {
	CONFIG_VERSION,
	lws_config_types,
	lws_config_subsystems,
	NULL,
	lws_config_getnew,
	lws_config_checker,
	lws_config_normalize,
	NULL,
};

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/* Our app_config(3) configuration context */
struct app_config_ctx *lws_config_ctx;

/* Current configuration */
const struct	lws_config *const lws_curconf;

/*
 * Initialize configuration
 */
int
lws_config_init(struct pevent_ctx *ctx, const char *path)
{
	static const struct app_config_alog_info error_info = {
		"error_log", 0
	};

	/* Sanity check */
	if (lws_config_ctx != NULL) {
		errno = EBUSY;
		return (-1);
	}

	/* Initialize our versions of subsystems supplied by app_config(3) */
	lws_curconf_subsystem = app_config_curconf_subsystem;
	lws_curconf_subsystem.arg = (void *)&lws_curconf;
	lws_pidfile_subsystem = app_config_pidfile_subsystem;
	lws_pidfile_subsystem.arg = (void *)"pidfile";
	lws_error_log_subsystem = app_config_alog_subsystem;
	lws_error_log_subsystem.name = "error_log";
	lws_error_log_subsystem.arg = (void *)&error_info;

	/* Initialize application configuration framework */
	if ((lws_config_ctx = app_config_init(ctx,
	    &lws_app_config, ctx)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "app_config_init");
		return (-1);
	}

	/* Load configuration from specified config file */
	if (app_config_load(lws_config_ctx, path, 1) == -1) {
		alog(LOG_ERR, "can't load configuration from \"%s\": %m", path);
		return (-1);
	}

	/* Done */
	return (0);
}

/***********************************************************************
			APP_CONFIG METHODS
***********************************************************************/

/* Support functions for lws_config_checker() */
static int	lws_config_check_server(int sidx, const struct lws_server * const s,
			char *ebuf, size_t emax);
static int	lws_config_check_virthost(int sidx,
			const struct lws_virthost * const v, char *ebuf, size_t emax);
static int	lws_config_check_servlet(int sidx, const struct lws_virthost * const v,
			const struct lws_servlet *const sl, int slidx,
			char *ebuf, size_t emax);

/*
 * Create a new configuration for this machine
 */
static int
lws_config_getnew(struct app_config_ctx *ctx, void *data)
{
	struct lws_config *const config = data;
	FILE *fp;

	/* Overlay default configuration read from file */
	if ((fp = fopen(DEFAULT_CONFIG_FILE, "r")) == NULL) {
		alog(LOG_ERR, "%s: %m", DEFAULT_CONFIG_FILE);
		return (-1);
	}
	if (structs_xml_input(&lws_config_type, APP_CONFIG_XML_TAG,
	    NULL, NULL, fp, config, 0, STRUCTS_LOGGER_ALOG) == -1) {
		alog(LOG_ERR, "can't load %s: %m", DEFAULT_CONFIG_FILE);
		fclose(fp);
		return (-1);
	}
	fclose(fp);

	/* Done */
	return (0);
}

/*
 * Validate a configuration object.
 */
static int
lws_config_checker(struct app_config_ctx *ctx, const void *data,
	char *ebuf, size_t emax)
{
	const struct lws_config *const config = data;
	u_int i;

	/* Check servers */
	for (i = 0; i < config->servers.length; i++) {
		if (!lws_config_check_server(i + 1,
		    &config->servers.elems[i], ebuf, emax))
			return (0);
	}

	/* Make sure all servers listen on different IP and/or port */
	for (i = 0; i < config->servers.length; i++) {
		const struct lws_server *const s = &config->servers.elems[i];
		u_int j;

		for (j = i + 1; j < config->servers.length; j++) {
			const struct lws_server *const t
			    = &config->servers.elems[j];

			if ((s->ip.s_addr == 0 || t->ip.s_addr == 0
			      || s->ip.s_addr == t->ip.s_addr)
			    && (s->port == t->port)) {
				snprintf(ebuf, emax, "servers #%d and #%d are"
				    " configured to both listen on the same"
				    " port %d", i + 1, j + 1, s->port);
				return (0);
			}
		}
	}

	/* Done */
	return (1);
}

static int
lws_config_check_server(int sidx, const struct lws_server *const s,
	char *ebuf, size_t emax)
{
	int default_vhost = -1;
	u_int i;

	/* Check server port */
	if (s->port == 0) {
		snprintf(ebuf, emax, "server #%d is configured"
		    " to use invalid port %d", sidx, s->port);
		return (0);
	}

	/* Check server SSL config */
	if (s->ssl.enabled) {
		if (s->ssl.private_key == NULL || s->ssl.certificate == NULL) {
			snprintf(ebuf, emax, "server #%d has SSL enabled"
			    " but no %s is configured", sidx,
			    s->ssl.private_key == NULL ?
			      "private key" : "certificate");
			return (0);
		}
	}

	/* Check virtual hosts */
	for (i = 0; i < s->virtual_hosts.length; i++) {
		const struct lws_virthost *const v = &s->virtual_hosts.elems[i];
		u_int j;

		/* Check virtual host */
		if (!lws_config_check_virthost(sidx, v, ebuf, emax))
			return (0);

		/* Each virtual host needs a unique server name */
		for (j = i + 1; j < s->virtual_hosts.length; j++) {
			const struct lws_virthost *const v2
			    = &s->virtual_hosts.elems[j];

			if (strcmp(v->server_name, v2->server_name) == 0) {
				snprintf(ebuf, emax, "server #%d has"
				    " two virtual hosts with the same"
				    " name \"%s\"", sidx, v->server_name);
				return (0);
			}
		}

		/* There can only be one default virtual host */
		if (v->default_virtual_host) {
			if (default_vhost != -1) {
				const struct lws_virthost *const v2
				    = &s->virtual_hosts.elems[default_vhost];

				snprintf(ebuf, emax, "server #%d: virtual"
				    " hosts \"%s\" and \"%s\" are both"
				    " configured as the default virtual host,"
				    " but only one can be", sidx,
				    v2->server_name, v->server_name);
				 return (0);
			}
			default_vhost = i;
		}
	}

	/* If there are any cookieauth servlets, we need an RSA key */
	if (s->ssl.private_key == NULL) {
		for (i = 0; i < s->virtual_hosts.length; i++) {
			const struct lws_virthost *const v
			    = &s->virtual_hosts.elems[i];
			u_int j;

			/* Look for a cookieauth servlet */
			for (j = 0; j < v->servlets.length; j++) {
				if (strcmp(v->servlets.elems[j].info.field_name,
				    "cookieauth") == 0)
					break;
			}
			if (j < v->servlets.length) {
				snprintf(ebuf, emax, "server #%d has a"
				    " cookieauth servlet but no configured"
				    " SSL private key", sidx);
				return (0);
			}
		}
	}

	/* Done */
	return (1);
}

static int
lws_config_check_virthost(int sidx, const struct lws_virthost *const v,
	char *ebuf, size_t emax)
{
	const char *s;
	u_int i;

	/* If server name is the empty string, must be default virtual host */
	if (*v->server_name == '\0') {
		if (!v->default_virtual_host) {
			snprintf(ebuf, emax, "server #%d: the virtual host"
			    " with an empty server name must be selected"
			    " as the default virtual host", sidx);
			return (0);
		}
		goto server_name_ok;
	}

	/* Check virtual host server name */
	if (v->server_name[0] == '.'
	    || v->server_name[strlen(v->server_name) - 1] == '.')
		goto bogus_server_name;
	for (s = v->server_name; *s != '\0'; s++) {
		if (s[0] == '.' && s[1] == '.')
			goto bogus_server_name;
		if (!isalnum(*s) && *s != '-' && *s != '.') {
bogus_server_name:	snprintf(ebuf, emax, "server #%d: virtual host \"%s\":"
			    " invalid virtual host server name",
			    sidx, v->server_name);
			return (0);
		}
	}
server_name_ok:

	/* Check servlets */
	for (i = 0; i < v->servlets.length; i++) {
		if (!lws_config_check_servlet(sidx, v,
		    &v->servlets.elems[i], i, ebuf, emax))
			return (0);
	}

	/* Done */
	return (1);
}

static int
lws_config_check_servlet(int sidx, const struct lws_virthost *const v,
	const struct lws_servlet *const sl, int slidx, char *ebuf, size_t emax)
{
	if (strcmp(sl->info.field_name, "cookieauth") == 0) {
		if (sl->info.un->cookieauth.logon_url == NULL) {
			snprintf(ebuf, emax, "server #%d, virtual host \"%s\":"
			    " cookieauth servlet #%d has no logon redirect url",
			    sidx, v->server_name, slidx);
			return (0);
		}
	}

	/* Done */
	return (1);
}

/* Support functions for lws_config_normalize() */
static int	lws_servlet_cmp(const void *v1, const void *v2);

/*
 * Order servlets so that authorization servlets get registered first.
 */
static void
lws_config_normalize(struct app_config_ctx *ctx, void *data)
{
	const struct lws_config *const config = data;
	u_int i;

	for (i = 0; i < config->servers.length; i++) {
		const struct lws_server *const s = &config->servers.elems[i];
		u_int j;

		for (j = 0; j < s->virtual_hosts.length; j++) {
			const struct lws_virthost *const v
			    = &s->virtual_hosts.elems[j];

			qsort(v->servlets.elems, v->servlets.length,
			    sizeof(*v->servlets.elems), lws_servlet_cmp);
		}
	}
}

/*
 * Compare servlets for registration order.
 */
static int
lws_servlet_cmp(const void *v1, const void *v2)
{
	static const char *servlet_order[] = {
		"cookieauth",
		"basicauth",
		"redirect",
		"file",
		NULL
	};
	const struct lws_servlet *const s[2] = { v1, v2 };
	int order[2];
	int i;
	int j;

	for (i = 0; i < 2; i++) {
		for (j = 0;
		    servlet_order[j] != NULL
		      && strcmp(s[i]->info.field_name, servlet_order[j]) != 0;
		    j++);
		order[i] = j;
	}
	return (order[0] - order[1]);
}

/***********************************************************************
			STRUCTS TYPES
***********************************************************************/

/* Structs type for 'struct lws_file_info' */
static const struct structs_field lws_file_info_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_file_info, docroot, &structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_file_info, allow_escape,
		&structs_type_boolean_char_01),
	STRUCTS_STRUCT_FIELD(lws_file_info, allow_templates,
		&structs_type_boolean_char_01),
	STRUCTS_STRUCT_FIELD(lws_file_info, filename,
		&structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_file_info, prefix,
		&structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_file_info, mime_type,
		&structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_file_info, mime_encoding,
		&structs_type_string_null),
	{ NULL, NULL }
};
static const struct structs_type lws_file_info_type
	= STRUCTS_STRUCT_TYPE(lws_file_info, lws_file_info_fields);

/* Structs type for 'struct lws_xmlrpc_info' */
static const struct structs_field lws_xmlrpc_info_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_xmlrpc_info, libname, &structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_xmlrpc_info, initsym, &structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_xmlrpc_info, infosym, &structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_xmlrpc_info, argsym, &structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_xmlrpc_info, delsym, &structs_type_string_null),
	{ NULL, NULL }
};
static const struct structs_type lws_xmlrpc_info_type
	= STRUCTS_STRUCT_TYPE(lws_xmlrpc_info, lws_xmlrpc_info_fields);

/* Structs type for 'method' field of 'struct lws_redirect_info' */
static const struct structs_id lws_redirect_meth_ids[] = {
	{ "no_append",		HTTP_SERVLET_REDIRECT_NO_APPEND },
	{ "append_query",	HTTP_SERVLET_REDIRECT_APPEND_QUERY },
	{ "append_uri",		HTTP_SERVLET_REDIRECT_APPEND_URI },
	{ "append_url",		HTTP_SERVLET_REDIRECT_APPEND_URL },
	{ "append_qstring",	HTTP_SERVLET_REDIRECT_APPEND_URL }, /*obsolete*/
	{ NULL }
};
static const struct structs_type lws_redirect_meth_type
	= STRUCTS_ID_TYPE(lws_redirect_meth_ids, sizeof(int));

/* Structs type for 'struct lws_redirect_info' */
static const struct structs_field lws_redirect_info_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_redirect_info, url, &structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_redirect_info, method,
		&lws_redirect_meth_type),
	{ NULL, NULL }
};
static const struct structs_type lws_redirect_info_type
	= STRUCTS_STRUCT_TYPE(lws_redirect_info, lws_redirect_info_fields);

/* Structs type for 'struct lws_cookieauth_info' */
static const struct structs_field lws_cookieauth_info_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_cookieauth_info, resource_name,
		&structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_cookieauth_info, logon_url,
		&structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_cookieauth_info, cookie_name,
		&structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_cookieauth_info, allow_exceptions,
		&structs_type_boolean_char_01),
	STRUCTS_STRUCT_FIELD(lws_cookieauth_info, exceptions_pattern,
		&structs_type_regex),
	{ NULL, NULL }
};
static const struct structs_type lws_cookieauth_info_type
	= STRUCTS_STRUCT_TYPE(lws_cookieauth_info, lws_cookieauth_info_fields);

/* Structs type for 'struct lws_basicauth_info' */
static const struct structs_field lws_basicauth_info_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_basicauth_info, realm,
		&structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_basicauth_info, username_property,
		&structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_basicauth_info, password_property,
		&structs_type_string),
	{ NULL, NULL }
};
static const struct structs_type lws_basicauth_info_type
	= STRUCTS_STRUCT_TYPE(lws_basicauth_info, lws_basicauth_info_fields);

/* Structs type for 'union lws_servlet_info' */
static const struct structs_ufield lws_servlet_info_fields[] = {
	STRUCTS_UNION_FIELD(file, &lws_file_info_type),
	STRUCTS_UNION_FIELD(xmlrpc, &lws_xmlrpc_info_type),
	STRUCTS_UNION_FIELD(redirect, &lws_redirect_info_type),
	STRUCTS_UNION_FIELD(basicauth, &lws_basicauth_info_type),
	STRUCTS_UNION_FIELD(cookieauth, &lws_cookieauth_info_type),
	{ NULL, NULL }
};
static const struct structs_type lws_servlet_info_type
	= STRUCTS_UNION_TYPE(lws_servlet_info, lws_servlet_info_fields);

/* Structs type for 'struct lws_servlet' */
static const struct structs_field lws_servlet_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_servlet, url_pattern, &structs_type_regex),
	STRUCTS_STRUCT_FIELD(lws_servlet, ordering, &structs_type_int),
	STRUCTS_STRUCT_FIELD(lws_servlet, info, &lws_servlet_info_type),
	{ NULL, NULL }
};
static const struct structs_type lws_servlet_type
	= STRUCTS_STRUCT_TYPE(lws_servlet, lws_servlet_fields);

/* Structs type for 'struct lws_servlet_ary' */
static const struct structs_type lws_servlet_ary_type
	= STRUCTS_ARRAY_TYPE(&lws_servlet_type,
	    "struct lws_servlet_ary", "servlet");

/* Structs type for 'struct lws_virthost' */
static const struct structs_field lws_virthost_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_virthost, server_name,
		&structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_virthost, default_virtual_host,
		&structs_type_boolean_char_01),
	STRUCTS_STRUCT_FIELD(lws_virthost, servlets,
		&lws_servlet_ary_type),
	{ NULL, NULL }
};
static const struct structs_type lws_virthost_type
	= STRUCTS_STRUCT_TYPE(lws_virthost, lws_virthost_fields);

/* Structs type for 'struct lws_server_ssl' */
static const struct structs_field lws_server_ssl_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_server_ssl, enabled,
		&structs_type_boolean_char_01),
	STRUCTS_STRUCT_FIELD(lws_server_ssl, private_key,
		&structs_type_string_null),
	STRUCTS_STRUCT_FIELD(lws_server_ssl, certificate,
		&structs_type_string_null),
	{ NULL, NULL }
};
static const struct structs_type lws_server_ssl_type
	= STRUCTS_STRUCT_TYPE(lws_server_ssl, lws_server_ssl_fields);

/* Structs type for 'struct lws_virthost_ary' */
static const struct structs_type lws_virthost_ary_type
	= STRUCTS_ARRAY_TYPE(&lws_virthost_type,
	    "struct lws_virthost_ary", "virtual_host");

/* Structs type for 'struct lws_server' */
static const struct structs_field lws_server_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_server, ip, &structs_type_ip4),
	STRUCTS_STRUCT_FIELD(lws_server, port, &structs_type_uint16),
	STRUCTS_STRUCT_FIELD(lws_server, ssl, &lws_server_ssl_type),
	STRUCTS_STRUCT_FIELD(lws_server, virtual_hosts, &lws_virthost_ary_type),
	{ NULL, NULL }
};
static const struct structs_type lws_server_type
	= STRUCTS_STRUCT_TYPE(lws_server, lws_server_fields);

/* Structs type for 'struct lws_server_ary' */
static const struct structs_type lws_server_ary_type
	= STRUCTS_ARRAY_TYPE(&lws_server_type,
	    "struct lws_server_ary", "server");

/* Structs type for 'struct lws_property' */
static const struct structs_field lws_property_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_property, name, &structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_property, value, &structs_type_string),
	{ NULL, NULL }
};
static const struct structs_type lws_property_type
	= STRUCTS_STRUCT_TYPE(lws_property, lws_property_fields);

/* Structs type for 'struct lws_property_ary' */
static const struct structs_type lws_property_ary_type
	= STRUCTS_ARRAY_TYPE(&lws_property_type,
	    "struct lws_property_ary", "property");

/* Structs type for 'struct lws_config' */
static const struct structs_field lws_config_fields[] = {
	STRUCTS_STRUCT_FIELD(lws_config, pidfile, &structs_type_string),
	STRUCTS_STRUCT_FIELD(lws_config, error_log, &alog_config_type),
	STRUCTS_STRUCT_FIELD(lws_config, servers, &lws_server_ary_type),
	STRUCTS_STRUCT_FIELD(lws_config, properties, &lws_property_ary_type),
	{ NULL, NULL }
};
const struct structs_type lws_config_type
	= STRUCTS_STRUCT_TYPE(lws_config, lws_config_fields);

