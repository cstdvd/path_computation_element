
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
#include "structs/type/struct.h"
#include "structs/type/union.h"
#include "structs/xml.h"
#include "structs/xmlrpc.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/xml.h"
#include "http/servlet/xmlrpc.h"
#include "util/typed_mem.h"

#define MEM_TYPE	"http_servlet_xmlrpc"

#define BOGUS_RTYPE	((const struct structs_type *)0x1832f0ad)

struct xmlrpc_state {
	struct http_servlet			*servlet;	/* servlet */
	struct http_servlet_xmlrpc_method	*methods;	/* methods */
	void					*arg;
	void					(*destroy)(void *);
	http_logger_t				*logger;
};

/* Internal functions */
static http_servlet_run_t	http_servlet_xmlrpc_run;
static http_servlet_destroy_t	http_servlet_xmlrpc_destroy;

static struct	http_servlet_xmlrpc_method *http_servlet_xmlrpc_copy_methods(
			const struct http_servlet_xmlrpc_method *methods);
static void	http_servlet_xmlrpc_free_methods(
			struct http_servlet_xmlrpc_method *methods);

static http_servlet_xml_handler_t	http_servlet_xmlrpc_handler;

/*
 * Create a new http_servlet_xmlrpc servlet.
 */
struct http_servlet *
http_servlet_xmlrpc_create(const struct http_servlet_xmlrpc_info *info,
	void *arg, void (*destroy)(void *))
{
	struct http_servlet_xml_info xinfo;
	struct http_servlet *servlet;
	struct xmlrpc_state *state;

	/* Create servlet */
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_xmlrpc_run;
	servlet->destroy = http_servlet_xmlrpc_destroy;

	/* Create servlet state structure */
	if ((state = MALLOC(MEM_TYPE, sizeof(*state))) == NULL)
		goto fail;
	memset(state, 0, sizeof(*state));
	state->arg = arg;
	state->destroy = destroy;
	state->logger = info->logger;

	/* Copy method list */
	if ((state->methods
	    = http_servlet_xmlrpc_copy_methods(info->methods)) == NULL)
		goto fail;

	/* Set up info for the more general http_servlet_xml servlet */
	memset(&xinfo, 0, sizeof(xinfo));
	xinfo.handler = http_servlet_xmlrpc_handler;
	xinfo.ptag = XML_RPC_REQUEST_TAG;
	xinfo.ptype = &structs_type_xmlrpc_request;
	xinfo.rtag = XML_RPC_REPLY_TAG;
	xinfo.rtype = &structs_type_xmlrpc_response;
	xinfo.allow_post = 1;
	xinfo.allow_get = 0;
	xinfo.logger = info->logger;
	xinfo.flags = STRUCTS_XML_FULL;

	/* Create "inner" XML servlet using 'xinfo' */
	if ((state->servlet = http_servlet_xml_create(&xinfo,
	    state, NULL)) == NULL)
		goto fail;

	/* Done */
	servlet->arg = state;
	return (servlet);

fail:
	/* Clean up after failure */
	if (state != NULL) {
		if (state->methods != NULL)
			http_servlet_xmlrpc_free_methods(state->methods);
		FREE(MEM_TYPE, state);
	}
	FREE(MEM_TYPE, servlet);
	return (NULL);
}

static int
http_servlet_xmlrpc_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct xmlrpc_state *const state = servlet->arg;

	return ((*state->servlet->run)(state->servlet, req, resp));
}

static void
http_servlet_xmlrpc_destroy(struct http_servlet *servlet)
{
	struct xmlrpc_state *const state = servlet->arg;

	/* Destroy 'inner' servlet */
	http_server_destroy_servlet(&state->servlet);

	/* Destroy 'outer' servlet */
	if (state->destroy != NULL)
		(*state->destroy)(state->arg);
	http_servlet_xmlrpc_free_methods(state->methods);
	FREE(MEM_TYPE, state);
	FREE(MEM_TYPE, servlet);
}

/*
 * Our "wrapper" handler that converts an XML-RPC request/reply
 * into a more normal request/reply.
 */
static void *
http_servlet_xmlrpc_handler(void *arg, struct http_request *req,
	const void *payload, const char *pattrs, char **rattrsp,
	const char *mtype)
{
	const struct xmlrpc_state *const state = arg;
	const struct structs_type *const xrept = &structs_type_xmlrpc_response;
	const struct http_servlet_xmlrpc_method *method;
	const struct xmlrpc_request *const xreq = payload; /* XML-RPC request */
	void **params = NULL;			/* parameter list for handler */
	void *reply = NULL;			/* reply returned by handler */
	struct xmlrpc_response_union *xreply = NULL;	/* XML-RPC reply */
	const struct structs_type *rtype = NULL;/* handlers' reply type */
	void *ret = NULL;			/* this func's return value */
	int faulted = 0;			/* handler returned fault */
	char *errbuf = NULL;			/* error message, if any */
	int exploded_params = 0;		/* exploded parameters */
	int exploded_response = 0;		/* exploded response */
	int errno_save;				/* saved errno value */
	u_int i;

	/* Find method */
	for (method = state->methods; method->name != NULL
	    && *method->name != '\0'
	    && strcmp(method->name, xreq->methodName) != 0; method++);
	if (method->name == NULL) {
		ASPRINTF(TYPED_MEM_TEMP, &errbuf,
		    "method name \"%s\" not recognized", xreq->methodName);
		if (errbuf == NULL)
			goto fail;
		(*state->logger)(LOG_ERR, "XML-RPC: %s", errbuf);
		errno = ENOSYS;
		goto null_reply;
	}
	exploded_params = (method->ptypes == NULL);

	/* Check number of arguments */
	if (xreq->params.length < method->min_params
	    || xreq->params.length > method->max_params) {
		ASPRINTF(TYPED_MEM_TEMP, &errbuf,
		    "%d parameter(s) present for method \"%s\" which takes"
		    " between %u and %u parameter(s)", xreq->params.length _
		    xreq->methodName _ method->min_params _ method->max_params);
		if (errbuf == NULL)
			goto fail;
		(*state->logger)(LOG_ERR, "XML-RPC: %s", errbuf);
		errno = EINVAL;
		goto null_reply;
	}

	/* Get parameters from the XML-RPC request */
	if ((params = MALLOC(TYPED_MEM_TEMP,
	    xreq->params.length * sizeof(*params))) == NULL)
		goto null_reply;
	memset(params, 0, xreq->params.length * sizeof(*params));
	for (i = 0; i < xreq->params.length; i++) {
		char ebuf[256];

		/* Optionally leave parameter list "exploded" */
		if (exploded_params) {
			params[i] = &xreq->params.elems[i].value;
			continue;
		}

		/* "Compact" this parameter using structs_xmlrpc2struct() */
		if ((params[i] = MALLOC(TYPED_MEM_TEMP,
		    method->ptypes[i]->size)) == NULL)
			break;
		if (structs_init(method->ptypes[i], NULL, params[i]) == -1) {
			FREE(TYPED_MEM_TEMP, params[i]);
			params[i] = NULL;
			break;
		}
		if (structs_xmlrpc2struct(&structs_type_xmlrpc_value,
		    &xreq->params.elems[i].value, NULL, method->ptypes[i],
		    params[i], NULL, ebuf, sizeof(ebuf)) == -1) {
			errno_save = errno;
			ASPRINTF(TYPED_MEM_TEMP, &errbuf, "parameter #%d of"
			    " method \"%s\" has an invalid type or value: %s",
			    i + 1 _ xreq->methodName _ ebuf);
			if (errbuf == NULL)
				goto fail;
			(*state->logger)(LOG_ERR, "XML-RPC: %s", errbuf);
			errno = errno_save;
			break;
		}
	}
	if (i < xreq->params.length)
		goto null_reply;

	/* Invoke our simplified handler */
	rtype = BOGUS_RTYPE;
	reply = (*method->handler)(state->arg, xreq->methodName, req,
	    xreq->params.length, (const void **)params, TYPED_MEM_TEMP,
	    &rtype, &faulted);
	if (reply == NULL) {
		errno_save = errno;
		(*state->logger)(LOG_ERR,
		    "XML-RPC: handler for \"%s\" failed: %s",
		    xreq->methodName, strerror(errno));
		errno = errno_save;
		goto null_reply;
	}

	/* Sanity check returned reply type */
	if (rtype == BOGUS_RTYPE) {
		ASPRINTF(TYPED_MEM_TEMP, &errbuf, "handler didn't set 'rtype'",
			 "");
		if (errbuf == NULL)
			goto fail;
		(*state->logger)(LOG_ERR, "XML-RPC: handler for"
		    " \"%s\" failed: %s", xreq->methodName, errbuf);
		errno = ECONNABORTED;
		goto null_reply;
	}
	if (faulted && rtype != &structs_type_xmlrpc_compact_fault) {
		ASPRINTF(TYPED_MEM_TEMP, &errbuf, "handler returned a fault"
			 " but didn't set 'rtype' to"
			 " &structs_type_xmlrpc_compact_fault", "");
		if (errbuf == NULL)
			goto fail;
		(*state->logger)(LOG_ERR, "XML-RPC: handler for"
		    " \"%s\" failed: %s", xreq->methodName, errbuf);
		errno = ECONNABORTED;
		goto null_reply;
	}

	/* Check for "exploded" return value */
	if (rtype == NULL) {
		rtype = &structs_type_xmlrpc_value;
		exploded_response = 1;
	}

null_reply:
	/* Save errno (used to create the fault when/if reply == NULL) */
	errno_save = errno;

	/* Create the XML-RPC reply */
	if ((xreply = MALLOC(mtype, sizeof(*xreply))) == NULL)
		goto fail;
	if (structs_init(xrept, NULL, xreply) == -1) {
		FREE(mtype, xreply);
		xreply = NULL;
		goto fail;
	}

	/* If handler returned NULL, create a fault structure using errno */
	if (reply == NULL) {
		struct xmlrpc_compact_fault *fault;

		/* Create an initialize fault structure from error string */
		if ((fault = MALLOC(TYPED_MEM_TEMP, sizeof(*fault))) == NULL)
			goto fail;
		if (structs_init(&structs_type_xmlrpc_compact_fault,
		    NULL, fault) == -1) {
			FREE(TYPED_MEM_TEMP, fault);
			goto fail;
		}
		fault->faultCode = errno;
		if (structs_set_string(&structs_type_xmlrpc_compact_fault,
		    "faultString", errbuf != NULL ? errbuf :
		    strerror(errno_save), fault, NULL, 0) == -1)
			;			/* too bad, just ignore error */

		/* Now pretend like handler() itself returned the fault */
		reply = fault;
		rtype = &structs_type_xmlrpc_compact_fault;
		faulted = 1;
	}

	/* Set fault if error, otherwise set reply as XML-RPC reply parameter */
	if (faulted) {
		if (structs_struct2xmlrpc(rtype, reply,
		    NULL, xrept, xreply, "fault.value") == -1)
			goto fail;
	} else {
		if (structs_array_insert(xrept, "params", 0, xreply) == -1)
			goto fail;
		if (exploded_response) {
			if (structs_set(xrept, reply,
			    "params.0.value", xreply) == -1)	
				goto fail;
		} else if (structs_struct2xmlrpc(rtype, reply, NULL,
		    xrept, xreply, "params.0.value") == -1)
			goto fail;
	}

	/* Done, so return reply */
	ret = xreply;
	xreply = NULL;

fail:
	/* Cleanup and exit */
	errno_save = errno;
	if (params != NULL) {
		if (!exploded_params) {
			for (i = 0; i < xreq->params.length; i++) {
				if (params[i] != NULL) {
					structs_free(method->ptypes[i],
					    NULL, params[i]);
					FREE(TYPED_MEM_TEMP, params[i]);
				}
			}
		}
		FREE(TYPED_MEM_TEMP, params);
	}
	if (reply != NULL) {
		structs_free(rtype, NULL, reply);
		FREE(TYPED_MEM_TEMP, reply);
	}
	if (xreply != NULL) {
		structs_free(xrept, NULL, xreply);
		FREE(mtype, xreply);
	}
	if (errbuf != NULL)
		FREE(TYPED_MEM_TEMP, errbuf);
	errno = errno_save;
	return (ret);
}

/*
 * Copy an XML-RPC method list
 */
static struct http_servlet_xmlrpc_method *
http_servlet_xmlrpc_copy_methods(
	const struct http_servlet_xmlrpc_method * methods)
{
	const struct http_servlet_xmlrpc_method *method;
	struct http_servlet_xmlrpc_method *mcopy;
	int num;
	int i;

	/* Count the number of methods */
	for (num = 0, method = methods; method->name != NULL; num++, method++);

	/* Allocate an array */
	if ((mcopy = MALLOC(MEM_TYPE, (num + 1) * sizeof(*mcopy))) == NULL)
		return (NULL);
	memset(mcopy, 0, (num + 1) * sizeof(*mcopy));

	/* Make a 'deep' copy (except for structs types XXX) */
	for (i = 0; i < num; i++) {
		const struct http_servlet_xmlrpc_method *const orig
		    = &methods[i];
		struct http_servlet_xmlrpc_method *const copy = &mcopy[i];

		/* Copy all except the malloc'd stuff */
		*copy = *orig;
		copy->name = NULL;
		copy->ptypes = NULL;

		/* Copy types array */
		if (orig->ptypes != NULL) {
			if ((copy->ptypes = MALLOC(MEM_TYPE, copy->max_params
			    * sizeof(*copy->ptypes))) == NULL) {
				http_servlet_xmlrpc_free_methods(mcopy);
				return (NULL);
			}
			memcpy((void *) copy->ptypes, orig->ptypes,
			       copy->max_params * sizeof(*copy->ptypes));
		}

		/* Copy name */
		if ((copy->name = STRDUP(MEM_TYPE, orig->name)) == NULL) {
			FREE(MEM_TYPE, (void *) copy->ptypes);
			http_servlet_xmlrpc_free_methods(mcopy);
			return (NULL);
		}
	}

	/* Done */
	return (mcopy);
}

static void
http_servlet_xmlrpc_free_methods(struct http_servlet_xmlrpc_method *  methods)
{
	const struct http_servlet_xmlrpc_method *method;
	int i;

	for (i = 0, method = methods; method->name != NULL; i++, method++) {
		FREE(MEM_TYPE, (char *) method->name);
		FREE(MEM_TYPE, (char *) method->ptypes);
	}
	FREE(MEM_TYPE, methods);
}

