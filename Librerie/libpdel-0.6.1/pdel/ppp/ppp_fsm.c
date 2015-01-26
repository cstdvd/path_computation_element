
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_util.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_fsm.h"

/* FSM defaults */
#define FSM_MAX_CONFIGURE	10
#define FSM_MAX_TERMINATE	3
#define FSM_MAX_FAILURE		8
#define FSM_TIMEOUT		2

/* Memory type */
#define FSM_MTYPE		"ppp_fsm"

/* Max amount of packet data to copy & send back */
#define MAX_PKTCOPY		200

/* FSM events */
enum fsm_event {
	UP		=0,
	DOWN		=1,
	OPEN		=2,
	CLOSE		=3,
	TO_P		=4,
	TO_M		=5,
	RCR_P		=6,
	RCR_M		=7,
	RCA		=8,
	RCN		=9,
	RTR		=10,
	RTA		=11,
	RXJ_P		=12,
	RXJ_M		=13
};
#define FSM_EVENT_MAX	14

/* Actions to take on events */
#define TLU		0x0010		/* this layer up */
#define TLD		0x0020		/* this layer down */
#define TLS		0x0040		/* this layer started */
#define TLF		0x0080		/* this layer finished */
#define IRC		0x0100		/* init restart counter */
#define ZRC		0x0200		/* zero restart counter */
#define SCR		0x0400		/* send config request */
#define SCA		0x0800		/* send config ack */
#define SCN		0x1000		/* send config nak/rej */
#define STR		0x2000		/* send terminate request */
#define STA		0x4000		/* send terminate ack */

#define STMASK		0x000f		/* next state mask */
#define NA		0xffff		/* impossible event */

/* Configurations we keep */
#define FSM_CONF_SELF		0	/* my requested config */
#define FSM_CONF_PEER		1	/* peer requested config */
#define FSM_CONF_NAK		2	/* nak'd peer config */
#define FSM_CONF_REJ		3	/* rejected peer config */
#define FSM_CONF_MAX		4

/* Information describing an instance of an FSM */
struct ppp_fsm {
	struct ppp_fsm_instance	*inst;		/* fsm instance object */
	enum ppp_fsm_state	state;		/* fsm state */
	struct ppp_log		*log;		/* log object */
	struct ppp_fsm_options	*config[FSM_CONF_MAX];	/* config options */
	u_char			ids[FSM_CODE_MAX];	/* packet ids */
	u_char			rejcode[FSM_CODE_MAX];	/* rejected codes */
	short			restart;	/* restart counter */
	short			failure[2];	/* failure counter */
	time_t			last_heard;	/* time last heard from */
	struct ppp_fsm_output	dead;		/* if dead and reason why */
	struct pevent_ctx	*ev_ctx;	/* event context */
	pthread_mutex_t		*mutex;		/* mutex */
	struct pevent		*timer;		/* restart timer */
	struct mesg_port	*outport;	/* where output goes */
};

#define FSM_TIMER_STATE(state)						\
	((state) >= FSM_STATE_CLOSING && (state) != FSM_STATE_OPENED)

#define FSM_DEAD(fsm)							\
	(((fsm)->state == FSM_STATE_INITIAL				\
	      || (fsm)->state == FSM_STATE_CLOSED)			\
	    && (fsm)->dead.u.down.reason != 0)

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(fsm->log, sev, fmt , ## args)

/*
 * RFC 1661 PPP state transition table
 *
 * Differences from RFC 1661:
 *	TLS added in [OPEN, CLOSED]
 *	TLF added in [DOWN, CLOSING]
 *	TLS added in [RCR+, STOPPED]
 *	TLS added in [RCR-, STOPPED]
 *	RUC and RXR events removed (handled directly)
 *
 * The extra TLS/TLF actions are to keep intention of lower layer in sync
 * with the intention of this layer (i.e., if TLS -> lower layer OPEN and
 * TLF -> lower layer CLOSE).
 */
static const u_int16_t fsm_actions[FSM_EVENT_MAX][FSM_STATE_MAX] = {

/*INITL	STARTNG	CLOSED	STOPPED	CLOSING	STOPPNG	REQ-SNT	ACK-RCD	ACK-SNT	OPENED*/

/* Up */
{ 2,	IRC|SCR|6,
		NA,	NA,	NA,	NA,	NA,	NA,	NA,	NA },
/* Down */
{ NA,	NA,	0,	TLS|1,	TLF|0,	1,	1,	1,	1,	TLD|1 },
/* Open */
{ TLS|1,1,	TLS|IRC|SCR|6,
			3,	5,	5,	6,	7,	8,	9 },
/* Close */
{ 0,	TLF|0,	2,	2,	4,	4,	IRC|STR|4,
							IRC|STR|4,
								IRC|STR|4,
								 TLD|IRC|STR|4},
/* TO+ */
{ NA,	NA,	NA,	NA,	STR|4,	STR|5,	SCR|6,	SCR|6,	SCR|8,	NA },
/* TO- */
{ NA,	NA,	NA,	NA,	TLF|2,	TLF|3,	TLF|3,	TLF|3,	TLF|3,	NA },
/* RCR+ */
{ NA,	NA,	STA|2,	TLS|IRC|SCR|SCA|8,
				4,	5,	SCA|8,	SCA|TLU|9,
								SCA|8,
								 TLD|SCR|SCA|8},
/* RCR- */
{ NA,	NA,	STA|2,	TLS|IRC|SCR|SCN|6,
				4,	5,	SCN|6,	SCN|7,	SCN|6,
								 TLD|SCR|SCN|6},
/* RCA */
{ NA,	NA,	STA|2,	STA|3,	4,	5,	IRC|7,
							SCR|6,	IRC|TLU|9,
								  TLD|SCR|6 },
/* RCN */
{ NA,	NA,	STA|2,	STA|3,	4,	5,	IRC|SCR|6,
							SCR|6,	IRC|SCR|8,
								  TLD|SCR|6 },
/* RTR */
{ NA,	NA,	STA|2,	STA|3,	STA|4,	STA|5,	STA|6,	STA|6,	STA|6,
								 TLD|ZRC|STA|5},
/* RTA */
{ NA,	NA,	2,	3,	TLF|2,	TLF|3,	6,	6,	8,
								  TLD|SCR|6 },
/* RXJ+ */
{ NA,	NA,	2,	3,	4,	5,	6,	6,	8,	9 },
/* RXJ- */
{ NA,	NA,	TLF|2,	TLF|3,	TLF|2,	TLF|3,	TLF|3,	TLF|3,	TLF|3,
								 TLD|IRC|STR|5},
};

/* Minimum packet data lengths */
static const	u_char fsm_minlen[FSM_CODE_MAX] = {
	0,	/* FSM_CODE_VENDOR */
	0,	/* FSM_CODE_CONFIGREQ */
	0,	/* FSM_CODE_CONFIGACK */
	0,	/* FSM_CODE_CONFIGNAK */
	0,	/* FSM_CODE_CONFIGREJ */
	0,	/* FSM_CODE_TERMREQ */
	0,	/* FSM_CODE_TERMACK */
	1,	/* FSM_CODE_CODEREJ */
	2,	/* FSM_CODE_PROTOREJ */
	4,	/* FSM_CODE_ECHOREQ */
	4,	/* FSM_CODE_ECHOREP */
	4,	/* FSM_CODE_DISCREQ */
	4,	/* FSM_CODE_IDENT */
	8,	/* FSM_CODE_TIMEREM */
	0,	/* FSM_CODE_RESETREQ */
	0,	/* FSM_CODE_RESETACK */
};

/*
 * Internal functions
 */
static void	ppp_fsm_input_packet(struct ppp_fsm *fsm,
			const u_char *data, u_int dlen);
static void	ppp_fsm_event(struct ppp_fsm *fsm, int event);
static void	ppp_fsm_send_config(struct ppp_fsm *fsm,
			enum ppp_fsm_code code);
static void	ppp_fsm_send_packet(struct ppp_fsm *fsm, u_char code,
			const void *data, u_int len);
static void	ppp_fsm_record(struct ppp_fsm *fsm,
			/* enum ppp_fsm_reason reason, */ ...);
static int	ppp_fsm_output(struct ppp_fsm *fsm,
			enum ppp_fsmoutput type, ...);
static void	ppp_fsm_output_build(struct ppp_fsm_output *output,
			enum ppp_fsmoutput type, va_list args);
static void	ppp_fsm_output_dead(struct ppp_fsm *fsm);
static void	ppp_fsm_syserr(struct ppp_fsm *fsm, const char *func);

static void	ppp_fsm_log_pkt(struct ppp_fsm *fsm, int sev,
			const char *prefix, const u_char *pkt);

static pevent_handler_t	ppp_fsm_timeout;

static const	char *st2str(u_int state);
static const	char *ev2str(u_int event);
static const	char *cd2str(u_int code);

/***********************************************************************
			PUBLIC API FUNCTIONS
***********************************************************************/

/*
 * Create a new FSM.
 *
 * "inst" is freed when the FSM is destroyed.
 */
struct ppp_fsm *
ppp_fsm_create(struct pevent_ctx *ev_ctx, pthread_mutex_t *mutex,
	struct ppp_fsm_instance *inst, struct ppp_log *log)
{
	struct ppp_fsm *fsm;
	int i;

	/* Get new FSM object */
	if ((fsm = MALLOC(FSM_MTYPE, sizeof(*fsm))) == NULL)
		return (NULL);
	memset(fsm, 0, sizeof(*fsm));
	fsm->ev_ctx = ev_ctx;
	fsm->mutex = mutex;
	fsm->inst = inst;
	fsm->state = FSM_STATE_INITIAL;

	/* Prefix log with FSM name */
	if ((fsm->log = ppp_log_prefix(log,
	    "%s: ", inst->type->name)) == NULL) {
		FREE(FSM_MTYPE, fsm);
		return (NULL);
	}

	/* Get message port */
	if ((fsm->outport = mesg_port_create(inst->type->name)) == NULL) {
		FREE(FSM_MTYPE, fsm);
		return (NULL);
	}

	/* For 'shell' FSM's like MP LCP, no config required */
	if ((inst->type->sup_codes & (1 << FSM_CODE_CONFIGREQ)) == 0) {
		fsm->state = FSM_STATE_OPENED;
		return (fsm);
	}

	/* Initialize configuration options */
	for (i = 0; i < FSM_CONF_MAX; i++) {
		if ((fsm->config[i] = ppp_fsm_option_create()) == NULL) {
			while (i-- > 0)
				ppp_fsm_option_destroy(&fsm->config[i]);
			mesg_port_destroy(&fsm->outport);
			FREE(FSM_MTYPE, fsm);
			return (NULL);
		}
	}

	/* Done */
	inst->fsm = fsm;
	return (fsm);
}

/*
 * Destroy an FSM
 */
void
ppp_fsm_destroy(struct ppp_fsm **fsmp)
{
	struct ppp_fsm *const fsm = *fsmp;
	struct ppp_fsm_output *output;
	int i;

	if (fsm == NULL)
		return;
	*fsmp = NULL;
	pevent_unregister(&fsm->timer);
	for (i = 0; i < FSM_CONF_MAX; i++)
		ppp_fsm_option_destroy(&fsm->config[i]);
	while ((output = mesg_port_get(fsm->outport, 0)) != NULL)
		ppp_fsm_free_output(output);
	mesg_port_destroy(&fsm->outport);
	(*fsm->inst->type->destroy)(fsm->inst);
	ppp_log_close(&fsm->log);
	FREE(FSM_MTYPE, fsm);
}

/*
 * Get output port.
 */
struct mesg_port *
ppp_fsm_get_outport(struct ppp_fsm *fsm)
{
	return (fsm->outport);
}

/*
 * Free an FSM output structure.
 */
void
ppp_fsm_free_output(struct ppp_fsm_output *output)
{
	switch (output->type) {
	case FSM_OUTPUT_DATA:
		FREE(FSM_MTYPE, output->u.data.data);
		break;
	default:
		break;
	}
	FREE(FSM_MTYPE, output);
}

/*
 * Input something to the FSM.
 */
void
ppp_fsm_input(struct ppp_fsm *fsm, enum ppp_fsm_input input, ...)
{
	const u_char *data;
	va_list args;
	u_int dlen;

	/* If we're dead, ignore it */
	if (FSM_DEAD(fsm))
		return;

	/* Handle input */
	va_start(args, input);
	switch (input) {
	case FSM_INPUT_OPEN:
		ppp_fsm_event(fsm, OPEN);
		break;
	case FSM_INPUT_CLOSE:
		ppp_fsm_record(fsm, FSM_REASON_CLOSE);
		ppp_fsm_event(fsm, CLOSE);
		break;
	case FSM_INPUT_UP:
		ppp_fsm_event(fsm, UP);
		break;
	case FSM_INPUT_DOWN_FATAL:
		ppp_fsm_record(fsm, FSM_REASON_DOWN_FATAL);
		ppp_fsm_event(fsm, DOWN);
		ppp_fsm_event(fsm, CLOSE);
		break;
	case FSM_INPUT_DOWN_NONFATAL:
		ppp_fsm_record(fsm, FSM_REASON_DOWN_NONFATAL);
		ppp_fsm_event(fsm, DOWN);
		break;
	case FSM_INPUT_RECD_PROTOREJ:
	    {
		u_int16_t proto;

		proto = va_arg(args, int);
		ppp_fsm_record(fsm, FSM_REASON_PROTOREJ, proto);
		ppp_fsm_event(fsm, RXJ_M);
		break;
	    }
	case FSM_INPUT_DATA:
		data = va_arg(args, const u_char *);
		dlen = va_arg(args, u_int);
		ppp_fsm_input_packet(fsm, data, dlen);
		break;
	case FSM_INPUT_XMIT_PROTOREJ:
	    {
		u_int16_t proto;
		u_int16_t *prj;
		u_int prlen;

		proto = va_arg(args, int);
		data = va_arg(args, const u_char *);
		dlen = va_arg(args, u_int);
		prlen = 2 + MIN(dlen, MAX_PKTCOPY);
		if ((prj = MALLOC(TYPED_MEM_TEMP, prlen)) == NULL)
			break;
		*prj = htons(proto);
		memcpy((char *)prj + 2, data, MIN(dlen, MAX_PKTCOPY));
		ppp_fsm_send_packet(fsm, FSM_CODE_PROTOREJ, prj, prlen);
		FREE(TYPED_MEM_TEMP, prj);
		break;
	    }
	default:
		LOG(LOG_ERR, "invalid input %d", input);
		break;
	}
	va_end(args);
}

/*
 * Get FSM state.
 */
enum ppp_fsm_state
ppp_fsm_get_state(struct ppp_fsm *fsm)
{
	return (fsm->state);
}

/*
 * Get time we last heard from the peer.
 */
time_t
ppp_fsm_last_heard(struct ppp_fsm *fsm)
{
	return (fsm->last_heard);
}

/*
 * Get underlying FSM instance.
 */
struct ppp_fsm_instance *
ppp_fsm_get_instance(struct ppp_fsm *fsm)
{
	return (fsm->inst);
}

/*
 * Send a reset-request.
 */
void
ppp_fsm_send_reset_req(struct ppp_fsm *fsm, const void *data, size_t dlen)
{
	fsm->ids[FSM_CODE_RESETREQ]++;
	ppp_fsm_send_packet(fsm, FSM_CODE_RESETREQ, data, dlen);
}

/*
 * Send a reset-ack.
 */
void
ppp_fsm_send_reset_ack(struct ppp_fsm *fsm, const void *data, size_t dlen)
{
	ppp_fsm_send_packet(fsm, FSM_CODE_RESETACK, data, dlen);
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

/*
 * Handle an incoming packet
 */
static void
ppp_fsm_input_packet(struct ppp_fsm *fsm, const u_char *pkt, u_int dlen)
{
	const u_char *const payload = pkt + sizeof(struct ppp_fsm_pkt);
	const struct ppp_fsm_type *const ftyp = fsm->inst->type;
	struct ppp_fsm_options *opts = NULL;
	struct ppp_fsm_pkt hdr;
	u_int len;

	/* Drop packet if it's unexpected */
	if (fsm->state == FSM_STATE_INITIAL
	    || fsm->state == FSM_STATE_STARTING)
		goto done;

	/* Check packet length */
	if (dlen < sizeof(hdr)) {
		LOG(LOG_NOTICE, "rec'd %s packet (%u bytes)", "runt", dlen);
		goto done;
	}

	/* Copy packet header into aligned memory */
	memcpy(&hdr, pkt, sizeof(hdr));

	/* Check packet length again */
	if ((len = ntohs(hdr.length)) < sizeof(hdr)) {
		LOG(LOG_NOTICE, "rec'd %s packet (%u bytes)", "runt", dlen);
		goto done;
	}
	if (len > dlen) {
		LOG(LOG_NOTICE, "rec'd %s packet (%u bytes)",
		    "truncated", dlen);
		goto done;
	}
	len -= sizeof(hdr);		/* get length of just the data part */

	/* Check code; send code-reject if not supported */
	if (hdr.code >= FSM_CODE_MAX
	    || (ftyp->sup_codes & (1 << hdr.code)) == 0) {
code_reject:	LOG(LOG_DEBUG, "rejecting unsupported code %u", hdr.code);
		ppp_fsm_send_packet(fsm, FSM_CODE_CODEREJ,
		    pkt, MIN(dlen, MAX_PKTCOPY));
		goto done;
	}

	/* Check data length */
	if (len < fsm_minlen[hdr.code]) {
		LOG(LOG_DEBUG + 1, "ignoring truncated packet");
		goto done;
	}

	/* Reset peer's idle time */
	fsm->last_heard = time(NULL);

	/* Initialize failure counters if appropriate */
	if (fsm->state < FSM_STATE_REQSENT) {
		fsm->failure[PPP_SELF] = FSM_MAX_FAILURE;
		fsm->failure[PPP_PEER] = FSM_MAX_FAILURE;
	}

	/* Logging */
	ppp_fsm_log_pkt(fsm, (hdr.code == FSM_CODE_ECHOREQ
	    || hdr.code == FSM_CODE_ECHOREP) ? LOG_DEBUG : LOG_INFO,
	    "recv", pkt);

	/* Extract encoded config options */
	switch (hdr.code) {
	case FSM_CODE_CONFIGREQ:
	case FSM_CODE_CONFIGACK:
	case FSM_CODE_CONFIGNAK:
	case FSM_CODE_CONFIGREJ:
		ppp_fsm_record(fsm, FSM_REASON_CONF, hdr.code);
		if ((opts = ppp_fsm_option_unpack(payload, len)) == NULL) {
			ppp_fsm_syserr(fsm, "malloc");
			goto close;
		}
		break;
	default:
		break;
	}

	/* Check magic number */
	switch (hdr.code) {
	case FSM_CODE_ECHOREQ:
	case FSM_CODE_ECHOREP:
	case FSM_CODE_IDENT:
	case FSM_CODE_DISCREQ:
	case FSM_CODE_TIMEREM:
	    {
		u_int32_t pkt_magic;
		u_int32_t req_magic;

		/* Get what peer's magic number ought to be */
		if (ftyp->get_magic == NULL)
			break;
		req_magic = (*ftyp->get_magic)(fsm->inst, PPP_PEER);

		/* Get actual magic number received */
		memcpy(&pkt_magic, payload, 4);
		pkt_magic = ntohl(pkt_magic);

		/*
		 * Only check if both magic numbers are non-zero and
		 * the FSM has reached the opened state.
		 */
		if (req_magic == 0
		    || pkt_magic == 0
		    || fsm->state != FSM_STATE_OPENED)
			break;

		/* If wrong, bail out */
		if (pkt_magic != req_magic) {
			LOG(LOG_NOTICE,
			    "rec'd %s with invalid magic# 0x%08x != 0x%08x",
			    cd2str(hdr.code), pkt_magic, req_magic);
			ppp_fsm_record(fsm, FSM_REASON_BADMAGIC);
			goto close;
		}
		break;
	    }
	default:
		break;
	}

	/* Deal with packet */
	switch (hdr.code) {
	case FSM_CODE_VENDOR:
		if (ftyp->recv_vendor == NULL)
			goto code_reject;
		(*ftyp->recv_vendor)(fsm->inst, payload, len);
		break;

	case FSM_CODE_CONFIGREQ:
	    {
		int ack;
		int i;

		/* Update reply id's */
		fsm->ids[FSM_CODE_CONFIGACK] = hdr.id;
		fsm->ids[FSM_CODE_CONFIGNAK] = hdr.id;
		fsm->ids[FSM_CODE_CONFIGREJ] = hdr.id;

		/* Update peer's requested options with new info */
		ppp_fsm_option_destroy(&fsm->config[FSM_CONF_PEER]);
		fsm->config[FSM_CONF_PEER] = opts;
		opts = NULL;				/* avoid double free */

		/* Reset nak and rej reply options */
		ppp_fsm_option_zero(fsm->config[FSM_CONF_NAK]);
		ppp_fsm_option_zero(fsm->config[FSM_CONF_REJ]);

		/* Examine peer's options for basic validity */
		for (i = 0; i < fsm->config[FSM_CONF_PEER]->num; i++) {
			const struct ppp_fsm_option *const opt
			    = &fsm->config[FSM_CONF_PEER]->opts[i];
			const struct ppp_fsm_optdesc *const desc
			    = ppp_fsm_option_desc(ftyp->options, opt);

			/* If not supported or invalid, reject it */
			if (desc == NULL
			    || !desc->supported
			    || opt->len < desc->min
			    || opt->len > desc->max) {

				/* Add to reject list */
				if (ppp_fsm_option_add(
				    fsm->config[FSM_CONF_REJ],
				    opt->type, opt->len, opt->data) == -1) {
					ppp_fsm_syserr(fsm, "malloc");
					goto close;
				}

				/* Remove from request list */
				ppp_fsm_option_del(
				    fsm->config[FSM_CONF_PEER], i--);
			}
		}

		/* Call FSM type method to deal with remaining options */
		if ((*ftyp->recv_conf_req)(fsm->inst,
		    fsm->config[FSM_CONF_PEER],
		    fsm->config[FSM_CONF_NAK],
		    fsm->config[FSM_CONF_REJ]) == -1)
			goto config_error;

		/* Check if not converging */
		if (fsm->config[FSM_CONF_NAK]->num > 0
		    && --fsm->failure[PPP_PEER] <= 0) {
			LOG(LOG_NOTICE, "negotiation failed to converge:"
			    " configuration not accepted by %s", "me");
			ppp_fsm_record(fsm, FSM_REASON_NEGOT);
			goto close;
		}

		/* Evoke RCR+ or RCR- event */
		ack = (fsm->config[FSM_CONF_NAK]->num
		    + fsm->config[FSM_CONF_REJ]->num == 0);
		if (ack)
			fsm->failure[PPP_PEER] = FSM_MAX_FAILURE;
		ppp_fsm_event(fsm, ack ? RCR_P : RCR_M);
		break;
	    }

	case FSM_CODE_CONFIGACK:

		/* Validate id and contents */
		if (hdr.id != fsm->ids[FSM_CODE_CONFIGREQ]) {
			LOG(LOG_DEBUG + 1, "ignoring id #%u", hdr.id);
			goto done;
		}
		if (!ppp_fsm_option_equal(opts, -1,
		    fsm->config[FSM_CONF_SELF], -1)) {
			LOG(LOG_DEBUG + 1, "ignoring altered contents");
			goto done;
		}

		/* Generate RCA event */
		fsm->ids[FSM_CODE_CONFIGREQ]++;
		fsm->failure[PPP_SELF] = FSM_MAX_FAILURE;
		ppp_fsm_event(fsm, RCA);
		break;

	case FSM_CODE_CONFIGNAK:
	case FSM_CODE_CONFIGREJ:
	    {
		int (*func)(struct ppp_fsm_instance *inst,
		    struct ppp_fsm_options *rej);

		/* Validate id */
		if (hdr.id != fsm->ids[FSM_CODE_CONFIGREQ]) {
			LOG(LOG_DEBUG + 1, "ignoring id #%u", hdr.id);
			goto done;
		}

		/* Check if not converging */
		if (hdr.code == FSM_CODE_CONFIGNAK
		    && --fsm->failure[PPP_SELF] <= 0) {
			LOG(LOG_NOTICE, "negotiation failed to converge:"
			    " configuration not accepted by %s", "peer");
			ppp_fsm_record(fsm, FSM_REASON_NEGOT);
			goto close;
		}

		/* Call implementation to deal with options */
		func = (hdr.code == FSM_CODE_CONFIGNAK) ?
		    ftyp->recv_conf_nak : ftyp->recv_conf_rej;
		if ((*func)(fsm->inst, opts) == -1) {
config_error:		if (errno == ELOOP)
				ppp_fsm_record(fsm, FSM_REASON_LOOPBACK);
			else if (errno == EINVAL)
				ppp_fsm_record(fsm, FSM_REASON_NEGOT);
			else
				ppp_fsm_record(fsm, FSM_REASON_SYSERR, errno);
			goto close;
		}

		/* Generate RCN event */
		fsm->ids[FSM_CODE_CONFIGREQ]++;
		ppp_fsm_event(fsm, RCN);
		break;
	    }

	case FSM_CODE_TERMREQ:

		/* Generate RTR event */
		fsm->ids[FSM_CODE_TERMACK] = hdr.id;
		ppp_fsm_record(fsm, FSM_REASON_TERM);
		ppp_fsm_event(fsm, RTR);
		goto close;

	case FSM_CODE_TERMACK:

		/* Validate id */
		if (hdr.id != fsm->ids[FSM_CODE_TERMREQ]) {
			LOG(LOG_DEBUG + 1, "ignoring id #%u", hdr.id);
			goto done;
		}
		fsm->ids[FSM_CODE_TERMREQ]++;

		/* Generate RTA event */
		fsm->ids[FSM_CODE_TERMACK] = hdr.id;
		ppp_fsm_event(fsm, RTA);
		break;

	case FSM_CODE_CODEREJ:
	    {
		const u_char code = payload[0];

		/* See if rejected code is required */
		if (code >= FSM_CODE_MAX
		    || (ftyp->req_codes & (1 << code)) == 0) {
			if (code < FSM_CODE_MAX)
				fsm->rejcode[code] = 1;
			ppp_fsm_event(fsm, RXJ_P);
		} else {
			ppp_fsm_record(fsm, FSM_REASON_CODEREJ, code);
			ppp_fsm_event(fsm, RXJ_M);
			goto close;
		}
		break;
	    }

	case FSM_CODE_PROTOREJ:
	    {
		u_int16_t proto;

		/* Get rejected protocol */
		memcpy(&proto, payload, 2);
		proto = ntohs(proto);
		if (proto == ftyp->proto) {
			ppp_fsm_record(fsm, FSM_REASON_PROTOREJ, proto);
			ppp_fsm_event(fsm, RXJ_M);
			goto close;
		}
		ppp_fsm_output(fsm, FSM_OUTPUT_PROTOREJ, proto);
		ppp_fsm_event(fsm, RXJ_P);	/* assume RXJ+ until hear o/w */
		break;
	    }

	case FSM_CODE_ECHOREQ:
	    {
		u_char buf[MAX_PKTCOPY];
		u_int32_t magic;

		/* Update reply id */
		fsm->ids[FSM_CODE_ECHOREP] = hdr.id;

		/* Only reply when in opened state (except MP LCP) */
		if (fsm->state != FSM_STATE_OPENED) {
			LOG(LOG_DEBUG + 1, "ignoring: not in %s yet",
			    st2str(FSM_STATE_OPENED));
			goto done;
		}

		/* Insert my magic number and reply */
		memcpy(buf, payload, MIN(len, sizeof(buf)));
		magic = (ftyp->get_magic != NULL) ?
		    (*ftyp->get_magic)(fsm->inst, PPP_SELF) : 0;
		magic = htonl(magic);
		memcpy(buf, &magic, 4);
		ppp_fsm_send_packet(fsm, FSM_CODE_ECHOREP,
		    buf, MIN(len, sizeof(buf)));
		break;
	    }
	case FSM_CODE_RESETREQ:
		if (ftyp->recv_reset_req == NULL)
			goto code_reject;
		fsm->ids[FSM_CODE_RESETACK] = hdr.id;
		(*ftyp->recv_reset_req)(fsm->inst, payload, len);
		break;

	case FSM_CODE_RESETACK:
		if (ftyp->recv_reset_ack == NULL)
			goto code_reject;
		if (hdr.id != fsm->ids[FSM_CODE_RESETREQ])
			break;
		(*ftyp->recv_reset_ack)(fsm->inst, payload, len);
		break;

	case FSM_CODE_ECHOREP:	/* ignore these */
	case FSM_CODE_IDENT:
	case FSM_CODE_DISCREQ:
	case FSM_CODE_TIMEREM:
	    	break;

	default:		/* already handled above */
		break;
	}

	/* Done */
	goto done;

close:
	/* Handle failure by closing up shop */
	ppp_fsm_event(fsm, CLOSE);

done:
	/* Clean up */
	ppp_fsm_option_destroy(&opts);
}

/*
 * Handle an event
 */
static void
ppp_fsm_event(struct ppp_fsm *fsm, int event)
{
	const int ostate = fsm->state;
	const int action = fsm_actions[event][fsm->state];

	/* Debugging */
	if (action == NA) {
		LOG(LOG_ERR, "%s event %s in state %s",
		    "invalid", ev2str(event), st2str(fsm->state));
		return;
	}
	LOG(LOG_DEBUG, "event %s in state %s",
	    ev2str(event), st2str(fsm->state));

	/* Perform actions */
	if ((action & TLU) != 0)
		ppp_fsm_output(fsm, FSM_OUTPUT_UP);
	if ((action & TLD) != 0)
		ppp_fsm_output(fsm, FSM_OUTPUT_DOWN, fsm->dead.u.down.reason);
	if ((action & TLS) != 0)
		ppp_fsm_output(fsm, FSM_OUTPUT_OPEN);
	if ((action & TLF) != 0)
		ppp_fsm_output(fsm, FSM_OUTPUT_CLOSE);
	if ((action & IRC) != 0) {
		fsm->restart = (event == CLOSE || event == RXJ_M) ?
		    FSM_MAX_TERMINATE : FSM_MAX_CONFIGURE;
	}
	if ((action & SCR) != 0)
		ppp_fsm_send_config(fsm, FSM_CODE_CONFIGREQ);
	if ((action & SCA) != 0)
		ppp_fsm_send_config(fsm, FSM_CODE_CONFIGACK);
	if ((action & SCN) != 0) {
		if (fsm->config[FSM_CONF_REJ]->num != 0)
			ppp_fsm_send_config(fsm, FSM_CODE_CONFIGREJ);
		if (fsm->config[FSM_CONF_NAK]->num != 0)
			ppp_fsm_send_config(fsm, FSM_CODE_CONFIGNAK);
	}
	if ((action & STR) != 0)
		ppp_fsm_send_packet(fsm, FSM_CODE_TERMREQ, NULL, 0);
	if ((action & STA) != 0)
		ppp_fsm_send_packet(fsm, FSM_CODE_TERMACK, NULL, 0);

	/* Transition to new state */
	fsm->state = (action & STMASK);
	if (fsm->state != ostate)
		LOG(LOG_DEBUG, "%s -> %s", st2str(ostate), st2str(fsm->state));

	/* Initialize failure counter if appropriate */
	if (ostate < FSM_STATE_REQSENT && fsm->state >= FSM_STATE_REQSENT) {
		fsm->failure[PPP_SELF] = FSM_MAX_FAILURE;
		fsm->failure[PPP_PEER] = FSM_MAX_FAILURE;
	}

	/* Stop the restart timer if it's not supposed to be running */
	if (!FSM_TIMER_STATE(fsm->state)) {
		pevent_unregister(&fsm->timer);
		goto no_timer;
	}

	/* Check if timer is already running and doesn't need to be restarted */
	if (FSM_TIMER_STATE(ostate) && (action & (SCR|STR)) == 0)
		goto no_timer;

	/* (Re)start restart timer */
	pevent_unregister(&fsm->timer);
	if (pevent_register(fsm->ev_ctx, &fsm->timer, 0, fsm->mutex,
	    ppp_fsm_timeout, fsm, PEVENT_TIME, FSM_TIMEOUT * 1000) == -1) {
		ppp_fsm_syserr(fsm, "pevent_register");
		return;
	}

no_timer:
	/* Emit 'dead' output if we're dead */
	if (FSM_DEAD(fsm))
		ppp_fsm_output_dead(fsm);
}

/*
 * Send a config req, ack, nak, or rej packet
 */
static void
ppp_fsm_send_config(struct ppp_fsm *fsm, enum ppp_fsm_code code)
{
	const struct ppp_fsm_type *const ftyp = fsm->inst->type;
	struct ppp_fsm_options *opts;
	u_char *buf;
	u_int len;

	/* Get the corresponding config option data */
	switch (code) {
	case FSM_CODE_CONFIGREQ:
	    {
		u_int i;
		u_int j;

		/* Regenerate my requested config options */
		opts = fsm->config[FSM_CONF_SELF];
		ppp_fsm_option_zero(opts);
		if ((*ftyp->build_conf_req)(fsm->inst, opts) == -1) {
			if (errno == EINVAL)
				ppp_fsm_record(fsm, FSM_REASON_NEGOT);
			else
				ppp_fsm_syserr(fsm, "build_conf_req");
			return;
		}

		/* Elide default values from within config-request */
		if (fsm->inst->type->defaults == NULL)
			break;
		for (i = 0; i < fsm->inst->type->defaults->num; i++) {
			for (j = 0; j < opts->num; j++) {
				if (ppp_fsm_option_equal(
				    fsm->inst->type->defaults, i, opts, j))
					ppp_fsm_option_del(opts, j--);
			}
		}
		break;
	    }
	case FSM_CODE_CONFIGACK:
		opts = fsm->config[FSM_CONF_PEER];
		break;
	case FSM_CODE_CONFIGNAK:
		opts = fsm->config[FSM_CONF_NAK];
		break;
	case FSM_CODE_CONFIGREJ:
		opts = fsm->config[FSM_CONF_REJ];
		break;
	default:
		assert (0);
		return;
	}

	/* Construct packet payload */
	len = ppp_fsm_option_packlen(opts);
	if ((buf = MALLOC(TYPED_MEM_TEMP, len)) == NULL) {
		LOG(LOG_ERR, "%s: %m", "malloc");
		ppp_fsm_syserr(fsm, "malloc");
		return;
	}

	/* Send packet */
	ppp_fsm_option_pack(opts, buf);
	ppp_fsm_send_packet(fsm, code, buf, len);
	FREE(TYPED_MEM_TEMP, buf);
}

/*
 * Handler for an FSM timeout event.
 */
static void
ppp_fsm_timeout(void *arg)
{
	struct ppp_fsm *const fsm = arg;

	/* Cancel event */
	pevent_unregister(&fsm->timer);

	/* If we're dead, ignore it */
	if (FSM_DEAD(fsm))
		return;

	/* Send timeout event */
	if (--fsm->restart <= 0)
		ppp_fsm_event(fsm, TO_M);
	else
		ppp_fsm_event(fsm, TO_P);
}

/*
 * Send a packet.
 *
 * Handle any errors by shutting down the FSM.
 */
static void
ppp_fsm_send_packet(struct ppp_fsm *fsm,
	u_char code, const void *data, u_int len)
{
	struct ppp_fsm_pkt *pkt;

	/* If peer rejected code, don't bother */
	if (fsm->rejcode[code])
		return;

	/* Build packet */
	if ((pkt = MALLOC(FSM_MTYPE, sizeof(*pkt) + len)) == NULL) {
		ppp_fsm_syserr(fsm, "malloc");
		return;
	}
	pkt->code = code;
	pkt->id = fsm->ids[code];
	pkt->length = htons(sizeof(*pkt) + len);
	memcpy(pkt->data, data, len);

	/* Logging */
	ppp_fsm_log_pkt(fsm, (pkt->code == FSM_CODE_ECHOREQ
	    || pkt->code == FSM_CODE_ECHOREP) ? LOG_DEBUG : LOG_INFO,
	    "xmit", (u_char *)pkt);

	/* Send packet */
	if (ppp_fsm_output(fsm, FSM_OUTPUT_DATA, pkt, sizeof(*pkt) + len) == -1)
		FREE(FSM_MTYPE, pkt);
}

/*
 * Record the reason for the FSM going down or dying.
 *
 * This information is saved until it can be output later.
 */
static void
ppp_fsm_record(struct ppp_fsm *fsm, ...)
{
	struct ppp_fsm_output *const output = &fsm->dead;
	va_list args;

	/* Prioritize reason first on severity, then first come, first serve */
	if (fsm->dead.u.down.reason == 0)
		goto record;
	switch (fsm->dead.u.down.reason) {
	case FSM_REASON_CONF:
	case FSM_REASON_DOWN_NONFATAL:
		break;
	default:
		return;
	}

record:
	/* Build output message */
	va_start(args, fsm);
	ppp_fsm_output_build(output, FSM_OUTPUT_DEAD, args);
	va_end(args);
}

/*
 * Emit some sort of output from the FSM.
 *
 * Any errors are handled internally by shutting down the FSM.
 */
static int
ppp_fsm_output(struct ppp_fsm *fsm, enum ppp_fsmoutput type, ...)
{
	struct ppp_fsm_output *output;
	va_list args;

	/* Allocate new output message */
	if ((output = MALLOC(FSM_MTYPE, sizeof(*output))) == NULL) {
		ppp_fsm_syserr(fsm, "malloc");
		return (-1);
	}
	memset(output, 0, sizeof(*output));

	/* Build output message */
	va_start(args, type);
	ppp_fsm_output_build(output, type, args);
	va_end(args);

	/* Send it */
	if (mesg_port_put(fsm->outport, output) == -1) {
		ppp_fsm_syserr(fsm, "mesg_port_put");
		FREE(FSM_MTYPE, output);
		return (-1);
	}

	/* Done */
	return (0);
}

/*
 * Output FSM_OUTPUT_DEAD message because we're dead.
 *
 * This is called when we reach the initial state after a fatal error.
 * We copy the reason information previously recorded via ppp_fsm_record().
 */
static void
ppp_fsm_output_dead(struct ppp_fsm *fsm)
{
	struct ppp_fsm_output *output;

	/* Create new output message */
	if ((output = MALLOC(FSM_MTYPE, sizeof(*output))) == NULL) {
		ppp_fsm_syserr(fsm, "malloc");
		return;
	}
	memset(output, 0, sizeof(*output));

	/* Copy previously recorded output message, changing DOWN to DEAD */
	*output = fsm->dead;

	/* Send it */
	if (mesg_port_put(fsm->outport, output) == -1) {
		ppp_fsm_syserr(fsm, "mesg_port_put");
		FREE(FSM_MTYPE, output);
	}
}

/*
 * Build an FSM output structure.
 */
static void
ppp_fsm_output_build(struct ppp_fsm_output *output,
	enum ppp_fsmoutput type, va_list args)
{
	output->type = type;
	switch (type) {
	case FSM_OUTPUT_OPEN:
	case FSM_OUTPUT_CLOSE:
	case FSM_OUTPUT_UP:
		break;
	case FSM_OUTPUT_DOWN:
	case FSM_OUTPUT_DEAD:
		output->u.down.reason = va_arg(args, int);
		switch (output->u.down.reason) {
		case FSM_REASON_SYSERR:
			output->u.down.u.error = va_arg(args, int);
			break;
		case FSM_REASON_CONF:
		case FSM_REASON_CODEREJ:
			output->u.down.u.code = va_arg(args, int);
			break;
		case FSM_REASON_PROTOREJ:
			output->u.down.u.proto = va_arg(args, int);
			break;
		default:
			break;
		}
		break;
	case FSM_OUTPUT_DATA:
		output->u.data.data = va_arg(args, u_char *);
		output->u.data.length = va_arg(args, u_int);
		break;
	case FSM_OUTPUT_PROTOREJ:
		output->u.proto = va_arg(args, int);
		break;
	default:
		assert(0);
	}
}

/*
 * Handle system error
 */
static void
ppp_fsm_syserr(struct ppp_fsm *fsm, const char *func)
{
	LOG(LOG_ERR, "%s: %m", func);
	ppp_fsm_record(fsm, FSM_REASON_SYSERR, errno);
}

/*
 * Decode and log contents of an FSM packet
 */
static void
ppp_fsm_log_pkt(struct ppp_fsm *fsm, int sev,
	const char *prefix, const u_char *pkt)
{
	const u_char *const payload = pkt + sizeof(struct ppp_fsm_pkt);
	struct ppp_fsm_pkt hdr;
	char buf[512] = { '\0' };
	u_int16_t dlen;

	memcpy(&hdr, pkt, sizeof(hdr));
	dlen = ntohs(hdr.length) - sizeof(hdr);
	switch (hdr.code) {
	case FSM_CODE_CONFIGREQ:
	case FSM_CODE_CONFIGNAK:
	case FSM_CODE_CONFIGREJ:
	case FSM_CODE_CONFIGACK:
		ppp_fsm_options_decode(fsm->inst->type->options,
		    payload, dlen, buf, sizeof(buf));
		break;
	case FSM_CODE_CODEREJ:
		snprintf(buf, sizeof(buf), "code=%u", payload[0]);
		break;
	case FSM_CODE_PROTOREJ:
		snprintf(buf, sizeof(buf),
		    "proto=0x%02x%02x", payload[0], payload[1]);
		break;
	case FSM_CODE_IDENT:
	case FSM_CODE_TERMREQ:
	case FSM_CODE_TERMACK:
		if (dlen > 0) {
			strlcpy(buf, "msg=\"", sizeof(buf));
			ppp_util_ascify(buf + strlen(buf),
			    sizeof(buf) - strlen(buf), payload + 4, dlen - 4);
			strlcat(buf, "\"", sizeof(buf));
		}
		break;
	case FSM_CODE_TIMEREM:
	    {
		u_int32_t remain;
		char tbuf[32];

		memcpy(&remain, payload + 4, 4);
		remain = ntohl(remain);
		if (remain == ~0)
			strlcpy(tbuf, "unlimited", sizeof(tbuf));
		else {
			snprintf(tbuf, sizeof(tbuf),
			    "%lu seconds", (u_long)remain);
		}
		strlcpy(buf, "remaining=", sizeof(buf));
		strlcat(buf, tbuf, sizeof(buf));
		if (dlen > 4) {
			strlcat(buf, " msg=\"", sizeof(buf));
			ppp_util_ascify(buf + strlen(buf),
			    sizeof(buf) - strlen(buf), payload + 8, dlen - 8);
			strlcat(buf, "\"", sizeof(buf));
		}
		break;
	    }
	default:
		break;
	}

	/* Finally, log it */
	if (*buf == '\0')
		LOG(sev, "%s %s #%u", prefix, cd2str(hdr.code), hdr.id);
	else {
		LOG(sev, "%s %s #%u: %s", prefix,
		    cd2str(hdr.code), hdr.id, buf);
	}
}

static const char *
st2str(u_int state)
{
	static const char *const snames[FSM_STATE_MAX] = {
	    "INITIAL", "STARTING", "CLOSED", "STOPPED", "CLOSING",
	    "STOPPING", "REQ-SENT", "ACK-RCVD", "ACK-SENT", "OPENED"
	};
	static char buf[16];

	if (state >= sizeof(snames) / sizeof(*snames)) {
		snprintf(buf, sizeof(buf), "?[%u]", state);
		return (buf);
	}
	return (snames[state]);
}

static const char *
ev2str(u_int event)
{
	static const char *const enames[FSM_EVENT_MAX] = {
	    "UP", "DOWN", "OPEN", "CLOSE", "TO+", "TO-", "RCR+",
	    "RCR-", "RCA", "RCN", "RTR", "RTA", "RXJ+", "RXJ-"
	};
	static char buf[16];

	if (event >= sizeof(enames) / sizeof(*enames)) {
		snprintf(buf, sizeof(buf), "?[%u]", event);
		return (buf);
	}
	return (enames[event]);
}

static const char *
cd2str(u_int code)
{
	static const char *const cnames[FSM_CODE_MAX] = {
	    "Vendor", "Conf-Req", "Conf-Ack", "Conf-Nak", "Conf-Rej",
	    "Term-Req", "Term-Ack", "Code-Rej", "Proto-Rej", "Echo-Req",
	    "Echo-Rsp", "Disc-Req", "Ident", "Time-Rem", "Reset-Req",
	    "Reset-Ack"
	};
	static char buf[16];

	if (code >= sizeof(cnames) / sizeof(*cnames)) {
		snprintf(buf, sizeof(buf), "?[%u]", code);
		return (buf);
	}
	return (cnames[code]);
}

/***********************************************************************
		    PUBLIC DEBUGGING FUNCTIONS
***********************************************************************/

/*
 * Return a string describing FSM output.
 */
const char *
ppp_fsm_output_str(struct ppp_fsm_output *output)
{
	static char buf[256];

	switch (output->type) {
	case FSM_OUTPUT_OPEN:
		snprintf(buf, sizeof(buf), "OPEN");
		break;
	case FSM_OUTPUT_CLOSE:
		snprintf(buf, sizeof(buf), "CLOSE");
		break;
	case FSM_OUTPUT_UP:
		snprintf(buf, sizeof(buf), "UP");
		break;
	case FSM_OUTPUT_DOWN:
		snprintf(buf, sizeof(buf), "DOWN reason=%s",
		    ppp_fsm_reason_str(output));
		break;
	case FSM_OUTPUT_DATA:
		snprintf(buf, sizeof(buf), "DATA type=%s",
		    cd2str(output->u.data.data[0]));
		break;
	case FSM_OUTPUT_PROTOREJ:
		snprintf(buf, sizeof(buf), "PROTOREJ proto=0x%04x",
		    output->u.proto);
		break;
	case FSM_OUTPUT_DEAD:
		snprintf(buf, sizeof(buf), "DEAD reason=%s",
		    ppp_fsm_reason_str(output));
		break;
	default:
		snprintf(buf, sizeof(buf), "?[%u]?", output->type);
		break;
	}
	return (buf);
}

/*
 * Return a string describing a FSM_OUTPUT_DOWN or FSM_OUTPUT_DEAD output.
 */
const char *
ppp_fsm_reason_str(struct ppp_fsm_output *output)
{
	static char buf[64];

	/* Sanity check */
	if (output->type != FSM_OUTPUT_DEAD
	    && output->type != FSM_OUTPUT_DOWN)
		return ("?not FSM_OUTPUT_DOWN or FSM_OUTPUT_DEAD");

	/* Describe reason */
	switch (output->u.down.reason) {
	case FSM_REASON_CLOSE:
		strlcpy(buf, "administratively closed", sizeof(buf));
		break;
	case FSM_REASON_DOWN_FATAL:
		strlcpy(buf, "the underlying packet delivery"
		    " layer failed (fatal)", sizeof(buf));
		break;
	case FSM_REASON_DOWN_NONFATAL:
		strlcpy(buf, "the underlying packet delivery"
		    " layer failed (non-fatal)", sizeof(buf));
		break;
	case FSM_REASON_CONF:
		snprintf(buf, sizeof(buf),
		    "rec'd %s after reaching opened state",
		    cd2str(output->u.down.u.code));
		break;
	case FSM_REASON_TERM:
		snprintf(buf, sizeof(buf),
		    "rec'd a Terminate-Request from peer");
		break;
	case FSM_REASON_CODEREJ:
		snprintf(buf, sizeof(buf),
		    "rec'd a fatal Code-Reject (code=%s)",
		    cd2str(output->u.down.u.code));
		break;
	case FSM_REASON_PROTOREJ:
		snprintf(buf, sizeof(buf),
		    "rec'd a fatal Protocol-Reject (proto=0x%04x)",
		    output->u.down.u.proto);
		break;
	case FSM_REASON_NEGOT:
		strlcpy(buf, "local and remote configurations"
		    " are not compatible", sizeof(buf));
		break;
	case FSM_REASON_BADMAGIC:
		strlcpy(buf, "rec'd a packet with an invalid magic number",
		    sizeof(buf));
		break;
	case FSM_REASON_LOOPBACK:
		strlcpy(buf, "a loopback condition was detected", sizeof(buf));
		break;
	case FSM_REASON_TIMEOUT:
		strlcpy(buf, "timed out waiting for an echo response",
		    sizeof(buf));
		break;
	case FSM_REASON_SYSERR:
		snprintf(buf, sizeof(buf), "internal system error, error=%s",
		    strerror(output->u.down.u.error));
		break;
	default:
		snprintf(buf, sizeof(buf), "?[%u]", output->u.down.reason);
		break;
	}
	return (buf);
}

