
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _LWS_CONFIG_H_
#define _LWS_CONFIG_H_

/* Information for a file servlet */
struct lws_file_info {
	char			*docroot;
	u_char			allow_escape;
	u_char			allow_templates;
	char			*filename;
	char			*prefix;
	char			*mime_type;
	char			*mime_encoding;
};

struct lws_xmlrpc_info {
	char			*libname;
	char			*initsym;
	char			*infosym;
	char			*argsym;
	char			*delsym;
};

/* Information for a redirect servlet */
struct lws_redirect_info {
	char			*url;
	int			method;
};

/* Information for a basic auth servlet */
struct lws_basicauth_info {
	char			*realm;
	char			*username_property;
	char			*password_property;
};

/* Information for a cookieauth servlet */
struct lws_cookieauth_info {
	char			*resource_name;
	char			*logon_url;
	char			*cookie_name;
	u_char			allow_exceptions;
	struct structs_regex	exceptions_pattern;
};

union lws_servlet_info {
	struct lws_file_info		file;
	struct lws_xmlrpc_info		xmlrpc;
	struct lws_redirect_info	redirect;
	struct lws_basicauth_info	basicauth;
	struct lws_cookieauth_info	cookieauth;
};

DEFINE_STRUCTS_UNION(lws_servlet_info_union, lws_servlet_info);

struct lws_servlet {
	struct structs_regex		url_pattern;
	int				ordering;
	struct lws_servlet_info_union	info;
};

DEFINE_STRUCTS_ARRAY(lws_servlet_ary, struct lws_servlet);

struct lws_virthost {
	char			*server_name;
	u_char			default_virtual_host;
	struct lws_servlet_ary	servlets;
};

DEFINE_STRUCTS_ARRAY(lws_virthost_ary, struct lws_virthost);

struct lws_server_ssl {
	u_char			enabled;
	char			*private_key;
	char			*certificate;
};

struct lws_server {
	struct in_addr		ip;
	u_int16_t		port;
	struct lws_server_ssl	ssl;
	struct lws_virthost_ary	virtual_hosts;
};

DEFINE_STRUCTS_ARRAY(lws_server_ary, struct lws_server);

/* Name, value pairs for use by the servlets */
struct lws_property {
	char			*name;
	char			*value;
};

DEFINE_STRUCTS_ARRAY(lws_property_ary, struct lws_property);

/* Configuration object for the lws application */
struct lws_config {
	char			*pidfile;
	struct alog_config	error_log;
	struct lws_server_ary	servers;
	struct lws_property_ary	properties;
};

/* Variables */
extern const	struct structs_type lws_config_type;
extern struct	app_config_ctx *lws_config_ctx;
extern const	struct lws_config *const lws_curconf;

/* Functions */
extern int	lws_config_init(struct pevent_ctx *ctx, const char *path);

#endif	/* !_LWS_CONFIG_H_ */
