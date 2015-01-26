
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>

#include <openssl/ssl.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/redirect.h"
#include "util/typed_mem.h"

#define MEM_TYPE		"http_servlet_redirect"

struct redirect_private {
	char		*url;
	int		append;
};

static http_servlet_run_t	http_servlet_redirect_run;
static http_servlet_destroy_t	http_servlet_redirect_destroy;

/*
 * Create a new redirect servlet.
 */
struct http_servlet *
http_servlet_redirect_create(const char *url, int append)
{
	struct http_servlet *servlet;
	struct redirect_private *priv;

	switch (append) {
	case HTTP_SERVLET_REDIRECT_NO_APPEND:
	case HTTP_SERVLET_REDIRECT_APPEND_QUERY:
	case HTTP_SERVLET_REDIRECT_APPEND_URI:
	case HTTP_SERVLET_REDIRECT_APPEND_URL:
		break;
	default:
		errno = EINVAL;
		return (NULL);
	}
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_redirect_run;
	servlet->destroy = http_servlet_redirect_destroy;
	if ((priv = MALLOC(MEM_TYPE, sizeof(*priv))) == NULL) {
		FREE(MEM_TYPE, servlet);
		return (NULL);
	}
	memset(priv, 0, sizeof(*priv));
	if ((priv->url = STRDUP(MEM_TYPE, url)) == NULL) {
		FREE(MEM_TYPE, priv);
		FREE(MEM_TYPE, servlet);
		return (NULL);
	}
	priv->append = append;
	servlet->arg = priv;
	return (servlet);
}

/*
 * Execute a redirect servlet.
 */
static int
http_servlet_redirect_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct redirect_private *const priv = servlet->arg;
	char *qstring;
	char *orig;

	/* If not appending anything, just send plain old redirect */
	if (priv->append == HTTP_SERVLET_REDIRECT_NO_APPEND) {
		if (http_response_set_header(resp, 0,
		    HTTP_HEADER_LOCATION, "%s", priv->url) == -1)
			return (-1);
		goto done;
	}

	/* If preserving existing request URI, append it to redirect URL */
	if (priv->append == HTTP_SERVLET_REDIRECT_APPEND_URI) {
		const char *const uri = http_request_get_uri(req);

		if (http_response_set_header(resp, 0,
		    HTTP_HEADER_LOCATION, "%s%s", priv->url,
		    uri + (priv->url[strlen(priv->url) - 1] == '/')) == -1)
			return (-1);
		goto done;
	}

	/* If preserving entire request URL, reconstruct and URL-encode */
	if (priv->append == HTTP_SERVLET_REDIRECT_APPEND_URL) {

		/* Reconstruct the original URL */
		ASPRINTF(TYPED_MEM_TEMP, &orig, "http%s://%s%s",
		    http_request_get_ssl(req) != NULL ? "s" : "" _
		    http_request_get_host(req) _ http_request_get_uri(req));
		if (orig == NULL)
			return (-1);

		/* URL-encode it so it can be (part of) new query string */
		if ((qstring = http_request_url_encode(TYPED_MEM_TEMP,
		    orig)) == NULL) {
			FREE(TYPED_MEM_TEMP, orig);
			return (-1);
		}
		FREE(TYPED_MEM_TEMP, orig);
	} else {
		const char *const qtemp = http_request_get_query_string(req);

		/* Just append the query string, not the entire URL */
		if (qtemp == NULL)
			return (-1);
		qstring = STRDUP(TYPED_MEM_TEMP, qtemp);
	}

	/* Set redirect with 'qstring' as additional query string argument(s) */
	if (http_response_set_header(resp, 0, HTTP_HEADER_LOCATION,
	    "%s%c%s", priv->url, strchr(priv->url, '?') != NULL ? '&' : '?',
	    qstring) == -1) {
		FREE(TYPED_MEM_TEMP, qstring);
		return (-1);
	}
	FREE(TYPED_MEM_TEMP, qstring);

done:
	/* Set Expires: header to workaround IE/Mac caching redirects bug */
	if (http_response_set_header(resp, 0, HTTP_HEADER_EXPIRES,
	    "Mon, 01 Jan 1980 08:00:00 GMT") == -1)
		return (-1);

	/* Send redirect response */
	http_response_send_error(resp, HTTP_STATUS_FOUND, NULL);
	return (1);
}

/*
 * Destroy a redirect servlet.
 */
static void
http_servlet_redirect_destroy(struct http_servlet *servlet)
{
	struct redirect_private *const priv = servlet->arg;

	FREE(MEM_TYPE, priv->url);
	FREE(MEM_TYPE, priv);
	FREE(MEM_TYPE, servlet);
}

