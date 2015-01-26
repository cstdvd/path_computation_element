
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
#include "ppp/ppp_node.h"
#include "ppp/ppp_ccp.h"

#include <netgraph/ng_mppc.h>

#ifndef MPPE_56
#define	MPPE_56		0x00000080
#undef MPPE_BITS
#define	MPPE_BITS	0x000000e0
#endif

/*
 * CCP FSM
 *
 * XXX This is hard coded to only support MPPC/MPPE compression.
 * XXX It should be generalized to support multiple PPP compression types.
 */

#define MPPC_SUPPORTED		(MPPC_BIT | MPPE_BITS | MPPE_STATELESS)

/* Memory type */
#define CCP_MTYPE		"ccp"

/* CCP compression types */
enum ccp_type {
	CCP_OPT_OUI		=0,	/* oui */
	CCP_OPT_PRED1		=1,	/* predictor type 1 */
	CCP_OPT_PRED2		=2,	/* predictor type 2 */
	CCP_OPT_PUDDLE		=3,	/* puddle jumper */
	CCP_OPT_HWPPC		=16,	/* hewlett-packard ppc */
	CCP_OPT_STAC		=17,	/* stac electronics lzs */
	CCP_OPT_MPPC		=18,	/* microsoft mppc/mppe */
	CCP_OPT_GAND		=19,	/* gandalf fza */
	CCP_OPT_V42BIS		=20,	/* v.42bis compression */
	CCP_OPT_BSD		=21,	/* bsd lzw compress */
	CCP_OPT_DEFLATE		=24,	/* gzip "deflate" compression */
};

/* Supported and required FSM codes */
#define CCP_SUPPORTED_CODES		\
	  (1 << FSM_CODE_CONFIGREQ)	\
	| (1 << FSM_CODE_CONFIGACK)	\
	| (1 << FSM_CODE_CONFIGNAK)	\
	| (1 << FSM_CODE_CONFIGREJ)	\
	| (1 << FSM_CODE_TERMREQ)	\
	| (1 << FSM_CODE_TERMACK)	\
	| (1 << FSM_CODE_CODEREJ)	\
	| (1 << FSM_CODE_RESETREQ)	\
	| (1 << FSM_CODE_RESETACK)
#define CCP_REQUIRED_CODES		\
	  (1 << FSM_CODE_CONFIGREQ)	\
	| (1 << FSM_CODE_CONFIGACK)	\
	| (1 << FSM_CODE_CONFIGNAK)	\
	| (1 << FSM_CODE_CONFIGREJ)	\
	| (1 << FSM_CODE_TERMREQ)	\
	| (1 << FSM_CODE_TERMACK)	\
	| (1 << FSM_CODE_CODEREJ)	\
	| (1 << FSM_CODE_RESETREQ)	\
	| (1 << FSM_CODE_RESETACK)

/* FSM options descriptors */
static opt_pr_t	ppp_cpp_pr_mppc;

static const	struct ppp_fsm_optdesc ccp_opt_desc[] = {
	{ "OUI",	CCP_OPT_OUI,	0, 255,	0,	NULL },
	{ "Predictor-1",CCP_OPT_PRED1,	0, 255,	0,	NULL },
	{ "Predictor-2",CCP_OPT_PRED2,	0, 255,	0,	NULL },
	{ "Puddle",	CCP_OPT_PUDDLE,	0, 255,	0,	NULL },
	{ "HWPPC",	CCP_OPT_HWPPC,	0, 255,	0,	NULL },
	{ "Stac",	CCP_OPT_STAC,	0, 255,	0,	NULL },
	{ "MPPC/MPPE",	CCP_OPT_MPPC,	4, 4,	1,	ppp_cpp_pr_mppc },
	{ "Gandalf",	CCP_OPT_GAND,	0, 255,	0,	NULL },
	{ "v.42bis",	CCP_OPT_V42BIS,	0, 255,	0,	NULL },
	{ "BSD-LZW",	CCP_OPT_BSD,	0, 255,	0,	NULL },
	{ "Deflate",	CCP_OPT_DEFLATE,0, 255,	0,	NULL },
	{ NULL,		0,		0, 0,	0,	NULL }
};

/* FSM type for CCP */
static ppp_fsm_type_destroy_t		ppp_ccp_destroy;
static ppp_fsm_type_build_conf_req_t	ppp_ccp_build_conf_req;
static ppp_fsm_type_recv_conf_req_t	ppp_ccp_recv_conf_req;
static ppp_fsm_type_recv_conf_rej_t	ppp_ccp_recv_conf_rej;
static ppp_fsm_type_recv_conf_nak_t	ppp_ccp_recv_conf_nak;
static ppp_fsm_type_recv_reset_req_t	ppp_ccp_recv_reset_req;
static ppp_fsm_type_recv_reset_ack_t	ppp_ccp_recv_reset_ack;

const struct	ppp_fsm_type ppp_fsm_ccp = {
	"CCP",
	PPP_PROTO_CCP,
	CCP_SUPPORTED_CODES,
	CCP_REQUIRED_CODES,
	ccp_opt_desc,
	NULL,
	ppp_ccp_destroy,
	ppp_ccp_build_conf_req,
	ppp_ccp_recv_conf_req,
	ppp_ccp_recv_conf_rej,
	ppp_ccp_recv_conf_nak,
	NULL,
	ppp_ccp_recv_reset_req,
	ppp_ccp_recv_reset_ack,
	NULL
};

/* CCP instance state */
struct ccp {
	struct ppp_fsm_instance	*inst;		/* backpointer to instance */
	struct ppp_ccp_config	conf;		/* initial config */
	struct ppp_ccp_req	req;		/* current request state */
	struct ppp_node		*node;		/* ng_ppp(4) node */
};

/* Internal functions */
static ppp_node_recvmsg_t	ppp_ccp_send_reset_req;

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

struct ppp_fsm_instance *
ppp_ccp_create(struct ppp_ccp_config *conf, struct ppp_node *node)
{
	struct ppp_fsm_instance *inst;
	struct ppp_ccp_req *req;
	struct ccp *ccp = NULL;

	/* Construct instance object */
	if ((inst = MALLOC(CCP_MTYPE, sizeof(*inst))) == NULL)
		return (NULL);
	memset(inst, 0, sizeof(*inst));
	inst->type = &ppp_fsm_ccp;

	/* Attach private data */
	if ((ccp = MALLOC(CCP_MTYPE, sizeof(*ccp))) == NULL)
		goto fail;
	memset(ccp, 0, sizeof(*ccp));
	ccp->conf = *conf;
	ccp->node = node;
	ccp->inst = inst;
	inst->arg = ccp;

	/* Initialize local request state */
	req = &ccp->req;
	req->mppc[PPP_SELF] = conf->mppc[PPP_SELF];
	req->mppe40[PPP_SELF] = conf->mppe40[PPP_SELF];
	req->mppe56[PPP_SELF] = conf->mppe56[PPP_SELF];
	req->mppe128[PPP_SELF] = conf->mppe128[PPP_SELF];
	req->mppe_stateless[PPP_SELF] = conf->mppe_stateless[PPP_SELF];

	/* Register handler for reset-req control message from ng_mppc(4) */
	if (ppp_node_set_recvmsg(ccp->node, NGM_MPPC_COOKIE,
	    NGM_MPPC_RESETREQ, ppp_ccp_send_reset_req, ccp) == -1)
		goto fail;

	/* Done */
	return (inst);

fail:
	/* Clean up after failure */
	if (ccp != NULL)
		FREE(CCP_MTYPE, ccp);
	FREE(CCP_MTYPE, inst);
	return (NULL);
}

/*
 * Get CCP request state.
 */
void
ppp_ccp_get_req(struct ppp_fsm *fsm, struct ppp_ccp_req *req)
{
	struct ppp_fsm_instance *const inst = ppp_fsm_get_instance(fsm);
	struct ccp *const ccp = inst->arg;

	assert(inst->type == &ppp_fsm_ccp);
	memcpy(req, &ccp->req, sizeof(*req));
}

/***********************************************************************
			FSM CALLBACKS
***********************************************************************/

static void
ppp_ccp_destroy(struct ppp_fsm_instance *inst)
{
	struct ccp *const ccp = inst->arg;

	ppp_node_set_recvmsg(ccp->node,
	    NGM_MPPC_COOKIE, NGM_MPPC_RESETREQ, NULL, NULL);
	FREE(CCP_MTYPE, ccp);
	FREE(CCP_MTYPE, inst);
}

static int
ppp_ccp_build_conf_req(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *opts)
{
	struct ccp *const ccp = (struct ccp *)fsm->arg;
	struct ppp_ccp_req *const req = &ccp->req;
	u_int32_t mppo = 0;

	/* Add MPPC/MPPE requested config options */
	if (req->mppc[PPP_SELF])
		mppo |= MPPC_BIT;
	if (req->mppe40[PPP_SELF])
		mppo |= MPPE_40;
	if (req->mppe56[PPP_SELF])
		mppo |= MPPE_56;
	if (req->mppe128[PPP_SELF])
		mppo |= MPPE_128;
	if (req->mppe_stateless[PPP_SELF])
		mppo |= MPPE_STATELESS;

	/* If no options left to try, don't ask for anything */
	if ((mppo & (MPPC_BIT | MPPE_BITS)) == 0) {
		errno = EINVAL;
		return (-1);
	}

	/* Add option */
	mppo = htonl(mppo);
	if (ppp_fsm_option_add(opts, CCP_OPT_MPPC, 4, &mppo) == -1)
		return (-1);

	/* Done */
	return (0);
}

static int
ppp_ccp_recv_conf_req(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *crq, struct ppp_fsm_options *nak,
	struct ppp_fsm_options *rej)
{
	struct ccp *const ccp = (struct ccp *)fsm->arg;
	struct ppp_ccp_config *const conf = &ccp->conf;
	struct ppp_ccp_req *const req = &ccp->req;
	int i;

	/* Initialize peer's request state */
	req->mppc[PPP_PEER] = 0;
	req->mppe40[PPP_PEER] = 0;
	req->mppe56[PPP_PEER] = 0;
	req->mppe128[PPP_PEER] = 0;
	req->mppe_stateless[PPP_PEER] = 0;

	/* Process options */
	for (i = 0; i < crq->num; i++) {
		const struct ppp_fsm_option *const opt = &crq->opts[i];

		switch (opt->type) {
		case CCP_OPT_MPPC:
		    {
			u_int32_t obits;
			u_int32_t bits;

			/* Get requested bits */
			memcpy(&obits, opt->data, 4);
			obits = ntohl(obits);
			bits = obits;

			/* Filter out bits we can't handle */
			bits &= MPPC_SUPPORTED;
			if ((bits & MPPC_BIT) != 0 && !conf->mppc[PPP_PEER])
				bits &= ~MPPC_BIT;
			if ((bits & MPPE_40) != 0 && !conf->mppe40[PPP_PEER])
				bits &= ~MPPE_40;
			if ((bits & MPPE_56) != 0 && !conf->mppe56[PPP_PEER])
				bits &= ~MPPE_56;
			if ((bits & MPPE_128) != 0 && !conf->mppe128[PPP_PEER])
				bits &= ~MPPE_128;
			if ((bits & MPPE_STATELESS) != 0
			    && !conf->mppe_stateless[PPP_PEER])
				bits &= ~MPPE_STATELESS;

			/*
			 * It doesn't really make sense to do MPPE encryption
			 * in only one direction. Also, Win95/98 PPTP can't
			 * handle uni-directional encryption. So if the remote
			 * side doesn't request encryption, try to prompt it.
			 * This is broken wrt. normal PPP negotiation: typical
			 * Microsoft.
			 */
			if ((bits & MPPE_BITS) == 0) {
				if (req->mppe40[PPP_SELF])
					bits |= MPPE_40;
				if (req->mppe56[PPP_SELF])
					bits |= MPPE_56;
				if (req->mppe128[PPP_SELF])
					bits |= MPPE_128;
			}

			/* Make sure we're not left with no options */
			if ((bits & MPPE_BITS) == 0) {
				if (ccp->conf.mppe40[PPP_SELF])
					bits |= MPPE_40;
				if (ccp->conf.mppe56[PPP_SELF])
					bits |= MPPE_56;
				if (ccp->conf.mppe128[PPP_SELF])
					bits |= MPPE_128;
			}

			/* Now choose the strongest encryption available */
			if ((bits & MPPE_128) != 0)
				bits &= ~(MPPE_56|MPPE_40);
			else if ((bits & MPPE_56) != 0)
				bits &= ~(MPPE_128|MPPE_40);
			else if ((bits & MPPE_40) != 0)
				bits &= ~(MPPE_128|MPPE_56);

			/* Send back a nak if there were any changes */
			if (bits != obits) {
				bits = htonl(bits);
				if (ppp_fsm_option_add(nak,
				    opt->type, 4, &bits) == -1)
					return (-1);
				break;
			}

			/* Peer's request is accepted */
			req->mppc[PPP_PEER] = (bits & MPPC_BIT) != 0;
			req->mppe40[PPP_PEER] = (bits & MPPE_40) != 0;
			req->mppe56[PPP_PEER] = (bits & MPPE_56) != 0;
			req->mppe128[PPP_PEER] = (bits & MPPE_128) != 0;
			req->mppe_stateless[PPP_PEER]
			    = (bits & MPPE_STATELESS) != 0;
			break;
		    }
		default:
			goto reject;
		}

		/* OK */
		continue;

reject:
		/* Reject this requested option */
		if (ppp_fsm_option_add(rej,
		    opt->type, opt->len, opt->data) == -1)
			return (-1);
	}

	/* If there were no options, shut down */
	if (crq->num == 0) {
		errno = EINVAL;
		return (-1);
	}

	/* Done */
	return (0);
}

static int
ppp_ccp_recv_conf_rej(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *rej)
{
	int i;

	for (i = 0; i < rej->num; i++) {
		const struct ppp_fsm_option *const opt = &rej->opts[i];

		switch (opt->type) {
		case CCP_OPT_MPPC:
			errno = EINVAL;
			return (-1);
		default:
			break;
		}
	}

	/* Done */
	return (0);
}

static int
ppp_ccp_recv_conf_nak(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *nak)
{
	struct ccp *const ccp = (struct ccp *)fsm->arg;
	struct ppp_ccp_req *const req = &ccp->req;
	int i;

	for (i = 0; i < nak->num; i++) {
		const struct ppp_fsm_option *const opt = &nak->opts[i];

		switch (opt->type) {
		case CCP_OPT_MPPC:
		    {
			u_int32_t bits;

			memcpy(&bits, opt->data, 4);
			bits = ntohl(bits);

			/* Mask away bits the client didn't like */
			if ((bits & MPPC_BIT) == 0)
				req->mppc[PPP_SELF] = 0;
			if ((bits & MPPE_40) == 0)
				req->mppe40[PPP_SELF] = 0;
			if ((bits & MPPE_56) == 0)
				req->mppe56[PPP_SELF] = 0;
			if ((bits & MPPE_128) == 0)
				req->mppe128[PPP_SELF] = 0;
			if ((bits & MPPE_STATELESS) == 0)
				req->mppe_stateless[PPP_SELF] = 0;

			/* Make sure we're not left with no options */
			if ((bits & MPPE_BITS) == 0) {
				if (ccp->conf.mppe40[PPP_SELF])
					bits |= MPPE_40;
				if (ccp->conf.mppe56[PPP_SELF])
					bits |= MPPE_56;
				if (ccp->conf.mppe128[PPP_SELF])
					bits |= MPPE_128;
			}
			break;
		    }
		default:
			break;
		}
	}

	/* Done */
	return (0);
}

/*
 * Receive a Reset-Req from peer. Relay request to the ng_mppc(4) node.
 */
static void
ppp_ccp_recv_reset_req(struct ppp_fsm_instance *fsm,
	const u_char *data, u_int len)
{
	struct ccp *const ccp = (struct ccp *)fsm->arg;

	ppp_node_send_msg(ccp->node, NG_PPP_HOOK_COMPRESS,
	    NGM_MPPC_COOKIE, NGM_MPPC_RESETREQ, NULL, 0);
}

/*
 * Receive a Reset-Ack.
 */
static void
ppp_ccp_recv_reset_ack(struct ppp_fsm_instance *fsm,
	const u_char *data, u_int len)
{
	return;			/* not used by MPPC/MPPE, just ignore it */
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

static void
ppp_ccp_send_reset_req(void *arg, struct ng_mesg *msg)
{
	struct ccp *const ccp = arg;
	struct ppp_fsm *const fsm = ccp->inst->fsm;

	ppp_fsm_send_reset_req(fsm, NULL, 0);
}

static void
ppp_cpp_pr_mppc(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	static const struct {
		u_int32_t	bit;
		const char	*name;
	} mppe_bits[] = {
		{ MPPC_BIT,		"MPPC" },
		{ MPPE_BITS,		"MPPE" },
		{ MPPE_40,		"40 bit" },
		{ MPPE_56,		"56 bit" },
		{ MPPE_128,		"128 bit" },
		{ MPPE_STATELESS,	"stateless" },
		{ 0 }
	};
	u_int32_t mppo;
	int first;
	int i;

	if (opt->len < 4) {
		snprintf(buf, bmax, "<truncated>");
		return;
	}
	memcpy(&mppo, opt->data, 4);
	mppo = ntohl(mppo);
	*buf = '\0';
	for (first = 1, i = 0; mppe_bits[i].bit != 0; i++) {
		if ((mppo & mppe_bits[i].bit) != 0) {
			snprintf(buf + strlen(buf), bmax - strlen(buf),
			    "%s%s", first ? "" : ", ", mppe_bits[i].name);
			first = 0;
		}
	}
}

