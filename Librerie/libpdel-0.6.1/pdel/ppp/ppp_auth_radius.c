
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_msoft.h"

#include <openssl/md5.h>

#include <poll.h>
#include <radlib.h>
#include <radlib_vs.h>
#include "ppp/ppp_auth_radius.h"

/* Memory type */
#define RADIUS_MTYPE		"ppp_auth_radius_info"

#define HEXVAL(c)		(isdigit(c) ? (c) - '0' : tolower(c) - 'a' + 10)

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(log, sev, fmt , ## args)

/* Macros for filling in 'struct ppp_auth_radius_info' fields */
#define RADINFO_ALLOC_FIELD(rip, field)					\
	do {								\
		void *_mem;						\
									\
		if ((_mem = MALLOC(RADIUS_MTYPE,			\
		    sizeof(*rip->field))) == NULL) {			\
			LOG(LOG_ERR, "%s: %m", "malloc");		\
			goto fail_errno;				\
		}							\
		if (rip->field != NULL) {				\
			LOG(LOG_WARNING, "duplicate %s field returned"	\
			    " by RADIUS server", #field);		\
			FREE(RADIUS_MTYPE, rip->field);			\
		}							\
		rip->field = _mem;					\
	} while (0)

#define RADINFO_ALLOC_IP(rip, data, field)				\
	do {								\
		RADINFO_ALLOC_FIELD(rip, field);			\
		*rip->field = rad_cvt_addr(data);			\
	} while (0)

#define RADINFO_ALLOC_INT(rip, data, field)				\
	do {								\
		RADINFO_ALLOC_FIELD(rip, field);			\
		*rip->field = rad_cvt_int(data);			\
	} while (0)

#define RADINFO_ALLOC_STRING(rip, data, len, field)			\
	do {								\
		if (rip->field != NULL) {				\
			LOG(LOG_WARNING, "duplicate %s field returned"	\
			    " by RADIUS server", #field);		\
			FREE(NULL, rip->field);				\
		}							\
		if ((rip->field = rad_cvt_string(data, len)) == NULL) {	\
			errno = ENOMEM;					\
			LOG(LOG_ERR, "%s: %m", "rad_cvt_string");	\
			goto fail_errno;				\
		}							\
	} while (0)

/* Return values from the msoft decoding routines */
#define MSOFT_ERROR_SYSTEM	-1
#define MSOFT_ERROR_LIBRADIUS	-2
#define MSOFT_ERROR_VALUE	-3

/* Internal functions */
static int	ppp_auth_radius_wait(int fd, const struct timeval *tv,
			int cstate, struct ppp_log *log);
static int	ppp_auth_radius_vendor_msoft(struct rad_handle *rad,
			struct ppp_log *log, const struct ppp_auth_cred *cred,
			struct ppp_auth_resp *resp,
			struct ppp_auth_radius_info *rip, int attr,
			const void *data, size_t len);
static int	ppp_auth_radius_mppe_decode(struct rad_handle *rad,
			struct ppp_log *log, int salted,
			struct ppp_auth_resp *resp, const void **datap,
			size_t *len);

/*
 * Authenticate via RADIUS.
 */
int
ppp_auth_radius_check(struct rad_handle *rad, struct ppp_log *log,
	const struct ppp_auth_cred *cred, struct ppp_auth_resp *resp,
	struct ppp_auth_radius_info *rip)
{
	struct ppp_auth_radius_info ri;
	struct timeval tv;
	const void *data;
	int rtn = -1;
	int result;
	int cstate;
	size_t len;
	int attr;
	int fd;

	/* Simplify logic by assuming 'rip' is always valid */
	if (rip == NULL)
		rip = &ri;
	memset(rip, 0, sizeof(*rip));

	/* Initialize response conservatively */
	memset(resp, 0, sizeof(*resp));
	strlcpy(resp->errmsg, "Unknown error", sizeof(resp->errmsg));

	/* Avoid cancellation within libradius */
	if ((errno = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
	    &cstate)) != 0) {
		LOG(LOG_ERR, "%s: %m", "pthread_setcancelstate");
		goto fail_errno;
	}

	/* Create request */
	if (rad_create_request(rad, RAD_ACCESS_REQUEST) == -1)
		goto fail_radius;

	/* Add attributes */
	if (rad_put_int(rad, RAD_SERVICE_TYPE, RAD_FRAMED) == -1
	    || rad_put_int(rad, RAD_FRAMED_PROTOCOL, RAD_PPP) == -1)
		goto fail_radius;
	switch (cred->type) {
	case PPP_AUTH_PAP:
		if (rad_put_string(rad, RAD_USER_NAME, cred->u.pap.name) == -1)
			goto fail_radius;
		if (rad_put_string(rad, RAD_USER_PASSWORD,
		    cred->u.pap.password) == -1)
			goto fail_radius;
		break;
	case PPP_AUTH_CHAP_MSV1:
	case PPP_AUTH_CHAP_MSV2:
		if (rad_put_string(rad, RAD_USER_NAME, cred->u.chap.name) == -1)
			goto fail_radius;
		if (rad_put_vendor_attr(rad, RAD_VENDOR_MICROSOFT,
		    RAD_MICROSOFT_MS_CHAP_CHALLENGE, cred->u.chap.chal_data,
		    cred->u.chap.chal_len) == -1)
			goto fail_radius;
		break;
	case PPP_AUTH_CHAP_MD5:
		if (rad_put_string(rad, RAD_USER_NAME, cred->u.chap.name) == -1)
			goto fail_radius;
		if (rad_put_attr(rad, RAD_CHAP_CHALLENGE,
		    cred->u.chap.chal_data, cred->u.chap.chal_len) == -1)
			goto fail_radius;
		break;
	default:
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "unknown credential type %u", cred->type);
		goto done;
	}

	/* Add CHAP response attribute */
	switch (cred->type) {
	case PPP_AUTH_CHAP_MSV1:
	    {
		const struct ppp_auth_cred_chap_msv1 *c = &cred->u.chap.u.msv1;
		u_char ic[50];

		ic[0] = 0;			/* this field is not used */
		ic[1] = c->use_nt;
		memcpy(&ic[2], c->lm_hash, 24);
		memcpy(&ic[26], c->nt_hash, 24);
		if (rad_put_vendor_attr(rad, RAD_VENDOR_MICROSOFT,
		    RAD_MICROSOFT_MS_CHAP_RESPONSE, ic, sizeof(ic)) == -1)
			goto fail_radius;
		break;
	    }
	case PPP_AUTH_CHAP_MSV2:
	    {
		const struct ppp_auth_cred_chap_msv2 *c = &cred->u.chap.u.msv2;
		u_char ic[50];

		ic[0] = 0;			/* this field is not used */
		ic[1] = c->flags;
		memcpy(&ic[2], c->peer_chal, sizeof(c->peer_chal));
		memcpy(&ic[18], c->reserved, sizeof(c->reserved));
		memcpy(&ic[26], c->nt_response, sizeof(c->nt_response));
		if (rad_put_vendor_attr(rad, RAD_VENDOR_MICROSOFT,
		    RAD_MICROSOFT_MS_CHAP2_RESPONSE, ic, sizeof(ic)) == -1)
			goto fail_radius;
		break;
	    }
	case PPP_AUTH_CHAP_MD5:
	    {
		const struct ppp_auth_cred_chap_md5 *c = &cred->u.chap.u.md5;
		u_char ic[MD5_DIGEST_LENGTH + 1];

		ic[0] = c->id;
		memcpy(&ic[1], c->hash, sizeof(c->hash));
		if (rad_put_attr(rad, RAD_CHAP_PASSWORD, ic, sizeof(ic)) == -1)
			goto fail_radius;
		break;
	    }
	default:
		break;
	}

	/* Send request */
	result = rad_init_send_request(rad, &fd, &tv);
	while (1) {
		int selected;

		/* Check return value */
		switch (result) {
		case RAD_ACCESS_ACCEPT:
			break;
		case RAD_ACCESS_REJECT:
			strlcpy(resp->errmsg,
			    "Authorization failed", sizeof(resp->errmsg));
			goto done;
		case RAD_ACCESS_CHALLENGE:
			strlcpy(resp->errmsg,
			    "RADIUS server returned RAD_ACCESS_CHALLENGE",
			    sizeof(resp->errmsg));
			goto done;
		case 0:
			break;
		default:
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			   "unexpected libradius return value %d", result);
			goto done;
		case -1:
			goto fail_radius;
		}

		/* If we got our response, continue below */
		if (result > 0)
			break;

		/* Wait for reply or timeout */
		if ((selected = ppp_auth_radius_wait(fd,
		    &tv, cstate, log)) == -1)
			goto fail_errno;

		/* Check in with libradius */
		result = rad_continue_send_request(rad, selected, &fd, &tv);
	}

	/* Extract attributes */
	while ((attr = rad_get_attr(rad, &data, &len)) != 0) {
		switch (attr) {
		case RAD_FRAMED_IP_ADDRESS:
			RADINFO_ALLOC_IP(rip, data, ip);
			break;
		case RAD_FRAMED_IP_NETMASK:
			RADINFO_ALLOC_IP(rip, data, netmask);
			break;
		case RAD_FILTER_ID:
			RADINFO_ALLOC_STRING(rip, data, len, filter_id);
			break;
		case RAD_SESSION_TIMEOUT:
			RADINFO_ALLOC_INT(rip, data, session_timeout);
			break;
		case RAD_FRAMED_MTU:
			RADINFO_ALLOC_INT(rip, data, mtu);
			break;
		case RAD_FRAMED_ROUTING:
			RADINFO_ALLOC_INT(rip, data, routing);
			break;
		case RAD_FRAMED_COMPRESSION:
			RADINFO_ALLOC_INT(rip, data, vjc);
			break;
		case RAD_FRAMED_ROUTE:
		    {
			void *mem;
			int num;

			/* Count number of existing routes */
			for (num = 0;
			    rip->routes != NULL && rip->routes[num] != NULL;
			    num++);

			/* Extend the string array */
			if ((mem = REALLOC(RADIUS_MTYPE, rip->routes,
			    (num + 2) * sizeof(*rip->routes))) == NULL) {
				LOG(LOG_ERR, "%s: %m", "realloc");
				goto fail_errno;
			}
			rip->routes = mem;

			/* Add new route */
			if ((rip->routes[num] = rad_cvt_string(data,
			    len)) == NULL) {
				errno = ENOMEM;
				LOG(LOG_ERR, "%s: %m", "rad_cvt_string");
				goto fail_errno;
			}
			rip->routes[num + 1] = NULL;
			break;
		    }
		case RAD_REPLY_MESSAGE:
			RADINFO_ALLOC_STRING(rip, data, len, reply_message);
			break;
		case RAD_VENDOR_SPECIFIC:
		    {
			u_int32_t vendor;

			attr = rad_get_vendor_attr(&vendor, &data, &len);
			switch (vendor) {
			case RAD_VENDOR_MICROSOFT:
				switch (ppp_auth_radius_vendor_msoft(rad,
				    log, cred, resp, rip, attr, data, len)) {
				case 0:
					break;
				default:
				case MSOFT_ERROR_SYSTEM:
					goto fail_errno;
				case MSOFT_ERROR_LIBRADIUS:
					goto fail_radius;
				case MSOFT_ERROR_VALUE:
					goto fail_value;
				}
				break;
			default:
				LOG(LOG_DEBUG, "unknown %s attribute"
				    " #%u:#%u returned by server",
				    "vendor", vendor, attr);
				break;
			}
			break;
		    }
		default:
			LOG(LOG_DEBUG, "unsupported %s attribute"
			    " #%u returned by server", "RADIUS", attr);
			break;
		case -1:
			goto fail_radius;
		}
	}

	/* Authorization successful */
	if (result == RAD_ACCESS_ACCEPT)
		rtn = 0;
	goto done;

fail_radius:
	/* Fail because of an error from libradius */
	strlcpy(resp->errmsg, rad_strerror(rad), sizeof(resp->errmsg));

fail_value:
	/* Fail because of a bogus RADIUS value */
	LOG(LOG_ERR, "RADIUS error: %s", resp->errmsg);
	goto done;

fail_errno:
	/* Fail because of some system error */
	strlcpy(resp->errmsg, strerror(errno), sizeof(resp->errmsg));

done:
	/* Restore cancel state and return */
	(void)pthread_setcancelstate(cstate, &cstate);
	if (rip == &ri)
		ppp_auth_radius_info_reset(rip);
	return (rtn);
}

/*
 * Wait for readability on the file descriptor or timeout.
 * Restore cancellability state of the thread during.
 */
static int
ppp_auth_radius_wait(int fd, const struct timeval *tv,
	int cstate, struct ppp_log *log)
{
	struct pollfd pfd;
	int rtn;

	/* Allow cancellation while in poll() */
	if ((errno = pthread_setcancelstate(cstate, &cstate)) != 0) {
		LOG(LOG_ERR, "pthread_setcancelstate: %m");
		return (-1);
	}

	/* Poll for data or timeout */
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
	switch (poll(&pfd, 1, tv->tv_sec * 1000 + tv->tv_usec / 1000)) {
	case 0:
		rtn = 0;
		break;
	case 1:
		rtn = 1;
		break;
	case -1:
	default:
		LOG(LOG_ERR, "poll: %m");
		rtn = -1;
	}

	/* Block cancellation again */
	if ((errno = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
	    &cstate)) != 0) {
		LOG(LOG_ERR, "%s: %m", "pthread_setcancelstate");
		return (-1);
	}

	/* Done */
	return (rtn);
}

/*
 * Decode a Microsoft vendor attribute.
 */
int
ppp_auth_radius_vendor_msoft(struct rad_handle *rad, struct ppp_log *log,
	const struct ppp_auth_cred *cred, struct ppp_auth_resp *resp,
	struct ppp_auth_radius_info *rip, int attr, const void *data,
	size_t len)
{
	int i;

	switch (attr) {
	case RAD_MICROSOFT_MS_CHAP_ERROR:
	case RAD_MICROSOFT_MS_CHAP2_SUCCESS:
	    {
		const char *s;

		/* Compensate for broken servers that leave out the ID byte */
		if (len > 0 && (len < 3 || ((const char *)data)[1] != '=')) {
			data = (const char *)data + 1;
			len--;
		}

		/* Copy string as-is; if error string, we're done */
		if (attr == RAD_MICROSOFT_MS_CHAP_ERROR) {
			RADINFO_ALLOC_STRING(rip, data, len, mschap_error);
			break;
		}
		RADINFO_ALLOC_STRING(rip, data, len, mschap2_success);

		/* Ignore authresp unless MS-CHAPv2 */
		if (cred->type != PPP_AUTH_CHAP_MSV2)
			break;

		/* Parse out server response */
		if ((s = strstr(rip->mschap2_success, "S=")) == NULL) {
bogus_authresp:		snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "invalid MS-CHAPv2 response string \"%s\" returned"
			    " from server", rip->mschap2_success);
			goto fail_value;
		}
		s += 2;
		for (i = 0; i < sizeof(resp->authresp); i++) {
			if (!isxdigit(s[i * 2]) || !isxdigit(s[i * 2 + 1]))
				goto bogus_authresp;
			resp->authresp[i] = (HEXVAL(s[i * 2]) << 4)
			    | HEXVAL(s[i * 2 + 1]);
		}
		break;
	    }
	case RAD_MICROSOFT_MS_MPPE_ENCRYPTION_POLICY:
		RADINFO_ALLOC_INT(rip, data, mppe_policy);
		switch (*rip->mppe_policy) {
		case 1:			/* encryption allowed */
		case 2:			/* encryption required */
			break;
		default:
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "invalid MS-CHAPv2 encryption policy %d returned"
			    " from server", *rip->mppe_policy);
			goto fail_value;
		}
		break;
	case RAD_MICROSOFT_MS_MPPE_ENCRYPTION_TYPES:
		RADINFO_ALLOC_INT(rip, data, mppe_types);
		break;
	case RAD_MICROSOFT_MS_MPPE_RECV_KEY:
	case RAD_MICROSOFT_MS_MPPE_SEND_KEY:
	    {
		u_char *key;
		int wlen;

		/* Ignore unless we did MS-CHAP */
		if (cred->type != PPP_AUTH_CHAP_MSV1
		    && cred->type != PPP_AUTH_CHAP_MSV2)
			break;

		/* Decode key */
		if ((i = ppp_auth_radius_mppe_decode(rad,
		    log, 1, resp, &data, &len)) != 0)
			return (i);
		key = (u_char *)data;

		/* Sanity check key length */
		wlen = (cred->type == PPP_AUTH_CHAP_MSV1
		    && attr == RAD_MICROSOFT_MS_MPPE_SEND_KEY) ? 8 : 16;
		if (len != wlen) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "invalid length %d != %d MPPE %s key returned"
			    " from server", (int)len, wlen,
			    attr == RAD_MICROSOFT_MS_MPPE_SEND_KEY ?
			    "send" : "recv");
			FREE(RADIUS_MTYPE, key);
			goto fail_value;
		}

		/* Copy key into response structure */
		switch (cred->type) {
		case PPP_AUTH_CHAP_MSV1:
			if (attr == RAD_MICROSOFT_MS_MPPE_SEND_KEY)
				memcpy(resp->mppe.msv1.key_64, data, len);
			else
				memcpy(resp->mppe.msv1.key_128, data, len);
			break;
		case PPP_AUTH_CHAP_MSV2:
			memcpy(resp->mppe.msv2.keys[attr
			    == RAD_MICROSOFT_MS_MPPE_RECV_KEY], data, len);
			break;
		default:
			break;
		}
		FREE(RADIUS_MTYPE, key);
		break;
	    }
	case RAD_MICROSOFT_MS_CHAP_MPPE_KEYS:
	    {
		u_char *keys;

		/* Ignore unless we did MS-CHAPv1 */
		if (cred->type != PPP_AUTH_CHAP_MSV1)
			break;

		/* Decode key */
		if ((i = ppp_auth_radius_mppe_decode(rad,
		    log, 0, resp, &data, &len)) != 0)
			return (i);
		keys = (u_char *)data;

		/* Sanity check key length */
		if (len != 32) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "invalid length %d != %d MPPE %s key returned"
			    " from server", (int)len, 32, "MS-CHAPv1");
			FREE(RADIUS_MTYPE, keys);
			goto fail_value;
		}

		/* Copy keys into response structure */
		memcpy(resp->mppe.msv1.key_64, keys, 8);
		memcpy(resp->mppe.msv1.key_128, keys + 8, 16);
		ppp_msoft_get_start_key(cred->u.chap.chal_data,
		    resp->mppe.msv1.key_128);
		FREE(RADIUS_MTYPE, keys);
		break;
	    }

	case -1:
		goto fail_radius;

	default:
		LOG(LOG_DEBUG, "unsupported %s attribute"
		    " #%u returned by server", "Microsoft", attr);
		break;
	}

	/* Done */
	return (0);

fail_errno:
	return (MSOFT_ERROR_SYSTEM);

fail_radius:
	return (MSOFT_ERROR_LIBRADIUS);

fail_value:
	return (MSOFT_ERROR_VALUE);
}

void
ppp_auth_radius_info_reset(struct ppp_auth_radius_info *rip)
{
	FREE(RADIUS_MTYPE, rip->ip);
	FREE(RADIUS_MTYPE, rip->netmask);
	FREE(NULL, rip->filter_id);
	FREE(RADIUS_MTYPE, rip->session_timeout);
	FREE(RADIUS_MTYPE, rip->mtu);
	FREE(RADIUS_MTYPE, rip->vjc);
	FREE(RADIUS_MTYPE, rip->routing);
	FREE(NULL, rip->reply_message);
	FREE(NULL, rip->mschap_error);
	FREE(NULL, rip->mschap2_success);
	FREE(RADIUS_MTYPE, rip->mppe_policy);
	FREE(RADIUS_MTYPE, rip->mppe_types);
	if (rip->routes != NULL) {
		char **route;

		for (route = rip->routes; *route != NULL; route++)
			FREE(NULL, *route);
		FREE(RADIUS_MTYPE, rip->routes);
	}
	memset(rip, 0, sizeof(*rip));
}

#define MPPE_ENCODE_SALT_LEN	2
#define MPPE_ENCODE_AUTH_LEN	16
#define MPPE_ENCODE_CHUNK_LEN	16

/*
 * Decode an MPPE keys attribute
 */
static int
ppp_auth_radius_mppe_decode(struct rad_handle *rad, struct ppp_log *log,
	int salted, struct ppp_auth_resp *resp, const void **datap, size_t *len)
{
	u_char key[MPPE_ENCODE_CHUNK_LEN];
	char reqauth[MPPE_ENCODE_AUTH_LEN];
	const u_char *edata = *datap;
	size_t elen = *len;
	const char *secret;
	u_char *data;
	MD5_CTX ctx;
	int pos;

	/* Sanity check encoded length */
	if (elen % MPPE_ENCODE_CHUNK_LEN
	    != (salted ? MPPE_ENCODE_SALT_LEN : 0)) {
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "bogus MPPE key %s length %u from server",
		    "encrypted", (int)elen);
		return (MSOFT_ERROR_VALUE);
	}

	/* Get the Request-Authenticator and server secret */
	if (rad_request_authenticator(rad, reqauth, sizeof(reqauth)) == -1) {
		errno = EINVAL;
		LOG(LOG_ERR, "%s: %m", "rad_request_authenticator");
		return (MSOFT_ERROR_SYSTEM);
	}
	secret = rad_server_secret(rad);

	/* Initialize decryption key */
	MD5_Init(&ctx);
	MD5_Update(&ctx, secret, strlen(secret));
	MD5_Update(&ctx, reqauth, sizeof(reqauth));
	if (salted && edata[0] != 0)
		MD5_Update(&ctx, edata, MPPE_ENCODE_SALT_LEN);
	MD5_Final(key, &ctx);

	/* Advance past initial salt */
	if (salted) {
		edata += MPPE_ENCODE_SALT_LEN;
		elen -= MPPE_ENCODE_SALT_LEN;
	}

	/* Allocate output buffer */
	if ((data = MALLOC(RADIUS_MTYPE, elen)) == NULL) {
		LOG(LOG_ERR, "%s: %m", "malloc");
		return (MSOFT_ERROR_SYSTEM);
	}

	/* Decrypt in blocks of MPPE_ENCODE_CHUNK_LEN */
	pos = 0;
	while (1) {
		int j;

		/* Decrypt the next block */
		for (j = 0; j < MPPE_ENCODE_CHUNK_LEN; j++)
			data[pos++] = edata[j] ^ key[j];

		/* Advance */
		edata += MPPE_ENCODE_CHUNK_LEN;
		elen -= MPPE_ENCODE_CHUNK_LEN;
		if (elen == 0)
			break;

		/* Update key */
		MD5_Init(&ctx);
		MD5_Update(&ctx, secret, strlen(secret));
		MD5_Update(&ctx, edata, MPPE_ENCODE_CHUNK_LEN);
		MD5_Final(key, &ctx);
	}

	/* If not salted, assume no length byte either */
	if (!salted) {
		*len = elen;
		*datap = data;
		return (0);
	}

	/* Extract actual length (first byte) and sanity check it */
	if (data[0] > elen - 1) {
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "bogus MPPE key %s length %u > %u from server",
		    "decrypted", data[0], (int)elen - 1);
		FREE(RADIUS_MTYPE, data);
		return (MSOFT_ERROR_VALUE);
	}
	*len = data[0];
	*datap = data;
	memmove(data, data + 1, data[0]);
	return (0);
}

