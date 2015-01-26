
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"
#include "lws_tmpl.h"
#include "lws_tmpl_auth.h"

/************************************************************************
		    COOKIE AUTHORIZATION FUNCTIONS
************************************************************************/

/* Our user-defined template functions */
static tmpl_handler_t	lws_tf_authname;
static tmpl_handler_t	lws_tf_login;
static tmpl_handler_t	lws_tf_logout;

/* User-defined template function descriptor list */
const struct lws_tmpl_func lws_tmpl_auth_functions[] = {
    LWS_TMPL_FUNC(login, 5, 8,
	"cookiename:resource:username:path:domain:linger:expire:persistent",
	"This function \"logs in\" the user's browser as user $3 so that"
"\n"	"subsequent calls to @authname($1, $2) will return that username for"
"\n"	"as long as the login remains valid. The username must be non-empty"
"\n"	"and is stored on the browser using a cryptographically signed HTTP"
"\n"	"cookie with name $1."
"\n"	"<p>"
"\n"	"$4 and $5 specify when the browser should send the cookie, see"
"\n"	"<a href=\"http://wp.netscape.com/newsref/std/cookie_spec.html\">the"
"\n"	"cookie spec</a> for details; an empty string for $4 means \"/\";"
"\n"	"an empty string for $5 means use the default domain."
"\n"	"<p>"
"\n"	"$6 is the linger timeout (in seconds), which is how long between"
"\n"	"accesses to the UI the browser can go before the authentication"
"\n"	"is deemed invalid. If omitted or zero, no linger timeout is applied."
"\n"	"<p>"
"\n"	"$7 is the expiration timeout (in seconds), which is how long in"
"\n"	"absolute terms the authorization is good for.  This value must be"
"\n"	"greater than zero. If omitted, no expiration time is applied."
"\n"	"<p>"
"\n"	"If $8 is omitted or zero, the browser is instructed to discard the"
"\n"	"cookie when the browser session ends."),
    LWS_TMPL_FUNC(logout, 1, 1, "cookiename",
	"Invalidate any cookie authentication using the cookie named $1."),
    LWS_TMPL_FUNC(authname, 2, 2, "cookiename:resource",
	"Retrieve the authenticated username from the browser-supplied cookie"
"\n"	"named $1 for the web server resource $2. See also"
"\n"	"<a href=\"#login\">@login()</a>."),
    { { NULL } }
};

/*
 * Supply a cookie to the user that authorizes them.
 *
 * Usage:
 *	@login(cookiename, resource, username,
 *		path, domain, [, linger [, expire [, persistent]]])
 */
static char *
lws_tf_login(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const struct lws_tmpl_info *const tfi = targ->arg;
	const struct lws_server *const s = tfi->server;
	const char *const cookiename = av[1];
	const char *const resource = av[2];
	const char *const username = av[3];
	const char *const path = (*av[4] != '\0') ? av[4] : NULL;
	const char *const domain = (*av[5] != '\0') ? av[5] : NULL;
	u_long linger = (ac >= 7) ? strtoul(av[6], NULL, 10) : 0;
	u_long expire = (ac >= 8) ? strtoul(av[7], NULL, 10) : 0;
	int persistent = (ac >= 9) ? (atoi(av[8]) != 0) : 0;
	const time_t now = time(NULL);

	/* Check username is non-empty */
	if (*username == '\0') {
		ASPRINTF(mtype, errmsgp, "username must be non-empty", "");
		return (NULL);
	}

	/* Get linger and expire times */
	if ((time_t)linger < 0) {
		ASPRINTF(mtype, errmsgp,
		    "invalid %s time %ld", "linger" _ (long)(time_t)linger);
		return (NULL);
	}
	if (now + (time_t)expire <= 0) {
		ASPRINTF(mtype, errmsgp,
		    "invalid %s time %ld", "expire" _ (long)(time_t)expire);
		return (NULL);
	}

	/* Login user */
	if (http_servlet_cookieauth_login(targ->resp, s->ssl.private_key,
	     username, linger, now + expire, 
	    persistent, (const unsigned char *) resource,
	    strlen(resource), cookiename, path, domain, 0) == -1)
		return (NULL);

	/* OK */
	return (STRDUP(mtype, ""));
}

/*
 * Logout user.
 *
 * Usage:
 *	@logout(cookiename)
 */
static char *
lws_tf_logout(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	/* Logout user */
	if (http_servlet_cookieauth_logout(av[1], NULL, NULL, targ->resp) == -1)
		return (NULL);
	return (STRDUP(mtype, ""));
}

/*
 * Get the authorized username, or emtpy string if none.
 *
 * Usage: @authname(cookiename, resource)
 */
static char *
lws_tf_authname(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const struct lws_tmpl_info *const tfi = targ->arg;
	const struct lws_server *const s = tfi->server;
	char *name;

	if ((name = http_servlet_cookieauth_user(s->ssl.private_key,
	      av[2], strlen(av[2]), av[1], targ->req, mtype)) == NULL
	    && errno == EACCES)
		name = STRDUP(mtype, "");
	return (name);
}

