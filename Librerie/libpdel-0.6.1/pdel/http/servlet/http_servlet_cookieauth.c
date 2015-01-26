
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>

#include <openssl/ssl.h>
#include <openssl/md5.h>

#include "pdel/pd_port.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/boolean.h"
#include "structs/type/data.h"
#include "structs/type/int.h"
#include "structs/type/null.h"
#include "structs/type/string.h"
#include "structs/type/struct.h"
#include "structs/type/time.h"
#include "structs/xml.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/redirect.h"
#include "http/servlet/cookieauth.h"

#include "sys/alog.h"
#include "io/string_fp.h"
#include "util/rsa_util.h"
#include "util/typed_mem.h"
#include "private/debug.h"


/*
 * Ref: http://home.netscape.com/newsref/std/cookie_spec.html
 */

#define MEM_TYPE		"http_servlet_cookieauth"
#define DATA_MEM_TYPE		"http_servlet_cookieauth.data"
#define COOKIE_TIME_FMT		"%a, %d-%b-%Y %T GMT"
#define COOKIE_LINGER_TIME	(30 * 60)		/* 30 minutes */

/* Per-servlet private info */
struct cookieauth_private {
	struct http_servlet	*redirect;	/* private redirect servlet */
	http_servlet_cookieauth_reqd_t
				*authreqd;	/* checks if auth required */
	void			*arg;		/* argument for authreqd() */
	void			(*destroy)(void *);	/* destructor for arg */
	char			*privkey;	/* rsa private key file */
	char			*cookiename;	/* name of cookie */
	struct structs_data	id;		/* unique system id */
};

/* Structure of the cookie data */
struct cookieauth {
	char			*username;	/* login username */
	char			*path;		/* cookie path */
	char			*domain;	/* cookie domain */
	u_char			secure;		/* cookie 'secure' bit */
	u_char			session_only;	/* this browser session only */
	time_t			timestamp;	/* time cookie was set */
	time_t			expire;		/* expiration time, or zero */
	u_int32_t		linger;		/* max linger time, or zero */
	struct structs_data	id;		/* unique system id */
	struct structs_data	sig;		/* rsa signature */
};

/* Internal functions */
static http_servlet_run_t	http_servlet_cookieauth_run;
static http_servlet_destroy_t	http_servlet_cookieauth_destroy;

static int	http_servlet_cookieauth_get(const char *privkey,
			const struct structs_data *id, const char *cookiename,
			struct http_request *req, struct cookieauth *auth);
static int	http_servlet_cookieauth_md5(const struct cookieauth *auth,
			u_char *md5);

#if PDEL_DEBUG
static void	dump_data(const void *data, u_int plen, const char *fmt, ...)
			__printflike(3, 4);
#endif

/* Internal variables */
static const struct structs_type authcookie_data_type
	= STRUCTS_DATA_TYPE(NULL, DATA_MEM_TYPE);

static const struct structs_field cookieauth_fields[] = {
	STRUCTS_STRUCT_FIELD(cookieauth, username, &structs_type_string),
	STRUCTS_STRUCT_FIELD(cookieauth, path, &structs_type_string_null),
	STRUCTS_STRUCT_FIELD(cookieauth, domain, &structs_type_string_null),
	STRUCTS_STRUCT_FIELD(cookieauth, secure, &structs_type_boolean_char),
	STRUCTS_STRUCT_FIELD(cookieauth,
	    session_only, &structs_type_boolean_char),
	STRUCTS_STRUCT_FIELD(cookieauth, timestamp, &structs_type_time_gmt),
	STRUCTS_STRUCT_FIELD(cookieauth, expire, &structs_type_time_gmt),
	STRUCTS_STRUCT_FIELD(cookieauth, linger, &structs_type_uint32),
	STRUCTS_STRUCT_FIELD(cookieauth, id, &authcookie_data_type),
	STRUCTS_STRUCT_FIELD(cookieauth, sig, &authcookie_data_type),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type cookieauth_type
	= STRUCTS_STRUCT_TYPE(cookieauth, &cookieauth_fields);

/*
 * Create a new cookieauth servlet.
 */
struct http_servlet *
http_servlet_cookieauth_create(const char *redirect, int append,
	http_servlet_cookieauth_reqd_t *authreqd, void *arg,
	void (*destroy)(void *), const char *privkey,
	const void *id, size_t idlen, const char *cookiename)
{
	struct http_servlet *servlet;
	struct cookieauth_private *priv;
	struct structs_data id_data;
	const char *s;

	/* Validate cookiename */
	for (s = cookiename; *s != '\0'; s++) {
		if (!isgraph((u_char)*s) || strchr(",;=", *s) != NULL)
			break;
	}
	if (s == cookiename || *s != '\0') {
		errno = EINVAL;
		return (NULL);
	}

	/* Create new servlet */
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_cookieauth_run;
	servlet->destroy = http_servlet_cookieauth_destroy;

	/* Set up private info */
	if ((priv = MALLOC(MEM_TYPE, sizeof(*priv))) == NULL)
		goto fail;
	memset(priv, 0, sizeof(*priv));
	if ((priv->redirect
	    = http_servlet_redirect_create(redirect, append)) == NULL)
		goto fail;
	priv->authreqd = authreqd;
	priv->arg = arg;
	priv->destroy = destroy;
	if ((priv->privkey = STRDUP(MEM_TYPE, privkey)) == NULL)
		goto fail;
	if ((priv->cookiename = STRDUP(MEM_TYPE, cookiename)) == NULL)
		goto fail;
	id_data.data = (u_char *)id;
	id_data.length = idlen;
	if (structs_get(&authcookie_data_type, NULL, &id_data, &priv->id) == -1)
		goto fail;
	servlet->arg = priv;

	/* Done */
	return (servlet);

fail:
	/* Clean up after failure */
	if (priv != NULL) {
		structs_free(&authcookie_data_type, NULL, &priv->id);
		FREE(MEM_TYPE, priv->cookiename);
		FREE(MEM_TYPE, priv->privkey);
		http_server_destroy_servlet(&priv->redirect);
		FREE(MEM_TYPE, priv);
	}
	FREE(MEM_TYPE, servlet);
	return (NULL);
}

/*
 * Execute cookie authorization servlet.
 */
static int
http_servlet_cookieauth_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct cookieauth_private *const priv = servlet->arg;
	struct cookieauth auth;

	/* Always allow access to the logon page */
	if (priv->authreqd != NULL && !(*priv->authreqd)(priv->arg, req))
		goto allow;

	/* Get valid authorization structure, if there is one */
	if (http_servlet_cookieauth_get(priv->privkey,
	    &priv->id, priv->cookiename, req, &auth) == -1) {

		/* Invalid authorization -> redirect to logon page */
		if (errno == EACCES) {
			return ((*priv->redirect->run)(priv->redirect,
			    req, resp));
		}

		/* Other errors -> generate server error */
		http_response_send_errno_error(resp);
		return (1);
	}

	/* Update cookie for linger timer */
	if (auth.linger != 0) {
		(void)http_servlet_cookieauth_login(resp, priv->privkey,
		    auth.username, auth.linger, auth.expire, auth.session_only,
		    priv->id.data, priv->id.length, priv->cookiename, auth.path,
		    auth.domain, auth.secure);
	}

	/* Free authorization info */
	structs_free(&cookieauth_type, NULL, &auth);

allow:
	/* Allow request to continue */
	return (0);
}

/*
 * Destroy an auth servlet.
 */
static void
http_servlet_cookieauth_destroy(struct http_servlet *servlet)
{
	struct cookieauth_private *const priv = servlet->arg;

	if (priv->destroy != NULL)
		(*priv->destroy)(priv->arg);
	structs_free(&authcookie_data_type, NULL, &priv->id);
	FREE(MEM_TYPE, priv->privkey);
	FREE(MEM_TYPE, priv->cookiename);
	http_server_destroy_servlet(&priv->redirect);
	FREE(MEM_TYPE, priv);
	FREE(MEM_TYPE, servlet);
}

/*
 * Add a cookie that will cause the servlet to not redirect.
 */
int
http_servlet_cookieauth_login(struct http_response *resp,
	const char *privkey, const char *username, u_int max_linger,
	time_t expire, int session_only, const u_char *id, size_t idlen,
	const char *cookiename, const char *path, const char *domain,
	int secure)
{
	struct structs_data data;	/* binary encoding of "auth" */
	struct cookieauth auth;		/* authorization info struct */
	u_char md5[MD5_DIGEST_LENGTH];
	u_char sigbuf[1024];
	char ebuf[128];
	int siglen;
	char *hval;
	FILE *sb;

	/* Build auth structure */
	if (structs_init(&cookieauth_type, NULL, &auth) == -1)
		return (-1);
	if (structs_set_string(&cookieauth_type,
	    "username", username, &auth, ebuf, sizeof(ebuf)) == -1) {
		alogf(LOG_ERR, "%s: %s", "structs_set_string" _ ebuf);
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}
	if (path != NULL
	    && structs_set_string(&cookieauth_type,
	      "path", path, &auth, ebuf, sizeof(ebuf)) == -1) {
		alogf(LOG_ERR, "%s: %s", "structs_set_string" _ ebuf);
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}
	if (domain != NULL
	    && structs_set_string(&cookieauth_type,
	      "domain", domain, &auth, ebuf, sizeof(ebuf)) == -1) {
		alogf(LOG_ERR, "%s: %s", "structs_set_string" _ ebuf);
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}
	auth.secure = !!secure;
	auth.session_only = !!session_only;
	auth.timestamp = time(NULL);
	auth.linger = max_linger;
	auth.expire = expire;
	data.data = (u_char *)id;
	data.length = idlen;
	if (structs_get(&authcookie_data_type, NULL, &data, &auth.id) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_get");
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}

	/* Add RSA signature */
	if (http_servlet_cookieauth_md5(&auth, md5) == -1) {
		alogf(LOG_ERR, "%s: %m", "http_servlet_cookieauth_md5");
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}
	if ((siglen = rsa_util_sign(privkey,
	    md5, sigbuf, sizeof(sigbuf))) == -1) {
		alogf(LOG_ERR, "%s: %m", "rsa_util_sign");
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}
	data.data = sigbuf;
	data.length = siglen;
	if (structs_get(&authcookie_data_type, NULL, &data, &auth.sig) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_get");
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}

	/* Encode auth structure into binary */
	if (structs_get_binary(&cookieauth_type,
	    NULL, &auth, DATA_MEM_TYPE, &data) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_get_binary");
		structs_free(&cookieauth_type, NULL, &auth);
		return (-1);
	}

#if PDEL_DEBUG
	if (PDEL_DEBUG_ENABLED(HTTP_SERVLET_COOKIEAUTH)) {
		printf("COOKIE AUTH STRUCTURE\n");
		structs_xml_output(&cookieauth_type,
		    "auth", NULL, &auth, stdout, NULL, STRUCTS_XML_FULL);
		dump_data(data.data, data.length, "COOKIE DATA");
	}
#endif

	structs_free(&cookieauth_type, NULL, &auth);

	/* Base64 encode it */
	if ((hval = structs_get_string(&authcookie_data_type,
	    NULL, &data, TYPED_MEM_TEMP)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_get_string");
		structs_free(&authcookie_data_type, NULL, &data);
		return (-1);
	}
	structs_free(&authcookie_data_type, NULL, &data);

	/* Create string output buffer */
	if ((sb = string_buf_output(TYPED_MEM_TEMP)) == NULL) {
		FREE(TYPED_MEM_TEMP, hval);
		return (-1);
	}

	/* Construct cookie header value */
	fprintf(sb, "%s=%s", cookiename, hval);
	FREE(TYPED_MEM_TEMP, hval);
	if (!session_only) {
		char tbuf[64];
		struct tm tm;

		strftime(tbuf, sizeof(tbuf),
		    COOKIE_TIME_FMT, gmtime_r(&expire, &tm));
		fprintf(sb, "; expires=%s", tbuf);
	}
	if (domain != NULL)
		fprintf(sb, "; domain=%s", domain);
	if (path != NULL)
		fprintf(sb, "; path=%s", path);
	if (secure)
		fprintf(sb, "; secure");
	hval = string_buf_content(sb, 1);
	fclose(sb);
	if (hval == NULL)
		return (-1);

	/* Set cookie header value */
	if (http_response_set_header(resp,
	    0, HTTP_HEADER_SET_COOKIE, "%s", hval) == -1) {
		FREE(TYPED_MEM_TEMP, hval);
		return (-1);
	}
	FREE(TYPED_MEM_TEMP, hval);

	/* Done */
	return (0);
}

/*
 * Remove authorization cookie.
 */
int
http_servlet_cookieauth_logout(const char *cookiename, const char *path,
	const char *domain, struct http_response *resp)
{
	static const time_t past = 0;
	char tbuf[64];
	struct tm tm;
	char *hval;
	FILE *sb;

	/* Create string output buffer */
	if ((sb = string_buf_output(TYPED_MEM_TEMP)) == NULL)
		return (-1);

	/* Construct cookie header value */
	strftime(tbuf, sizeof(tbuf), COOKIE_TIME_FMT, gmtime_r(&past, &tm));
	fprintf(sb, "%s=x; expires=%s", cookiename, tbuf);
	if (domain != NULL)
		fprintf(sb, "; domain=%s", domain);
	if (path != NULL)
		fprintf(sb, "; path=%s", path);
	hval = string_buf_content(sb, 1);
	fclose(sb);
	if (hval == NULL)
		return (-1);

	/* Set cookie header value */
	if (http_response_set_header(resp, 0,
	    HTTP_HEADER_SET_COOKIE, "%s", hval) == -1) {
		FREE(TYPED_MEM_TEMP, hval);
		return (-1);
	}
	FREE(TYPED_MEM_TEMP, hval);

	/* Done */
	return (0);
}

/*
 * Get username.
 */
char *
http_servlet_cookieauth_user(const char *privkey, const void *id, size_t idlen,
	const char *cookiename, struct http_request *req, const char *mtype)
{
	struct cookieauth auth;
	struct structs_data idd;
	char *username;

	/* Get valid authorization structure, if there is one */
	idd.data = (u_char *)id;
	idd.length = idlen;
	if (http_servlet_cookieauth_get(privkey,
	    &idd, cookiename, req, &auth) == -1)
		return (NULL);

	/* Get copy of username */
	if ((username = structs_get_string(&cookieauth_type,
	    "username", &auth, mtype)) == NULL)
		alogf(LOG_ERR, "%s: %m", "structs_get_string");

	/* Free auth structure */
	structs_free(&cookieauth_type, NULL, &auth);

	/* Return username */
	return (username);
}

/*
 * Get valid authorization structure if there is one.
 */
static int
http_servlet_cookieauth_get(const char *privkey, const struct structs_data *id,
	const char *cookiename, struct http_request *req,
	struct cookieauth *auth)
{
	const int namelen = strlen(cookiename);
	const time_t now = time(NULL);
	const char *hval;
	const char *next;

	/* Get cookie header */
	if ((hval = http_request_get_header(req, HTTP_HEADER_COOKIE)) == NULL)
		goto invalid;

	/* Find our cookie */
	for ( ; *hval != '\0'; hval = next) {
		u_char md5[MD5_DIGEST_LENGTH];
		struct structs_data data;
		char valbuf[512];
		const char *eq;
		char ebuf[128];
		int vallen;

		/* Get next cookie and compare name */
		while (isspace((u_char)*hval))
			hval++;
		if ((eq = strchr(hval, '=')) == NULL)
			break;
		if ((next = strchr(eq + 1, ';')) == NULL) {
			next = eq + strlen(eq);
			vallen = strlen(eq + 1);
		} else {
			vallen = next - (eq + 1);
			next++;
		}
		if (strncmp(hval, cookiename, namelen) != 0
		    || hval + namelen != eq)
			continue;

		/* Isolate cookie value */
		if (vallen > sizeof(valbuf) - 1) {
			DBG(HTTP_SERVLET_COOKIEAUTH, "cookie too long", "");
			continue;
		}
		memcpy(valbuf, hval + namelen + 1, vallen);
		valbuf[vallen] = '\0';

		/* Decode base64 data into binary data */
		if (structs_init(&authcookie_data_type, NULL, &data) == -1) {
			alogf(LOG_ERR, "%s: %m", "structs_init");
			continue;
		}
		if (structs_set_string(&authcookie_data_type,
		    NULL, valbuf, &data, NULL, 0) == -1) {
			DBG(HTTP_SERVLET_COOKIEAUTH,
			    "error decoding base64: %s", ebuf);
			structs_free(&authcookie_data_type, NULL, &data);
			continue;
		}

#if PDEL_DEBUG
		if (PDEL_DEBUG_ENABLED(HTTP_SERVLET_COOKIEAUTH))
			dump_data(data.data, data.length, "COOKIE DATA");
#endif

		/* Initialize the struct cookieauth */
		if (structs_init(&cookieauth_type, NULL, auth) == -1) {
			alogf(LOG_ERR, "%s: %m", "structs_init");
			continue;
		}

		/* Decode binary data into the struct cookieauth */
		if (structs_set_binary(&cookieauth_type, NULL, &data,
		    auth, ebuf, sizeof(ebuf)) == -1) {
			DBG(HTTP_SERVLET_COOKIEAUTH,
			    "error decoding auth data: %s", ebuf);
			structs_free(&cookieauth_type, NULL, auth);
			structs_free(&authcookie_data_type, NULL, &data);
			continue;
		}
		structs_free(&authcookie_data_type, NULL, &data);

#if PDEL_DEBUG
		if (PDEL_DEBUG_ENABLED(HTTP_SERVLET_COOKIEAUTH)) {
			printf("COOKIE AUTH STRUCTURE\n");
			structs_xml_output(&cookieauth_type,
			    "auth", NULL, auth, stdout, NULL, STRUCTS_XML_FULL);
		}
#endif

		/* Validate auth cookie timestamp and expiration */
		if (auth->timestamp > now
		    || (auth->expire != 0 && now >= auth->expire)
		    || (auth->linger != 0
		      && now >= auth->timestamp + auth->linger)) {
			DBG(HTTP_SERVLET_COOKIEAUTH, "expired cookie", "");
			structs_free(&cookieauth_type, NULL, auth);
			continue;
		}

		/* Validate auth cookie identifier */
		if (structs_equal(&authcookie_data_type,
		    NULL, &auth->id, id) != 1) {
			DBG(HTTP_SERVLET_COOKIEAUTH, "wrong system id", "");
			structs_free(&cookieauth_type, NULL, auth);
			continue;
		}

		/* Validate auth cookie RSA signature */
		if (http_servlet_cookieauth_md5(auth, md5) == -1) {
			alogf(LOG_ERR, "%s: %m", "http_servlet_cookieauth_md5");
			structs_free(&cookieauth_type, NULL, auth);
			return (-1);
		}
		if (!rsa_util_verify_priv(privkey, md5,
		    auth->sig.data, auth->sig.length)) {
			DBG(HTTP_SERVLET_COOKIEAUTH, "invalid RSA signature",
			    "");
			structs_free(&cookieauth_type, NULL, auth);
			continue;
		}

		/* OK */
		return (0);
	}

invalid:
	/* No valid cookie found */
	errno = EACCES;
	return (-1);
}

/*
 * Compute MD5 for RSA signature.
 */
static int
http_servlet_cookieauth_md5(const struct cookieauth *auth, u_char *md5)
{
	struct structs_data data;
	struct cookieauth copy;
	MD5_CTX ctx;

	/* Copy supplied auth structure */
	if (structs_get(&cookieauth_type, NULL, auth, &copy) == -1)
		return (-1);

	/* Zero out the 'sig' field */
	FREE(DATA_MEM_TYPE, copy.sig.data);
	memset(&copy.sig, 0, sizeof(copy.sig));

	/* Create binary encoding of 'copy' */
	if (structs_get_binary(&cookieauth_type,
	    NULL, &copy, DATA_MEM_TYPE, &data) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_get_binary");
		structs_free(&cookieauth_type, NULL, &copy);
		return (-1);
	}
	structs_free(&cookieauth_type, NULL, &copy);

	/* Compute MD5 of that */
	MD5_Init(&ctx);
	MD5_Update(&ctx, data.data, data.length);
	MD5_Final(md5, &ctx);

	/* Done */
	structs_free(&authcookie_data_type, NULL, &data);
	return (0);
}

#if PDEL_DEBUG
/*
 * Dump some data.
 */
static void
dump_data(const void *data, u_int plen, const char *fmt, ...)
{
	const u_char *pkt = data;
	const int num = 16;
	va_list args;
	int i, j;

	va_start(args, fmt);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	for (i = 0; i < ((plen + num - 1) / num) * num; i += num) {
		printf("0x%04x  ", i);
		for (j = i; j < i + num; j++) {
			if (j < plen)
				printf("%02x", pkt[j]);
			else
				printf("  ");
			if ((j % 2) == 1)
				printf(" ");
		}
		printf("       ");
		for (j = i; j < i + num; j++) {
			if (j < plen) {
				printf("%c", isprint((u_char)pkt[j]) ?
				    pkt[j] : '.');
			}
		}
		printf("\n");
	}
}
#endif

