
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
#include "http/servlet/basicauth.h"
#include "util/typed_mem.h"

#define MEM_TYPE		"http_servlet_basicauth"

static http_servlet_run_t	http_servlet_basicauth_run;
static http_servlet_destroy_t	http_servlet_basicauth_destroy;

struct http_servlet_basicauth {
	http_servlet_basicauth_t	*auth;
	void				(*destroy)(void *);
	void				*arg;
};

/*
 * Create a new auth servlet.
 */
struct http_servlet *
http_servlet_basicauth_create(http_servlet_basicauth_t *auth,
	void *arg, void (*destroy)(void *))
{
	struct http_servlet_basicauth *info;
	struct http_servlet *servlet;

	/* Sanity check */
	if (auth == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	/* Create servlet */
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_basicauth_run;
	servlet->destroy = http_servlet_basicauth_destroy;

	/* Add info */
	if ((info = MALLOC(MEM_TYPE, sizeof(*info))) == NULL) {
		FREE(MEM_TYPE, servlet);
		return (NULL);
	}
	memset(info, 0, sizeof(*info));
	info->auth = auth;
	info->arg = arg;
	info->destroy = destroy;
	servlet->arg = info;

	/* Done */
	return (servlet);
}

/*
 * Execute authorization servlet.
 */
static int
http_servlet_basicauth_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct http_servlet_basicauth *const info = servlet->arg;
	const char *username;
	const char *password;
	const char *realm;

	/* Get username and password */
	if ((username = http_request_get_username(req)) == NULL)
		username = "";
	if ((password = http_request_get_password(req)) == NULL)
		password = "";

	/* Check authorization and return error if it fails */
	if ((realm = (*info->auth)(info->arg,
	    req, username, password)) != NULL) {
		http_response_send_basic_auth(resp, realm);
		return (1);
	}

	/* Continue */
	return (0);
}

/*
 * Destroy an auth servlet.
 */
static void
http_servlet_basicauth_destroy(struct http_servlet *servlet)
{
	struct http_servlet_basicauth *const info = servlet->arg;

	if (info->destroy != NULL)
		(*info->destroy)(info->arg);
	FREE(MEM_TYPE, servlet->arg);
	FREE(MEM_TYPE, servlet);
}

