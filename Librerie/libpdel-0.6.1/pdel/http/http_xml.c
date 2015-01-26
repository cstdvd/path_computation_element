
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>

#include <openssl/ssl.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/union.h"
#include "structs/xml.h"
#include "structs/xmlrpc.h"

#include "io/boundary_fp.h"
#include "io/string_fp.h"
#include "sys/alog.h"
#include "util/typed_mem.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/xml.h"

#define MEM_TYPE		"http_xml_send_xmlrpc"

/* Context for http_xml_send_xmlrpc() */
struct http_xml_send_xmlrpc_ctx {
	struct xmlrpc_request		*xreq;
	struct xmlrpc_response_union	*xrep;
};

/*
 * Internal functions
 */
static void	http_xml_send_xmlrpc_cleanup(void *arg);
static void	http_xml_send_cleanup(void *arg);

/*
 * Send an XML-RPC message
 *
 * Returns:
 *	 0	Success
 *	-1	System error, errno is set
 *	-2	XML-RPC fault received
 *
 * The reply 'rep' may be NULL to ignore any reply value, otherwise
 * it should already be initialized.
 *
 * If an XML-RPC fault is received, and "faultp" is not NULL, then
 * it will be initialized with the fault received.
 *
 * This properly handles the calling thread's being canceled.
 */
int
http_xml_send_xmlrpc(struct http_client *client,
	struct in_addr ip, u_int16_t port, int https,
	const char *username, const char *password,
	const char *methodName, u_int nparams,
	const struct structs_type **ptypes, const void **pdatas,
	const struct structs_type *rep_type, void *rep,
	struct xmlrpc_compact_fault *faultp, structs_xmllog_t *rlogger)
{
	return http_xml_send_xmlrpc2(client, ip, port, https,
	    username, password, XML_RPC_URL, methodName, nparams,
	    ptypes, pdatas, rep_type, rep, faultp, rlogger);
}

/*
 * Same as http_xml_send_xmlrpc() but with user-defined URL path.
 */
int
http_xml_send_xmlrpc2(struct http_client *client,
	struct in_addr ip, u_int16_t port, int https,
	const char *username, const char *password, const char *urlpath,
	const char *methodName, u_int nparams,
	const struct structs_type **ptypes, const void **pdatas,
	const struct structs_type *rep_type, void *rep,
	struct xmlrpc_compact_fault *faultp, structs_xmllog_t *rlogger)
{
	const struct structs_type *const xreq_type
	    = &structs_type_xmlrpc_request;
	const struct structs_type *const xrep_type
	    = &structs_type_xmlrpc_response;
	const struct structs_type *const ftype
	    = &structs_type_xmlrpc_compact_fault;
	struct http_xml_send_xmlrpc_ctx *ctx;
	struct xmlrpc_compact_fault fault;
	char ebuf[128];
	int ret = -1;
	int r;

	/* Get context */
	if ((ctx = MALLOC(MEM_TYPE, sizeof(*ctx))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		return (-1);
	}
	memset(ctx, 0, sizeof(*ctx));
	pthread_cleanup_push(http_xml_send_xmlrpc_cleanup, ctx);

	/* Build XML-RPC request and reply */
	if ((ctx->xreq = structs_xmlrpc_build_request(MEM_TYPE,
	    methodName, nparams, ptypes, pdatas)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_xmlrpc_build_request");
		goto fail;
	}
	if ((ctx->xrep = MALLOC(MEM_TYPE, xrep_type->size)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		goto fail;
	}
	if (structs_init(xrep_type, NULL, ctx->xrep) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_init");
		FREE(MEM_TYPE, ctx->xrep);
		ctx->xrep = NULL;
		goto fail;
	}

#ifdef XML_RPC_DEBUG
	printf("%s: sending this XML-RPC request:\n", __FUNCTION__);
	(void)structs_xml_output(xreq_type, XML_RPC_REQUEST_TAG,
	    NULL, ctx->xreq, stdout, NULL, 0);
#endif

	/* Send request and get reply; note: we could get canceled here. */
	r = http_xml_send(client, ip, port, https, urlpath,
	    username, password, XML_RPC_REQUEST_TAG, NULL, xreq_type,
	    ctx->xreq, STRUCTS_XML_FULL, XML_RPC_REPLY_TAG, NULL, NULL,
	    xrep_type, ctx->xrep, 0, rlogger);

#ifdef XML_RPC_DEBUG
	printf("%s: got this XML-RPC reply (error=%s):\n",
	    __FUNCTION__, r == -1 ? strerror(errno) : "none");
	(void)structs_xml_output(xrep_type, XML_RPC_REPLY_TAG,
	    NULL, ctx->xrep, stdout, NULL, 0);
#endif

	/* Check error */
	if (r == -1)
		goto fail;

	/* Check for fault */
	if (structs_init(ftype, NULL, &fault) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_init");
		goto fail;
	}
	if (structs_xmlrpc2struct(xrep_type, ctx->xrep,
	    "fault.value", ftype, &fault, NULL, NULL, 0) == 0) {
		if (faultp != NULL)
			*faultp = fault;
		else
			structs_free(ftype, NULL, &fault);
		ret = -2;			/* -2 indicates fault */
		goto fail;
	}
	structs_free(ftype, NULL, &fault);

	/* Extract response (if desired) */
	if (rep != NULL) {
		if (rep_type != NULL) {		/* return compact type */
			if (structs_xmlrpc2struct(xrep_type, ctx->xrep,
			    "params.0.value", rep_type, rep, NULL, ebuf,
			    sizeof(ebuf)) == -1) {
				(*rlogger)(LOG_ERR, "error decoding XML-RPC"
				    " response: %s", ebuf);
				goto fail;
			}
		} else {			/* return exploded type */
			if (structs_get(&structs_type_xmlrpc_value,
			    "params.0.value", ctx->xrep, rep) == -1) {
				alogf(LOG_ERR, "structs_get: %m%s", "");
				goto fail;
			}
		}
	}

	/* OK */
	ret = 0;

fail:;
	/* Done */
	pthread_cleanup_pop(1);
	return (ret);
}

/*
 * Cleanup for http_xml_send_xmlrpc()
 */
static void
http_xml_send_xmlrpc_cleanup(void *arg)
{
	struct http_xml_send_xmlrpc_ctx *const ctx = arg;
	const struct structs_type *const xreq_type
	    = &structs_type_xmlrpc_request;
	const struct structs_type *const xrep_type
	    = &structs_type_xmlrpc_response;

	if (ctx->xreq != NULL) {
		structs_free(xreq_type, NULL, ctx->xreq);
		FREE(MEM_TYPE, ctx->xreq);
	}
	if (ctx->xrep != NULL) {
		structs_free(xrep_type, NULL, ctx->xrep);
		FREE(MEM_TYPE, ctx->xrep);
	}
	FREE(MEM_TYPE, ctx);
}

/*
 * Send a copy of the message, wait for a reply, and return the reply.
 *
 * The "reply" should already be initialized.
 *
 * This properly handles the calling thread's being canceled.
 */
int
http_xml_send(struct http_client *client, struct in_addr ip,
	u_int16_t port, int https, const char *urlpath, const char *username,
	const char *password, const char *ptag, const char *pattrs,
	const struct structs_type *ptype, const void *payload, int pflags,
	const char *rtag, char **rattrsp, const char *rattrs_mtype,
	const struct structs_type *rtype, void *reply, int rflags,
	structs_xmllog_t *rlogger)
{
	struct http_client_connection *cc;
	struct http_request *req;
	struct http_response *resp;
	int ret = -1;
	u_int code;
	FILE *fp;

	/* Get HTTP connection */
	if ((cc = http_client_connect(client, ip, port, https)) == NULL) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "get HTTP client" _ inet_ntoa(ip) _ port);
		return -1;
	}

	/* Push cleanup hook */
	pthread_cleanup_push(http_xml_send_cleanup, cc);

	/* Set up request */
	req = http_client_get_request(cc);
	if (http_request_set_method(req,
	    payload != NULL ? HTTP_METHOD_POST : HTTP_METHOD_GET) == -1) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "set method" _ inet_ntoa(ip) _ port);
		goto fail;
	}
	if (http_request_set_path(req, urlpath) == -1) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "set path" _ inet_ntoa(ip) _ port);
		goto fail;
	}
	if (http_request_set_header(req, 0, "Content-Type", "text/xml") == -1) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "set content-type" _ inet_ntoa(ip) _ port);
		goto fail;
	}
	if (username != NULL && password != NULL) {
		char *auth;

		if ((auth = http_request_encode_basic_auth(TYPED_MEM_TEMP,
		    username, password)) == NULL) {
			alogf(LOG_ERR, "can't %s for %s:%u: %m",
			    "encode authorization" _ inet_ntoa(ip) _ port);
			goto fail;
		}
		if (http_request_set_header(req, 0, "Authorization",
		    "Basic %s", auth) == -1) {
			alogf(LOG_ERR, "can't %s for %s:%u: %m",
			    "set authorization header" _ inet_ntoa(ip) _ port);
			FREE(TYPED_MEM_TEMP, auth);
			goto fail;
		}
		FREE(TYPED_MEM_TEMP, auth);
	}

	/* Write XML data to HTTP client output stream */
	if (payload != NULL) {
		if ((fp = http_request_get_output(req, 1)) == NULL) {
			alogf(LOG_ERR, "can't %s for %s:%u: %m",
			    "get output" _ inet_ntoa(ip) _ port);
			goto fail;
		}
		if (structs_xml_output(ptype,
		    ptag, pattrs, payload, fp, NULL, pflags) == -1) {
			alogf(LOG_ERR, "can't %s for %s:%u: %m",
			    "write XML" _ inet_ntoa(ip) _ port);
			goto fail;
		}
	}

	/* Get response */
	if ((resp = http_client_get_response(cc)) == NULL) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "get response" _ inet_ntoa(ip) _ port);
		goto fail;
	}
	if ((code = http_response_get_code(resp)) != HTTP_STATUS_OK) {
		alogf(LOG_ERR, "rec'd HTTP error code %d from"
		      "http%s://%s:%u%s: %s", code _ https ? "s" : "" _
		      inet_ntoa(ip) _ port _ urlpath _
		      http_response_status_msg(code));
		goto fail;
	}

	/* Read XML reply from client input stream */
	if ((fp = http_response_get_input(resp)) == NULL) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "get input" _ inet_ntoa(ip) _ port);
		goto fail;
	}
	if (structs_xml_input(rtype, rtag, rattrsp,
	    rattrs_mtype, fp, reply, rflags, rlogger) == -1) {
		alogf(LOG_ERR, "can't %s for %s:%u: %m",
		    "read XML reply" _ inet_ntoa(ip) _ port);
		goto fail;
	}

	/* OK */
	ret = 0;

fail:;
	/* Done */
	pthread_cleanup_pop(1);
	return (ret);
}

/*
 * Cleanup for http_xml_send()
 */
static void
http_xml_send_cleanup(void *arg)
{
	struct http_client_connection *cc = arg;

	http_client_close(&cc);
}

