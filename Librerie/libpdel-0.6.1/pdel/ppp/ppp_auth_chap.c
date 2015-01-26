
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_fsm.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_link.h"
#include "ppp/ppp_util.h"
#include "ppp/ppp_auth_chap.h"

#define CHAP_MTYPE		"ppp_authtype.chap"

#define CHAP_RETRY		3
#define CHAP_MAXTRY		5

#define CHAP_CHALLENGE		1
#define CHAP_RESPONSE		2
#define CHAP_ACK		3
#define CHAP_NAK		4

#define CHAP_MSG_ACK		"Authorization successful"
#define CHAP_MSG_NAK		"Authorization failed"
#define CHAP_MSG_BUFSIZE	256

#define MSCHAPV1_MSG_ACK	CHAP_MSG_ACK
#define MSCHAPV1_MSG_NAK	"E=691 R=0"

#define MSCHAPV2_MSG_ACK	CHAP_MSG_ACK
#define MSCHAPV2_MSG_NAK	CHAP_MSG_NAK

/* CHAP info structure */
struct ppp_auth_chap {
	struct ppp_link			*link;
	struct ppp_log			*log;
	struct ppp_auth_config		aconf;
	const struct ppp_auth_type	*auth;
	struct ppp_auth_cred		cred;
	struct ppp_auth_resp		resp;
	const struct ppp_auth_chap_type	*type;
	struct pevent_ctx		*ev_ctx;
	struct pevent			*timer;
	pthread_mutex_t			*mutex;
	int				dir;
	int				retry;
	u_char				id;
};

/* Internal functions */
static void	ppp_auth_chap_send_challenge(struct ppp_auth_chap *chap);
static void	ppp_auth_chap_send_response(struct ppp_auth_chap *chap);
static void	ppp_auth_chap_send_result(struct ppp_auth_chap *chap,
			u_char id, int ack);
static int	ppp_chap_unpack(const u_char *data, size_t len,
			char *name, u_char *value, int *vlenp);
static void	ppp_chap_send_value(struct ppp_auth_chap *chap, u_char code,
			const u_char *value, size_t vlen, const char *name);

static ppp_link_auth_finish_t	ppp_auth_chap_acquire_finish;
static ppp_link_auth_finish_t	ppp_auth_chap_check_finish;

static pevent_handler_t		ppp_auth_chap_timeout;

/* Internal variables */
static const	char *chap_codes[] = {
	"zero",
	"challenge",
	"response",
	"ack",
	"nak"
};

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(chap->log, sev, fmt , ## args)

/*
 * Start CHAP
 */
void *
ppp_auth_chap_start(struct pevent_ctx *ev_ctx, struct ppp_link *link,
	pthread_mutex_t *mutex, int dir, u_int16_t *protop, struct ppp_log *log)
{
	struct ppp_auth_chap *chap;

	/* Create info structure */
	if ((chap = MALLOC(CHAP_MTYPE, sizeof(*chap))) == NULL)
		return (NULL);
	memset(chap, 0, sizeof(*chap));
	chap->ev_ctx = ev_ctx;
	chap->mutex = mutex;
	chap->link = link;
	chap->log = log;
	chap->dir = dir;
	chap->retry = CHAP_MAXTRY;

	/* Get link auth config and auth type */
	chap->aconf = *ppp_link_auth_get_config(link);
	chap->auth = ppp_link_get_auth(link, dir);
	switch (chap->auth->index) {
	case PPP_AUTH_CHAP_MD5:
		chap->type = &ppp_auth_chap_md5;
		break;
	case PPP_AUTH_CHAP_MSV1:
		chap->type = &ppp_auth_chap_msv1;
		break;
	case PPP_AUTH_CHAP_MSV2:
		chap->type = &ppp_auth_chap_msv2;
		break;
	default:
		errno = EPROTONOSUPPORT;
		FREE(CHAP_MTYPE, chap);
		return (NULL);
	}
	chap->cred.type = chap->auth->index;

	/* Return protocol */
	*protop = PPP_PROTO_CHAP;

	/* If sending auth, wait for peer's challenge */
	if (dir == PPP_PEER)
		return (chap);

	/* If receiving auth, send first challenge */
	ppp_auth_chap_send_challenge(chap);

	/* Done */
	return (chap);
}

/*
 * Cancel CHAP
 */
void
ppp_auth_chap_cancel(void *arg)
{
	struct ppp_auth_chap *chap = arg;

	pevent_unregister(&chap->timer);
	ppp_log_close(&chap->log);
	FREE(CHAP_MTYPE, chap);
}

/*
 * Handle timeout event.
 */
static void
ppp_auth_chap_timeout(void *arg)
{
	struct ppp_auth_chap *chap = arg;

	/* Logging */
	LOG(LOG_DEBUG, "%s timeout", chap->dir == PPP_SELF ?
	    chap_codes[CHAP_CHALLENGE] : chap_codes[CHAP_RESPONSE]);

	/* Cancel timeout event */
	pevent_unregister(&chap->timer);

	/* Send another challenge or response? */
	if (chap->retry <= 0) {
		ppp_link_auth_complete(chap->link, chap->dir, NULL, NULL);
		return;
	}

	/* Send challenge or response again */
	if (chap->dir == PPP_SELF)
		ppp_auth_chap_send_challenge(chap);
	else
		ppp_auth_chap_send_response(chap);
}

/*
 * Handle CHAP input; "data" is guaranteed to be an aligned pointer.
 */
void
ppp_auth_chap_input(void *arg, int dir, void *data, size_t len)
{
	struct ppp_auth_chap *chap = arg;
	struct ppp_fsm_pkt *const pkt = data;
	u_char value[PPP_MAX_AUTHVALUE];
	char name[PPP_MAX_AUTHNAME];
	int vlen;

	if (len < sizeof(*pkt))
		return;
	pkt->length = ntohs(pkt->length);
	if (pkt->length > len)
		return;
	if (pkt->length < len)
		len = pkt->length;
	len -= sizeof(*pkt);
	switch (pkt->code) {
	case CHAP_CHALLENGE:
	    {
		struct ppp_auth_cred_chap *const cred = &chap->cred.u.chap;

		/* Check direction */
		if (dir != PPP_PEER)
			break;

		/* Logging */
		LOG(LOG_DEBUG, "rec'd %s #%u", chap_codes[pkt->code], pkt->id);

		/* Parse out packet contents */
		if (ppp_chap_unpack(pkt->data, len, name, value, &vlen) == -1) {
			LOG(LOG_NOTICE, "rec'd malformed %s",
			    chap_codes[pkt->code]);
			break;
		}

#ifdef notyet
		/* Don't respond to our own outstanding challenge */
		/*
		 * Don't respond to a challenge that looks like it came from
		 * us and has the wrong origination value embedded in it. This
		 * avoids a security hole associated with using the same CHAP
		 * password to authenticate in both directions on a link.
		 */
#endif

		/* Check challenge length (fixed for MS-CHAP types) */
		if (chap->type->cfixed && vlen != chap->type->clen) {
			LOG(LOG_NOTICE, "wrong %s length %u != %u"
			    " for %s", chap_codes[pkt->code],
			    vlen, chap->type->clen, chap->auth->name);
			break;
		}

		/* Ignore if already handling a previous challenge */
		if (ppp_link_auth_in_progress(chap->link, chap->dir)) {
			LOG(LOG_DEBUG, "ignoring packet, action pending");
			break;
		}

		/* Partially fill in credentials based on challenge info */
		memset(cred, 0, sizeof(*cred));
		strlcpy(cred->name, name, sizeof(cred->name));	/* XXX */
		cred->chal_len = vlen;
		memcpy(cred->chal_data, value, cred->chal_len);
		if (chap->type->set_id != NULL)
			(*chap->type->set_id)(cred, pkt->id);
		if (chap->auth->index == PPP_AUTH_CHAP_MSV2) {
			if (ppp_util_random(cred->u.msv2.peer_chal,
			    sizeof(cred->u.msv2.peer_chal)) == -1) {
				LOG(LOG_NOTICE, "%s: %m", "ppp_util_random");
				break;
			}
		}

		/* Acquire credentials */
		if (ppp_link_authorize(chap->link, chap->dir,
		    &chap->cred, ppp_auth_chap_acquire_finish) == -1) {
			ppp_link_auth_complete(chap->link,
			    chap->dir, NULL, NULL);
			break;
		}

		/* Save peer's id for my response */
		chap->id = pkt->id;

		/* Now wait for credentials acquisition to finish */
		break;
	    }
	case CHAP_RESPONSE:
	    {
		struct ppp_auth_cred_chap *const cred = &chap->cred.u.chap;
		int pvlen;

		/* Check direction */
		if (dir != PPP_SELF)
			break;

		/* Logging */
		LOG(LOG_DEBUG, "rec'd %s #%u", chap_codes[pkt->code], pkt->id);

		/* Stop timer */
		pevent_unregister(&chap->timer);

		/* Ignore if already checking a previous response */
		if (ppp_link_auth_in_progress(chap->link, chap->dir)) {
			LOG(LOG_DEBUG, "ignoring packet, action pending");
			break;
		}

		/* Fill out peer credentials using response packet */
		if (ppp_chap_unpack(pkt->data, len, cred->name,
		    (u_char *)&cred->u + chap->type->roff, &pvlen) == -1) {
			LOG(LOG_NOTICE, "rec'd malformed %s",
			    chap_codes[pkt->code]);
			break;
		}
		if (chap->type->set_id != NULL)
			(*chap->type->set_id)(cred, pkt->id);

		/* Check credentials */
		if (ppp_link_authorize(chap->link, chap->dir,
		    &chap->cred, ppp_auth_chap_check_finish) == -1) {
			ppp_auth_chap_send_result(chap, pkt->id, 0);
			ppp_link_auth_complete(chap->link,
			    chap->dir, NULL, NULL);
			break;
		}

		/* Save peer's id for my response */
		chap->id = pkt->id;

		/* Now wait for check to finish */
		break;
	    }
	case CHAP_ACK:
	case CHAP_NAK:
	    {
		int valid = (pkt->code == CHAP_ACK);

		/* Check direction */
		if (dir != PPP_PEER)
			break;

		/* Logging */
		LOG(LOG_DEBUG, "rec'd %s #%u", chap_codes[pkt->code], pkt->id);

		/* Stop timer */
		pevent_unregister(&chap->timer);

		/* Do final stuff */
		if ((*chap->type->final)(&chap->cred.u.chap, chap->log,
		    valid, pkt->data, len, chap->resp.authresp) == -1) {
			LOG(LOG_NOTICE, "invalid CHAP %s",
			    chap_codes[pkt->code]);
			valid = 0;
		}

		/* Finish up */
		if (valid) {
			ppp_link_auth_complete(chap->link,
			    chap->dir, &chap->cred, &chap->resp.mppe);
		} else {
			ppp_link_auth_complete(chap->link,
			    chap->dir, NULL, NULL);
		}
		break;
	    }
	default:
		break;
	}
}

/*
 * Continue after a successful credentials acquisition.
 */
static void
ppp_auth_chap_acquire_finish(void *arg,
	const struct ppp_auth_cred *creds, const struct ppp_auth_resp *resp)
{
	struct ppp_auth_chap *const chap = arg;
	struct ppp_auth_cred_chap *const cred = &chap->cred.u.chap;

	/* Copy credentials */
	chap->cred = *creds;

	/* Sanitize credentials */
	cred->name[sizeof(cred->name) - 1] = '\0';
	cred->chal_len = MAX(cred->chal_len, sizeof(cred->chal_data));

	/* Send response */
	ppp_auth_chap_send_response(chap);
}

/*
 * Continue after a successful credentials check.
 */
static void
ppp_auth_chap_check_finish(void *arg,
	const struct ppp_auth_cred *creds, const struct ppp_auth_resp *resp)
{
	struct ppp_auth_chap *const chap = arg;
	struct ppp_auth_cred_chap *const cred = &chap->cred.u.chap;
	int valid = (*resp->errmsg == '\0');

	/* Copy response */
	chap->resp = *resp;

	/* Report validity */
	if (valid) {
		LOG(LOG_INFO, "rec'd %s credentials for \"%s\"",
		    "valid", cred->name);
	} else {
		LOG(LOG_NOTICE, "rec'd %s credentials for \"%s\": %s",
		    "invalid", cred->name, resp->errmsg);
	}

	/* Send result */
	ppp_auth_chap_send_result(chap, chap->id, valid);

	/* Finish up */
	ppp_link_auth_complete(chap->link,
	    chap->dir, valid ? &chap->cred : NULL, &chap->resp.mppe);
}

/*
 * Send a CHAP challenge
 */
static void
ppp_auth_chap_send_challenge(struct ppp_auth_chap *chap)
{
	struct ppp_auth_cred_chap *cred = &chap->cred.u.chap;

	/* Create a challenge (first time only) */
	if (chap->retry == CHAP_MAXTRY) {

		/* Generate random challenge bytes */
		if (ppp_util_random(cred->chal_data, chap->type->clen) == -1) {
			LOG(LOG_ERR, "%s: %m", "ppp_util_random");
			return;
		}
		cred->chal_len = chap->type->clen;

		/* Set id field (if appropriate) */
		if (chap->type->set_id != NULL)
			(*chap->type->set_id)(&chap->cred.u.chap, ++chap->id);
	}

	/* Send packet */
	ppp_chap_send_value(chap, CHAP_CHALLENGE,
	    cred->chal_data, cred->chal_len, cred->name);
}

/*
 * Send a CHAP response.
 */
static void
ppp_auth_chap_send_response(struct ppp_auth_chap *chap)
{
	struct ppp_auth_cred_chap *const cred = &chap->cred.u.chap;
	u_char value[PPP_MAX_AUTHVALUE];

	/* Send response */
	memcpy((u_char *)&cred->u + chap->type->roff,
	    value, chap->type->rlen);
	ppp_chap_send_value(chap, CHAP_RESPONSE,
	    value, chap->type->rlen, cred->name);
}

/*
 * Send a challenge or response packet.
 */
static void
ppp_chap_send_value(struct ppp_auth_chap *chap, u_char code,
	const u_char *value, size_t vlen, const char *name)
{
	union {
	    u_char buf[sizeof(struct ppp_fsm_pkt) + 1
		+ PPP_MAX_AUTHVALUE + PPP_MAX_AUTHNAME];
	    struct ppp_fsm_pkt pkt;
	} u;
	struct ppp_fsm_pkt *const pkt = &u.pkt;

	/* Cancel previous timeout event (if any) and start another */
	pevent_unregister(&chap->timer);
	if (pevent_register(chap->ev_ctx, &chap->timer, 0, chap->mutex,
	    ppp_auth_chap_timeout, chap, PEVENT_TIME, CHAP_RETRY * 1000) == -1)
		LOG(LOG_ERR, "%s: %m", "pevent_register");

	/* Construct packet */
	pkt->id = chap->id;
	pkt->code = code;
	pkt->length = htons(sizeof(*pkt) + 1 + vlen + strlen(name));
	pkt->data[0] = vlen;
	memcpy(pkt->data + 1, value, vlen);
	memcpy(pkt->data + 1 + vlen, name, strlen(name));

	/* Logging */
	LOG(LOG_DEBUG, "xmit %s #%u", chap_codes[code], chap->id);

	/* Send packet */
	ppp_link_write(chap->link, PPP_PROTO_CHAP, pkt, ntohs(pkt->length));

	/* Decrement retry counter */
	chap->retry--;
}

/*
 * Send a CHAP result
 */
static void
ppp_auth_chap_send_result(struct ppp_auth_chap *chap, u_char id, int ack)
{
	union {
	    u_char buf[sizeof(struct ppp_fsm_pkt) + CHAP_MSG_BUFSIZE];
	    struct ppp_fsm_pkt pkt;
	} u;
	struct ppp_fsm_pkt *const pkt = &u.pkt;
	struct ppp_auth_cred_chap *cred = &chap->cred.u.chap;
	int i;

	/* Construct packet */
	pkt->id = id;
	pkt->code = ack ? CHAP_ACK : CHAP_NAK;

	/* Add response string */
	switch (chap->auth->index) {
	case PPP_AUTH_CHAP_MSV1:
		strlcpy(pkt->data, ack ? MSCHAPV1_MSG_ACK
		    : MSCHAPV1_MSG_NAK, CHAP_MSG_BUFSIZE);
		break;
	case PPP_AUTH_CHAP_MSV2:
		if (ack) {
			char hex[(PPP_MSOFTV2_AUTHRESP_LEN * 2) + 1];

			for (i = 0; i < PPP_MSOFTV2_AUTHRESP_LEN; i++) {
				sprintf(hex + (i * 2),
				    "%02X", chap->resp.authresp[i]);
			}
			snprintf(pkt->data, CHAP_MSG_BUFSIZE, "S=%s", hex);
		} else {
			char cbuf[(2 * PPP_MSOFTV2_CHAL_LEN) + 1];

			for (i = 0; i < PPP_MSOFTV2_CHAL_LEN; i++) {
				sprintf(cbuf + (2 * i),
				    "%02X", cred->u.msv2.peer_chal[i]);
			}
			snprintf(pkt->data, CHAP_MSG_BUFSIZE,
			    "E=691 R=0 C=%s V=3 M=%s", cbuf, MSCHAPV2_MSG_NAK);
		}
		break;
	default:
		strlcpy(pkt->data, ack ? CHAP_MSG_ACK
		    : CHAP_MSG_NAK, CHAP_MSG_BUFSIZE);
		break;
	}
	pkt->length = htons(sizeof(*pkt) + strlen(pkt->data));

	/* Logging */
	LOG(LOG_DEBUG, "xmit %s #%u", chap_codes[pkt->code], chap->id);

	/* Send packet */
	ppp_link_write(chap->link, PPP_PROTO_CHAP, pkt, ntohs(pkt->length));
}

/*
 * Decode a CHAP challenge or response packet.
 */
static int
ppp_chap_unpack(const u_char *data, size_t len,
	char *name, u_char *value, int *vlenp)
{
	int nlen;
	int vlen;

	/* Check well-formedness */
	if (len < 1
	    || (vlen = data[0]) < 1
	    || vlen > PPP_MAX_AUTHVALUE
	    || (nlen = len - vlen - 1) < 0
	    || nlen > PPP_MAX_AUTHNAME - 1)
		return (-1);

	/* Get stuff */
	memcpy(name, data + 1 + vlen, nlen);
	name[nlen] = '\0';
	memcpy(value, data + 1, vlen);
	*vlenp = vlen;
	return (0);
}


