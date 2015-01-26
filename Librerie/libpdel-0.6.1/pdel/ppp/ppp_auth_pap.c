
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
#include "ppp/ppp_auth_pap.h"

#define PAP_MTYPE		"ppp_authtype.pap"

#define PAP_RETRY		3

#define PAP_REQUEST		1
#define PAP_ACK			2
#define PAP_NAK			3

#define PAP_MSG_ACK		"Authorization successful"
#define PAP_MSG_NAK		"Authorization failed"
#define PAP_MSG_BUFSIZE		64

/* PAP info structure */
struct ppp_auth_pap {
	struct ppp_link		*link;
	struct ppp_log		*log;
	struct ppp_auth_config	aconf;
	struct ppp_auth_cred	cred;
	struct ppp_auth_resp	resp;
	int			dir;
	u_char			id;
	struct pevent_ctx	*ev_ctx;
	struct pevent		*timer;
	pthread_mutex_t		*mutex;
};

/* Internal functions */
static void	ppp_auth_pap_send_request(struct ppp_auth_pap *pap);
static void	ppp_auth_pap_send_response(struct ppp_auth_pap *pap,
			u_char id, int ack);

static ppp_link_auth_finish_t		ppp_auth_pap_acquire_finish;
static ppp_link_auth_finish_t		ppp_auth_pap_check_finish;

/* Internal variables */
static const	char *pap_codes[] = {
	"zero",
	"request",
	"ack",
	"nak"
};

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(pap->log, sev, fmt , ## args)

/*
 * Start PAP
 */
void *
ppp_auth_pap_start(struct pevent_ctx *ev_ctx, struct ppp_link *link,
	pthread_mutex_t *mutex, int dir, u_int16_t *protop, struct ppp_log *log)
{
	struct ppp_auth_pap *pap;

	/* Create info structure */
	if ((pap = MALLOC(PAP_MTYPE, sizeof(*pap))) == NULL)
		return (NULL);
	memset(pap, 0, sizeof(*pap));
	pap->ev_ctx = ev_ctx;
	pap->mutex = mutex;
	pap->link = link;
	pap->log = log;
	pap->dir = dir;
	pap->cred.type = PPP_AUTH_PAP;

	/* Get link auth config */
	pap->aconf = *ppp_link_auth_get_config(link);

	/* Return protocol */
	*protop = PPP_PROTO_PAP;

	/* If receiving auth, wait for peer's request */
	if (dir == PPP_SELF)
		return (pap);

	/* If sending auth, acquire credentials */
	if (ppp_link_authorize(pap->link, pap->dir,
	    &pap->cred, ppp_auth_pap_acquire_finish) == -1) {
		FREE(PAP_MTYPE, pap);
		return (NULL);
	}

	/* Done */
	return (pap);
}

/*
 * Cancel PAP
 */
void
ppp_auth_pap_cancel(void *arg)
{
	struct ppp_auth_pap *pap = arg;

	pevent_unregister(&pap->timer);
	ppp_log_close(&pap->log);
	FREE(PAP_MTYPE, pap);
}

/*
 * Handle PAP input; "data" is guaranteed to be an aligned pointer.
 */
void
ppp_auth_pap_input(void *arg, int dir, void *data, size_t len)
{
	struct ppp_auth_pap *pap = arg;
	struct ppp_fsm_pkt *const pkt = data;

	if (len < sizeof(*pkt) + 2)
		return;
	pkt->length = ntohs(pkt->length);
	if (pkt->length > len)
		return;
	if (pkt->length < len)
		len = pkt->length;
	len -= sizeof(*pkt);
	switch (pkt->code) {
	case PAP_REQUEST:
	    {
		struct ppp_auth_cred_pap *const cred = &pap->cred.u.pap;
		u_char nlen;
		u_char plen;

		/* Check direction */
		if (dir != PPP_SELF)
			break;

		/* Logging */
		LOG(LOG_DEBUG, "rec'd %s #%u", pap_codes[pkt->code], pkt->id);

		/* Extract username and password */
		if (len < 1)
			return;
		nlen = pkt->data[0];
		if (len < 1 + nlen + 1)
			return;
		plen = pkt->data[1 + nlen];
		if (len < 1 + nlen + 1 + plen)
			return;
		strncpy(cred->name, pkt->data + 1,
		    MIN(nlen, sizeof(cred->name) - 1));
		cred->name[sizeof(cred->name) - 1] = '\0';
		strncpy(cred->password, pkt->data + 1 + nlen + 1,
		    MIN(plen, sizeof(cred->password) - 1));
		cred->password[sizeof(cred->password) - 1] = '\0';

		/* Ignore if already checking a previous request */
		if (ppp_link_auth_in_progress(pap->link, pap->dir)) {
			LOG(LOG_DEBUG, "ignoring packet, action pending");
			break;
		}

		/* Check credentials */
		if (ppp_link_authorize(pap->link, pap->dir,
		    &pap->cred, ppp_auth_pap_check_finish) == -1) {
			ppp_auth_pap_send_response(pap, pap->id, 0);
			ppp_link_auth_complete(pap->link, pap->dir, NULL, NULL);
			break;
		}

		/* Save peer's id for my response */
		pap->id = pkt->id;

		/* Now wait for credentials check to finish */
		break;
	    }
	case PAP_ACK:
	case PAP_NAK:

		/* Check direction */
		if (dir != PPP_PEER)
			break;

		/* Logging */
		LOG(LOG_DEBUG, "rec'd %s #%u", pap_codes[pkt->code], pkt->id);

		/* Stop timer */
		pevent_unregister(&pap->timer);

		/* Finish up */
		ppp_link_auth_complete(pap->link, pap->dir,
		    pkt->code == PAP_ACK ? &pap->cred : NULL, NULL);
		break;
	default:
		return;
	}
}

/*
 * Continue after a successful credentials acquisition.
 */
static void
ppp_auth_pap_acquire_finish(void *arg,
	const struct ppp_auth_cred *creds, const struct ppp_auth_resp *resp)
{
	struct ppp_auth_pap *const pap = arg;
	struct ppp_auth_cred_pap *const cred = &pap->cred.u.pap;

	/* Copy credentials */
	pap->cred = *creds;

	/* Sanitize credentials */
	cred->name[sizeof(cred->name) - 1] = '\0';
	cred->password[sizeof(cred->password) - 1] = '\0';

	/* Send first PAP request */
	ppp_auth_pap_send_request(pap);
}

/*
 * Continue after a successful credentials check.
 */
static void
ppp_auth_pap_check_finish(void *arg,
	const struct ppp_auth_cred *creds, const struct ppp_auth_resp *resp)
{
	struct ppp_auth_pap *const pap = arg;
	struct ppp_auth_cred_pap *const cred = &pap->cred.u.pap;
	int valid = (*resp->errmsg == '\0');

	/* Copy response */
	pap->resp = *resp;

	/* Report validity */
	if (valid) {
		LOG(LOG_INFO, "rec'd %s credentials for \"%s\"",
		    "valid", cred->name);
	} else {
		LOG(LOG_NOTICE, "rec'd %s credentials for \"%s\": %s",
		    "invalid", cred->name, resp->errmsg);
	}

	/* Send response */
	ppp_auth_pap_send_response(pap, pap->id, valid);

	/* Finish up */
	ppp_link_auth_complete(pap->link,
	    pap->dir, valid ? &pap->cred : NULL, NULL);
}

/*
 * Send a PAP request
 */
static void
ppp_auth_pap_send_request(struct ppp_auth_pap *pap)
{
	struct ppp_auth_cred_pap *const cred = &pap->cred.u.pap;
	union {
	    u_char buf[sizeof(struct ppp_fsm_pkt)
		+ sizeof(cred->name) + sizeof(cred->password)];
	    struct ppp_fsm_pkt pkt;
	} u;
	struct ppp_fsm_pkt *const pkt = &u.pkt;

	/* Cancel previous timeout event (if any) and start another */
	pevent_unregister(&pap->timer);
	if (pevent_register(pap->ev_ctx, &pap->timer, 0,
	    pap->mutex, (pevent_handler_t *)ppp_auth_pap_send_request,
	    pap, PEVENT_TIME, PAP_RETRY * 1000) == -1)
		LOG(LOG_ERR, "%s: %m", "pevent_register");

	/* Construct packet */
	pkt->id = ++pap->id;
	pkt->code = PAP_REQUEST;
	pkt->length = htons(sizeof(*pkt)
	    + 1 + strlen(cred->name) + 1 + strlen(cred->password));
	pkt->data[0] = strlen(cred->name);
	memcpy(pkt->data + 1, cred->name, strlen(cred->name));
	pkt->data[1 + strlen(cred->name)] = strlen(cred->password);
	memcpy(pkt->data + 1 + strlen(cred->name) + 1,
	    cred->password, strlen(cred->password));

	/* Logging */
	LOG(LOG_DEBUG, "xmit %s #%u", pap_codes[pkt->code], pkt->id);

	/* Send packet */
	ppp_link_write(pap->link, PPP_PROTO_PAP, pkt, ntohs(pkt->length));
}

/*
 * Send a PAP response
 */
static void
ppp_auth_pap_send_response(struct ppp_auth_pap *pap, u_char id, int ack)
{
	union {
	    u_char buf[sizeof(struct ppp_fsm_pkt) + 1 + PAP_MSG_BUFSIZE];
	    struct ppp_fsm_pkt pkt;
	} u;
	struct ppp_fsm_pkt *const pkt = &u.pkt;

	/* Construct packet */
	pkt->id = id;
	pkt->code = ack ? PAP_ACK : PAP_NAK;
	snprintf(pkt->data + 1, PAP_MSG_BUFSIZE,
	    "%s", ack ? PAP_MSG_ACK : PAP_MSG_NAK);
	pkt->data[0] = strlen(pkt->data + 1);
	pkt->length = htons(sizeof(*pkt) + 1 + strlen(pkt->data + 1));

	/* Logging */
	LOG(LOG_DEBUG, "xmit %s #%u", pap_codes[pkt->code], pkt->id);

	/* Send packet */
	ppp_link_write(pap->link, PPP_PROTO_PAP, pkt, ntohs(pkt->length));
}


