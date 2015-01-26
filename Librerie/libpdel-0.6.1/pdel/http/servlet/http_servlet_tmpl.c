
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
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
#include <pthread.h>

#include <openssl/ssl.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "tmpl/tmpl.h"
#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/tmpl.h"
#include "util/typed_mem.h"

#define MEM_TYPE	"http_servlet_tmpl"

/* Template servlet private context */
struct tmpl_private {
	struct http_servlet_tmpl_info	info;	/* static template info */
	struct tmpl			*tmpl;	/* parsed template */
	struct timespec			mtime;	/* last mod time of file */
	pthread_rwlock_t		lock;	/* servlet r/w lock */
};

/* Private info per servlet instantiation */
struct tmpl_instance {
	struct tmpl_private		*priv;	/* back pointer to servlet */
	struct tmpl_ctx			*ctx;	/* tmpl exectution context */
	struct http_servlet_tmpl_arg	targ;	/* arg for handler funcs */
};

/* Internal functions */
static http_servlet_run_t	http_servlet_tmpl_run;
static http_servlet_destroy_t	http_servlet_tmpl_destroy;

static void	http_servlet_tmpl_run_cleanup(void *arg);

/*
 * Create a new template servlet.
 */
struct http_servlet *
http_servlet_tmpl_create(struct http_servlet_tmpl_info *info)
{
	struct http_servlet *servlet = NULL;
	struct tmpl_private *priv = NULL;

	/* Create servlet */
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		goto fail;
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_tmpl_run;
	servlet->destroy = http_servlet_tmpl_destroy;

	/* Initialize private info */
	if ((priv = MALLOC(MEM_TYPE, sizeof(*priv))) == NULL)
		goto fail;
	memset(priv, 0, sizeof(*priv));
	if (info->path != NULL
	    && (priv->info.path = STRDUP(MEM_TYPE, info->path)) == NULL)
		goto fail;
	if (info->mime_type != NULL
	    && (priv->info.mime_type = STRDUP(MEM_TYPE,
	      info->mime_type)) == NULL)
		goto fail;
	if (info->mime_encoding != NULL
	    && (priv->info.mime_encoding = STRDUP(MEM_TYPE,
	      info->mime_encoding)) == NULL)
		goto fail;
	priv->info.logger = info->logger;
	if (_http_servlet_tmpl_copy_tinfo(&priv->info.tinfo,
	    &info->tinfo) == -1)
		goto fail;
	pthread_rwlock_init(&priv->lock, NULL);
	servlet->arg = priv;

	/* OK */
	return (servlet);

fail:
	if (priv != NULL) {
		FREE(MEM_TYPE, (char *)priv->info.path);
		FREE(MEM_TYPE, (char *)priv->info.mime_type);
		FREE(MEM_TYPE, (char *)priv->info.mime_encoding);
		_http_servlet_tmpl_free_tinfo(&priv->info.tinfo);
		FREE(MEM_TYPE, priv);
	}
	if (servlet != NULL)
		FREE(MEM_TYPE, servlet);
	return (NULL);
}

/*
 * Destroy template servlet.
 */
static void
http_servlet_tmpl_destroy(struct http_servlet *servlet)
{
	struct tmpl_private *const priv = servlet->arg;
	struct http_servlet_tmpl_info *const info = &priv->info;
	int r;

	/* Acquire the lock to avoid race condition */
	r = pthread_rwlock_wrlock(&priv->lock);
	assert(r == 0);

	/* Destroy user argument */
	if (info->tinfo.freer != NULL)
		(*info->tinfo.freer)(info->tinfo.arg);

	/* Destroy template context */
	tmpl_destroy(&priv->tmpl);

	/* Release the lock */
	r = pthread_rwlock_unlock(&priv->lock);
	assert(r == 0);

	/* Free structure */
	FREE(MEM_TYPE, (char *)info->path);
	FREE(MEM_TYPE, (char *)info->mime_type);
	FREE(MEM_TYPE, (char *)info->mime_encoding);
	_http_servlet_tmpl_free_tinfo(&info->tinfo);
	FREE(MEM_TYPE, priv);
	FREE(MEM_TYPE, servlet);
}

/*
 * Run template servlet
 */
static int
http_servlet_tmpl_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct tmpl_private *const priv = servlet->arg;
	struct http_servlet_tmpl_info *const info = &priv->info;
	struct http_servlet_tmpl_tinfo *const tinfo = &priv->info.tinfo;
	struct tmpl_instance *this = NULL;
	FILE *output = NULL;
	const char *hval;
	struct stat sb;
	int num_errors;
	int r;

	/* Construct per-instance state */
	if ((this = MALLOC(MEM_TYPE, sizeof(*this))) == NULL) {
		(*info->logger)(LOG_ERR, "%s: %s: %s",
		    __FUNCTION__, "malloc", strerror(errno));
		return (-1);
	}
	memset(this, 0, sizeof(*this));
	this->priv = priv;

	/* Grab lock to avoid race with http_servlet_tmpl_destroy() */
	r = pthread_rwlock_rdlock(&priv->lock);
	assert(r == 0);

	/* Push cleanup hook in case thread gets canceled */
	pthread_cleanup_push(http_servlet_tmpl_run_cleanup, this);

	/* Get servlet output stream (buffered) */
	if ((output = http_response_get_output(resp, 1)) == NULL) {
		(*info->logger)(LOG_ERR, "can't get template output: %s",
		    strerror(errno));
		goto fail_errno;
	}

	/* Set MIME type */
	if (info->mime_type == NULL) {
		http_response_set_header(resp, 0,
		    HTTP_HEADER_CONTENT_TYPE, "text/html; charset=iso-8859-1");
	} else {
		http_response_set_header(resp, 0,
		    HTTP_HEADER_CONTENT_TYPE, "%s", info->mime_type);
		if (info->mime_encoding != NULL) {
			http_response_set_header(resp,
			0, HTTP_HEADER_CONTENT_ENCODING,
			    "%s", info->mime_encoding);
		}
	}

	/* Assume servlet output is not cachable */
	http_response_set_header(resp, 1, HTTP_HEADER_PRAGMA, "no-cache");
	http_response_set_header(resp, 0,
	    HTTP_HEADER_CACHE_CONTROL, "no-cache");

	/* Get modification timestamp of the template file */
	if (stat(info->path, &sb) == -1) {
		(*info->logger)(LOG_ERR, "%s: %s: %s",
		    __FUNCTION__, info->path, strerror(errno));
		memset(&sb.st_mtime, 0, sizeof(sb.st_mtime));
	}

	/* Invalidate cached template if template file has changed */
	if (priv->tmpl != NULL
	    && memcmp(&sb.st_mtime, &priv->mtime, sizeof(priv->mtime)) != 0) {
		(*info->logger)(LOG_INFO,
		    "template \"%s\" was updated", info->path);
		tmpl_destroy(&priv->tmpl);
	}

	/* Do we need to (re)parse the template? */
	if (priv->tmpl == NULL) {

		/* Parse template file */
		if ((priv->tmpl = tmpl_create_mmap(info->path,
		    &num_errors, tinfo->mtype)) == NULL) {
			(*info->logger)(LOG_ERR,
			    "can't create template from \"%s\": %s",
			    info->path, strerror(errno));
			goto fail_errno;
		}

		/* Check for an error from tmpl_create() */
		if (priv->tmpl == NULL) {
			(*info->logger)(LOG_ERR,
			    "can't create \"%s\" template: %s", info->path,
			    strerror(errno));
			goto fail_errno;
		}

		/* Warn if there were any parse errors */
		if (num_errors != 0) {
			(*info->logger)(LOG_WARNING,
			    "%d parse error%s in template \"%s\"",
			    num_errors, num_errors == 1 ? "" : "s",
			    info->path);
		}

		/* Update last modified time */
		memcpy(&priv->mtime, &sb.st_mtime, sizeof(priv->mtime));
	}

	/* Read URL-encoded form data if this is a normal POST */
	if (strcmp(http_request_get_method(req), HTTP_METHOD_POST) == 0
	    && (hval = http_request_get_header(req,
	      HTTP_HEADER_CONTENT_TYPE)) != NULL
	    && strcasecmp(hval, HTTP_CTYPE_FORM_URLENCODED) == 0) {
		if (http_request_read_url_encoded_values(req) == -1) {
			(*info->logger)(LOG_ERR,
			    "error reading %s data for \"%s\" template: %s",
			    HTTP_METHOD_POST, info->path, strerror(errno));
			goto fail_errno;
		}
	}

	/* Fill in handler function cookie */
	this->targ.arg = info->tinfo.arg;
	this->targ.req = req;
	this->targ.resp = resp;

	/* Create tmpl execution context */
	if ((this->ctx = tmpl_ctx_create(&this->targ,
	    tinfo->mtype, tinfo->handler, tinfo->errfmtr)) == NULL) {
		(*info->logger)(LOG_ERR, "%s: %s: %s",
		    __FUNCTION__, "tmpl_ctx_create", strerror(errno));
		goto fail_errno;
	}

	/* Execute template */
	if (tmpl_execute(priv->tmpl, this->ctx, output, tinfo->flags) == -1) {
		(*info->logger)(LOG_ERR, "can't execute \"%s\" template: %s",
		    info->path, strerror(errno));
		goto fail_errno;
	}

	/* OK */
	goto done;

fail_errno:
	/* Fail with appropriate error response */
	http_response_send_errno_error(resp);

done:
	/* Done */
	if (output != NULL)
		fclose(output);
	pthread_cleanup_pop(1);
	return (1);
}

/*
 * Cleanup after http_servlet_tmpl_run().
 */
static void
http_servlet_tmpl_run_cleanup(void *arg)
{
	struct tmpl_instance *const this = arg;
	int r;

	if (this->ctx != NULL)
		tmpl_ctx_destroy(&this->ctx);
	r = pthread_rwlock_unlock(&this->priv->lock);
	assert(r == 0);
	FREE(MEM_TYPE, this);
}

/*
 * Copy a 'tinfo' structure.
 */
int
_http_servlet_tmpl_copy_tinfo(struct http_servlet_tmpl_tinfo *dst,
	const struct http_servlet_tmpl_tinfo *src)
{

	/* Copy stuff */
	memset(dst, 0, sizeof(*dst));
	dst->flags = src->flags;
	dst->handler = src->handler;
	dst->errfmtr = src->errfmtr;
	if (src->mtype != NULL
	    && (dst->mtype = STRDUP(MEM_TYPE, src->mtype)) == NULL)
		goto fail;
	dst->arg = src->arg;
	dst->freer = src->freer;

	/* Done */
	return (0);

fail:
	/* Clean up after failure */
	FREE(MEM_TYPE, (char *)dst->mtype);
	memset(dst, 0, sizeof(*dst));
	return (-1);
}

/*
 * Free a copied 'tinfo' structure.
 */
void
_http_servlet_tmpl_free_tinfo(struct http_servlet_tmpl_tinfo *tinfo)
{
	FREE(MEM_TYPE, (char *)tinfo->mtype);
	memset(tinfo, 0, sizeof(*tinfo));
}

/************************************************************************
			    TMPL USER FUNCTIONS
************************************************************************/

char *
http_servlet_tmpl_func_query(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *value;

	if (ac != 2) {
		errno = EINVAL;
		return (NULL);
	}
	if ((value = http_request_get_value(arg->req, av[1], 0)) == NULL)
		value = "";
	return (STRDUP(mtype, value));
}

char *
http_servlet_tmpl_func_query_exists(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[2];

	if (ac != 2) {
		errno = EINVAL;
		return (NULL);
	}
	snprintf(buf, sizeof(buf), "%d",
	    http_request_get_value(arg->req, av[1], 0) != NULL);
	return (STRDUP(mtype, buf));
}

char *
http_servlet_tmpl_func_query_string(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *eqs = http_request_get_query_string(arg->req);
	char *dqs;

	if (ac != 1) {
		errno = EINVAL;
		return (NULL);
	}
	/* URL-decode query string */
	if ((dqs = MALLOC(mtype, strlen(eqs) + 1)) == NULL)
		return (NULL);
	http_request_url_decode(eqs, dqs);

	/* Return it */
	return (dqs);
}

char *
http_servlet_tmpl_func_get_header(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *hval;

	if (ac != 2) {
		errno = EINVAL;
		return (NULL);
	}
	if ((hval = http_request_get_header(arg->req, av[1])) == NULL)
		hval = "";
	return (STRDUP(mtype, hval));
}

char *
http_servlet_tmpl_func_set_header(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	if (ac != 3) {
		errno = EINVAL;
		return (NULL);
	}
	if (http_response_set_header(arg->resp, 0, av[1], "%s", av[2]) == -1)
		return (NULL);
	return (STRDUP(mtype, ""));
}

char *
http_servlet_tmpl_func_remove_header(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	if (ac != 2) {
		errno = EINVAL;
		return (NULL);
	}
	(void)http_response_remove_header(arg->resp, av[1]);
	return (STRDUP(mtype, ""));
}

char *
http_servlet_tmpl_func_unbuffer(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	if (ac != 1) {
		errno = EINVAL;
		return (NULL);
	}
	http_response_send_headers(arg->resp, 1);
	return (STRDUP(mtype, ""));
}

char *
http_servlet_tmpl_func_redirect(struct tmpl_ctx *ctx,
	char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const arg = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	if (ac != 2) {
		errno = EINVAL;
		return (NULL);
	}
	if (http_response_set_header(arg->resp, 0,
	    HTTP_HEADER_LOCATION, "%s", av[1]) == -1)
		return (NULL);
	http_response_send_error(arg->resp, HTTP_STATUS_FOUND, NULL);
	return (STRDUP(mtype, ""));
}

