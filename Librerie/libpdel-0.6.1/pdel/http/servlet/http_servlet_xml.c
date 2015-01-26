
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
#include "structs/xml.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/xml.h"
#include "util/typed_mem.h"

#define MEM_TYPE	"http_servlet_xml"

struct http_servlet_xml_private {
	struct http_servlet_xml_info	info;	/* copy of 'info' struct */
	void				*arg;	/* handler arg */
	void				(*destroy)(void *);	/* arg d'tor */
};

/* Internal functions */
static http_servlet_run_t	http_servlet_xml_run;
static http_servlet_destroy_t	http_servlet_xml_destroy;

/*
 * Create a new http_servlet_xml servlet.
 */
struct http_servlet *
http_servlet_xml_create(const struct http_servlet_xml_info *info,
	void *arg, void (*destroy)(void *))
{
	struct http_servlet_xml_private *priv;
	struct http_servlet *servlet;

	/* Create servlet */
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_xml_run;
	servlet->destroy = http_servlet_xml_destroy;

	/* Create private info */
	if ((priv = MALLOC(MEM_TYPE, sizeof(*priv))) == NULL)
		goto fail;
	memset(priv, 0, sizeof(*priv));
	priv->arg = arg;
	priv->destroy = destroy;
	priv->info = *info;
	priv->info.ptag = NULL;
	priv->info.rtag = NULL;

	/* Copy info structure strings */
	if ((priv->info.ptag = STRDUP(MEM_TYPE, info->ptag)) == NULL)
		goto fail;
	if ((priv->info.rtag = STRDUP(MEM_TYPE, info->rtag)) == NULL)
		goto fail;

	/* Done */
	servlet->arg = priv;
	return (servlet);

fail:
	/* Clean up after failure */
	if (priv != NULL) {
		FREE(MEM_TYPE, (char *)priv->info.ptag);
		FREE(MEM_TYPE, (char *)priv->info.rtag);
		FREE(MEM_TYPE, priv);
	}
	FREE(MEM_TYPE, servlet);
	return (NULL);
}

/*
 * XML message processing servlet.
 */
static int
http_servlet_xml_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct http_servlet_xml_private *const priv = servlet->arg;
	struct http_servlet_xml_info *const info = &priv->info;
	const char *method = http_request_get_method(req);
	FILE *const input = http_request_get_input(req);
	FILE *const output = http_response_get_output(resp, 1);
	void *payload = NULL;
	void *reply = NULL;
	char *attrs = NULL;
	char *rattrs = NULL;

	/* Check method */
	if (!((info->allow_post && strcmp(method, HTTP_METHOD_POST) == 0)
	    || (info->allow_get && strcmp(method, HTTP_METHOD_GET) == 0))) {
		http_response_send_error(resp,
		    HTTP_STATUS_METHOD_NOT_ALLOWED, NULL);
		return (1);
	}

	/* Get request structure (POST only) */
	if (strcmp(method, HTTP_METHOD_POST) == 0) {
		if ((payload = MALLOC(TYPED_MEM_TEMP,
		    info->ptype->size)) == NULL) {
			(*info->logger)(LOG_ERR, "%s: %s",
			    "malloc", strerror(errno));
			goto server_error;
		}
		if (structs_xml_input(info->ptype, info->ptag, &attrs,
		    TYPED_MEM_TEMP, input, payload, STRUCTS_XML_UNINIT,
		    info->logger) == -1) {
			(*info->logger)(LOG_ERR, "%s: %s",
			    "structs_xml_input", strerror(errno));
			FREE(TYPED_MEM_TEMP, payload);
			payload = NULL;
			goto server_error;
		}
#ifdef XML_RPC_DEBUG
		printf("%s: read this XML input from http_request %p:\n",
		    __FUNCTION__, req);
		(void)structs_xml_output(info->ptype, info->ptag,
		    attrs, payload, stdout, NULL, 0);
#endif
	}

	/* Invoke handler */
	if ((reply = (*info->handler)(priv->arg, req,
	    payload, attrs, &rattrs, TYPED_MEM_TEMP)) == NULL) {
		(*info->logger)(LOG_ERR, "%s: %s",
		    "error from handler", strerror(errno));
		goto server_error;
	}

	/* Set MIME type */
	http_response_set_header(resp, 0, HTTP_HEADER_CONTENT_TYPE, "text/xml");

#ifdef XML_RPC_DEBUG
	printf("%s: writing this XML output back to http_response %p:\n",
	    __FUNCTION__, resp);
	(void)structs_xml_output(info->rtype,
	    info->rtag, rattrs, reply, stdout, NULL, info->flags);
#endif

	/* Write back response */
	if (structs_xml_output(info->rtype, info->rtag,
	    rattrs, reply, output, NULL, info->flags) == -1) {
		(*info->logger)(LOG_ERR, "%s: %s",
		    "error writing output", strerror(errno));
	}
	goto done;

server_error:
	http_response_send_errno_error(resp);

done:
	/* Clean up */
	if (payload != NULL) {
		structs_free(info->ptype, NULL, payload);
		FREE(TYPED_MEM_TEMP, payload);
	}
	if (reply != NULL) {
		structs_free(info->rtype, NULL, reply);
		FREE(TYPED_MEM_TEMP, reply);
	}
	FREE(TYPED_MEM_TEMP, attrs);
	FREE(TYPED_MEM_TEMP, rattrs);
	return (1);
}

static void
http_servlet_xml_destroy(struct http_servlet *servlet)
{
	struct http_servlet_xml_private *const priv = servlet->arg;

	if (priv->destroy != NULL)
		(*priv->destroy)(priv->arg);
	FREE(MEM_TYPE, (char *)priv->info.ptag);
	FREE(MEM_TYPE, (char *)priv->info.rtag);
	FREE(MEM_TYPE, priv);
	FREE(MEM_TYPE, servlet);
}

