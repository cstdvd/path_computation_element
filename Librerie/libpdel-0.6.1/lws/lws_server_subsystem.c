
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"
#include "lws_server_subsystem.h"
#include "lws_tmpl.h"

#include <pdel/http/servlet/xmlrpc.h>

#ifndef WIN32
#include "dlfcn.h"
#endif

#define MEM_TYPE		"lws_server"
#define TMPL_SUFFIX		".tmpl"

/* Subsystem methods */
static app_ss_startup_t		lws_server_start;
static app_ss_shutdown_t	lws_server_stop;

/* Subsystem dependency list */
static const char *lws_server_deplist[] = {
	"servers",
	NULL
};

/* Subsystem definition */
const struct app_subsystem lws_server_subsystem = {
	"lws_server",
	NULL,
	lws_server_start,
	lws_server_stop,
	NULL,
	NULL,
	lws_server_deplist
};

/* List of running HTTP servers */
static struct	http_server **lws_http_servers;

/* Private copy of the config object */
static void	*lws_servers_config;

/* Internal functions */
static int	lws_server_start_vhost(const struct lws_config *config,
			int sidx, const struct lws_virthost *const v);
static void	lws_server_start_servlet(const struct lws_config *config,
			int sidx, const struct lws_virthost *const v,
			const struct lws_servlet *s);

static http_servlet_file_hide_t		lws_file_hide_tmpl;
static http_servlet_basicauth_t		lws_basicauth_auth;
static http_servlet_cookieauth_reqd_t	lws_cookieauth_reqd;
static http_servlet_tmpl_free_t		lws_tmpl_free;
static http_logger_t			lws_server_logger;

/*
 * Start LWS servers.
 */
static int
lws_server_start(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *data)
{
	struct pevent_ctx *const ev_ctx = app_config_get_cookie(ctx);
	const struct lws_config *config;
	static int not_first;
	u_int i;

	/* Log message */
	if (!not_first) {
		alog(LOG_NOTICE, "process %lu server started", (u_long)pid);
		not_first = 1;
	}

	/* Copy config object; we need a stable copy */
	if ((lws_servers_config = app_config_copy(ctx, data)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "app_config_copy");
		return (-1);
	}
	config = lws_servers_config;

	/* Initialize template functions */
	if (lws_tmpl_funcs_init() == -1) {
		alogf(LOG_ERR, "failed to initialize template functions%s", "");
		app_config_free(ctx, &lws_servers_config);
		return (-1);
	}

	/* Allocate servers array */
	if ((lws_http_servers = MALLOC(MEM_TYPE,
	    config->servers.length * sizeof(*lws_http_servers))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		lws_tmpl_funcs_uninit();
		app_config_free(ctx, &lws_servers_config);
		return (-1);
	}

	/* Start each server */
	for (i = 0; i < config->servers.length; i++) {
		const struct lws_server *const s = &config->servers.elems[i];
		const struct http_server_ssl *ssl = NULL;
		struct http_server_ssl ssl_info;
		const struct lws_virthost *dv;
		struct http_servlet *servlet;
		int default_vhost = -1;
		char buf[64];
		char *url;
		u_int j;

		/* Set up SSL info if enabled */
		if (s->ssl.enabled) {
			ssl_info.cert_path = s->ssl.certificate;
			ssl_info.pkey_path = s->ssl.private_key;
			ssl_info.pkey_password = "";  /* XXX pkey unencrypted */
			ssl = &ssl_info;
		}

		/* Start server */
		snprintf(buf, sizeof(buf), "LWS %s", LWS_SERVER_VERSION);
		if ((lws_http_servers[i] = http_server_start(ev_ctx, s->ip,
		    s->port, ssl, buf, lws_server_logger)) == NULL) {
			alog(LOG_ERR, "failed to start server #%d on %s:%u",
			    i, inet_ntoa(s->ip), s->port);
			continue;
		}

		/* Start virtual hosts */
		for (j = 0; j < s->virtual_hosts.length; j++) {
			const struct lws_virthost *const v
			    = &s->virtual_hosts.elems[j];

			if (lws_server_start_vhost(config, i,
			    &s->virtual_hosts.elems[j]) == -1) {
				alog(LOG_ERR, "error starting virtual host"
				    " \"%s\" for server #%d: %m",
				    v->server_name, i);
			}
			if (v->default_virtual_host)
				default_vhost = j;
		}

		/*
		 * If a default virtual host is specified, and it's not
		 * the "" virtual host, then register a redirect servlet
		 * to it from the "" virtual host.
		 */
		if (default_vhost == -1)
			continue;
		dv = &s->virtual_hosts.elems[default_vhost];
		if (*dv->server_name == '\0')
			continue;

		/* Generate the URL for the redirect */
		if (s->port != (s->ssl.enabled ?  HTTPS_PORT : HTTP_PORT))
			snprintf(buf, sizeof(buf), ":%u", s->port);
		else
			*buf = '\0';
		ASPRINTF(TYPED_MEM_TEMP, &url, "http%s://%s%s",
		    s->ssl.enabled ? "s" : "" _ dv->server_name _ buf);

		/* Create redirect servlet */
		servlet = http_servlet_redirect_create(url,
		    HTTP_SERVLET_REDIRECT_APPEND_URI);
		FREE(TYPED_MEM_TEMP, url);
		if (servlet == NULL) {
			alog(LOG_ERR, "can't %s redirect servlet"
			    " for the default virtual host: %m", "create");
			continue;
		}

		/* Register servlet on virtual host "" */
		if (http_server_register_servlet(lws_http_servers[i],
		    servlet, NULL, "^/", 0) == -1) {
			alog(LOG_ERR, "can't %s redirect servlet"
			    " for the default virtual host: %m", "register");
			http_server_destroy_servlet(&servlet);
		}
	}

	/* Done */
	return (0);
}

static int
lws_server_start_vhost(const struct lws_config *config,
	int sidx, const struct lws_virthost *const v)
{
	u_int i;

	/* Register servlets */
	for (i = 0; i < v->servlets.length; i++) {
		lws_server_start_servlet(config,
		    sidx, v, &v->servlets.elems[i]);
	}

	/* Done */
	return (0);
}

static void
lws_server_start_servlet(const struct lws_config *config, int sidx,
	const struct lws_virthost *const v, const struct lws_servlet *s)
{
	const struct lws_server *const server = &config->servers.elems[sidx];
	const int vidx = v - server->virtual_hosts.elems;
	const int lidx = s - v->servlets.elems;
	struct http_servlet *servlet;
	const char *emsg = "";

	/* Create servlet */
	if (strcmp(s->info.field_name, "file") == 0) {
		const struct lws_file_info *const info = &s->info.un->file;
		struct http_servlet_file_info finfo;

		/* Set up file servlet info */
		memset(&finfo, 0, sizeof(finfo));
		finfo.docroot = info->docroot;
		finfo.allow_escape = info->allow_escape;
		finfo.filename = info->filename;
		finfo.prefix = info->prefix;
		finfo.mime_type = info->mime_type;
		finfo.mime_encoding = info->mime_encoding;
		finfo.logger = lws_server_logger;

		/*
		 * Enable serving templates if configured to do so.
		 * The template user cookie points to the lws_server.
		 */
		if (info->allow_templates) {
			struct lws_tmpl_info *tfi;

			/* Create info structure for the servlet */
			if ((tfi = MALLOC(MEM_TYPE, sizeof(*tfi))) == NULL) {
				emsg = "MALLOC(templates) failed";
				goto servlet_failed;
			}
			memset(tfi, 0, sizeof(*tfi));
			tfi->server = server;
			tfi->fileinfo = info;
			tfi->sidx = sidx;
			tfi->vidx = vidx;
			tfi->lidx = lidx;

			/* Set up template servlet info */
			finfo.tinfo.flags = TMPL_SKIP_NL_WHITE;
			finfo.tinfo.mtype = TMPL_MEM_TYPE;
			finfo.tinfo.handler = lws_tmpl_handler;
			finfo.tinfo.errfmtr = lws_tmpl_errfmtr;
			finfo.tinfo.arg = tfi;
			finfo.tinfo.freer = lws_tmpl_free;

			/* Hide template files when not debugging */
			if (debug_level == 0)
				finfo.hide = lws_file_hide_tmpl;
		}

		/* Create file servlet */
		if ((servlet = http_servlet_file_create(&finfo)) == NULL) {
			emsg = "servlet_file_create() failed";
			goto servlet_failed;
		}
#ifndef WIN32
	} else if (strcmp(s->info.field_name, "xmlrpc") == 0) {
		char ebuf[150];
		void *(*initsym)(void *) = NULL;
		void *infosym = NULL;
		void *argsym = NULL;
		void *delsym = NULL;
		void *dlh = NULL;
		const struct lws_xmlrpc_info *const info
		    = &s->info.un->xmlrpc;

		/* Open the library */
		if (NULL == (dlh = dlopen(info->libname, RTLD_LAZY))) {
			snprintf(ebuf, sizeof(ebuf), 
				 "dlopen() failed-%s", dlerror());
			emsg = ebuf;
			goto servlet_failed;
		}
		if (info->argsym != NULL && info->argsym[0] != '\0') {
			argsym = dlsym(dlh, info->argsym);
		}
		if (info->delsym != NULL && info->delsym[0] != '\0') {
			delsym = dlsym(dlh, info->delsym);
		}
		if (info->initsym != NULL && info->initsym[0] != '\0') {
			initsym = dlsym(dlh, info->initsym);
			if (initsym != NULL) {
				infosym = (initsym)(argsym);
				emsg = "initsym() failed";
			} else {
				emsg = "dlsym(initsym) failed";
			}
		}
		if (infosym == NULL) {
			/* Only resolve infosym if not returned by initsym */
			if (info->infosym != NULL 
			    && info->infosym[0] != '\0') {
				infosym = dlsym(dlh, info->infosym);
			}
		}
		if (NULL == infosym) {
			if (emsg[0] == '\0') {
				emsg = "dlsym(infosym) failed";
			}
			goto servlet_failed;
		}
		/* Create redirect servlet */
		if ((servlet = http_servlet_xmlrpc_create(infosym, argsym,
							  delsym)) == NULL) {
			emsg = "servlet_xmlrpc_create() failed";
			goto servlet_failed;
		}
#endif
	} else if (strcmp(s->info.field_name, "redirect") == 0) {
		const struct lws_redirect_info *const info
		    = &s->info.un->redirect;

		/* Create redirect servlet */
		if ((servlet = http_servlet_redirect_create(info->url,
		    info->method)) == NULL)
			goto servlet_failed;
	} else if (strcmp(s->info.field_name, "basicauth") == 0) {
		const struct lws_basicauth_info *const info
		    = &s->info.un->basicauth;

		/* Create basicauth servlet */
		if ((servlet = http_servlet_basicauth_create(lws_basicauth_auth,
		    (void *)info, NULL)) == NULL)
			goto servlet_failed;
	} else if (strcmp(s->info.field_name, "cookieauth") == 0) {
		const struct lws_cookieauth_info *const info
		    = &s->info.un->cookieauth;

		/* Create cookieauth servlet */
		if ((servlet = http_servlet_cookieauth_create(info->logon_url,
		    HTTP_SERVLET_REDIRECT_APPEND_URL, lws_cookieauth_reqd,
		    (void *)info, NULL, server->ssl.private_key,
		    info->resource_name, strlen(info->resource_name),
		    info->cookie_name)) == NULL)
			goto servlet_failed;
	} else {

		/* Unknown servlet ?? */
		errno = ESRCH;

servlet_failed:
		/* Log servlet creation error */
		alog(LOG_ERR, "server #%d: virtual host \"%s\":"
		     " failed to %s %s servlet for \"%s\": %m [%s]", sidx,
		     v->server_name, "create", s->info.field_name,
		     s->url_pattern.pat, emsg);
		return;
	}

	/* Register servlet */
	if (http_server_register_servlet(lws_http_servers[sidx], servlet,
	    v->server_name, s->url_pattern.pat, s->ordering) == -1) {
		alog(LOG_ERR, "server #%d: virtual host \"%s\":"
		     " failed to %s %s servlet for \"%s\": %m [%s]", sidx,
		     v->server_name, "register", s->info.field_name,
		     s->url_pattern.pat, emsg);
		http_server_destroy_servlet(&servlet);
	}
}

static void
lws_server_stop(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *data)
{
	const struct lws_config *const config = data;
	u_int i;

	/* Stop each server */
	for (i = 0; i < config->servers.length; i++)
		http_server_stop(&lws_http_servers[i]);
	FREE(MEM_TYPE, lws_http_servers);
	lws_http_servers = NULL;

	/* Deallocate template functions */
	lws_tmpl_funcs_uninit();

	/* Free config copy */
	app_config_free(ctx, &lws_servers_config);
}

/*
 * This function hides the actual template files themselves.
 */
static int
lws_file_hide_tmpl(const struct http_servlet_file_info *info,
	struct http_request *req, struct http_response *resp, const char *path)
{
	const size_t plen = strlen(path);
	const size_t slen = sizeof(TMPL_SUFFIX) - 1;

	if (plen >= slen && strcmp(path + plen - slen, TMPL_SUFFIX) == 0)
		return (1);					/* hide it */
	return (0);
}

static const char *
lws_basicauth_auth(void *arg, struct http_request *req,
	const char *username, const char *password)
{
	const struct lws_basicauth_info *const info = arg;
	const char *auth_username = NULL;
	const char *auth_password = NULL;
	u_int i;

	/* Get username and password */
	for (i = 0; i < lws_curconf->properties.length; i++) {
		const struct lws_property *const prop
		    = &lws_curconf->properties.elems[i];

		if (strcmp(prop->name, info->username_property) == 0) {
			auth_username = prop->value;
			if (auth_password != NULL)
				break;
		}
		if (strcmp(prop->name, info->password_property) == 0) {
			auth_password = prop->value;
			if (auth_username != NULL)
				break;
		}
	}

	/* If properties were not found, reject authorization */
	if (auth_username == NULL || auth_password == NULL)
		return (info->realm);

	/* Compare username and password */
	if (strcmp(username, auth_username) != 0
	    || strcmp(password, auth_password) != 0)
		return (info->realm);

	/* Allow access */
	return (NULL);
}

static int
lws_cookieauth_reqd(void *arg, struct http_request *req)
{
	const struct lws_cookieauth_info *const info = arg;
	const char *const path = http_request_get_path(req);

	/* Check for logon page URL */
	if (strcmp(path, info->logon_url) == 0)
		return (0);

	/* Check against exceptions pattern */
	if (info->allow_exceptions
	    && regexec(&info->exceptions_pattern.reg, path, 0, NULL, 0) == 0)
		return (0);

	/* Require authentication */
	return (1);
}

static void
lws_tmpl_free(void *arg)
{
	struct lws_tmpl_info *const tfi = arg;

	FREE(MEM_TYPE, tfi);
}

static void
lws_server_logger(int sev, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	valog(sev, fmt, args);
	va_end(args);
}

