
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
#include "ppp/ppp_ipcp.h"

/* Whether to do VJC compressed CID's */
#define IPCP_ALLOW_SELF_COMPCID	0	/* whether we allow to recv */
#define IPCP_ALLOW_PEER_COMPCID	1	/* whether we allow to send */

/* Memory type */
#define IPCP_MTYPE		"ipcp"

/* IPCP configuration options */
enum ipcp_option {
	IPCP_OPT_IP		=3,	/* ip address */
	IPCP_OPT_COMP		=2,	/* compression */
	IPCP_OPT_DNS1		=129,	/* primary dns */
	IPCP_OPT_DNS2		=131,	/* secondary dns */
	IPCP_OPT_NBNS1		=130,	/* primary nbns */
	IPCP_OPT_NBNS2		=132,	/* secondary nbns */
};

static const u_char ipcp_dns_opts[2] = { IPCP_OPT_DNS1, IPCP_OPT_DNS2 };
static const u_char ipcp_nbns_opts[2] = { IPCP_OPT_NBNS1, IPCP_OPT_NBNS2 };

/* Supported and required FSM codes */
#define IPCP_SUPPORTED_CODES		\
	  (1 << FSM_CODE_CONFIGREQ)	\
	| (1 << FSM_CODE_CONFIGACK)	\
	| (1 << FSM_CODE_CONFIGNAK)	\
	| (1 << FSM_CODE_CONFIGREJ)	\
	| (1 << FSM_CODE_TERMREQ)	\
	| (1 << FSM_CODE_TERMACK)	\
	| (1 << FSM_CODE_CODEREJ)
#define IPCP_REQUIRED_CODES		\
	  (1 << FSM_CODE_CONFIGREQ)	\
	| (1 << FSM_CODE_CONFIGACK)	\
	| (1 << FSM_CODE_CONFIGNAK)	\
	| (1 << FSM_CODE_CONFIGREJ)	\
	| (1 << FSM_CODE_TERMREQ)	\
	| (1 << FSM_CODE_TERMACK)	\
	| (1 << FSM_CODE_CODEREJ)

/* FSM options descriptors */
static opt_pr_t	ppp_ipcp_pr_ip;
static opt_pr_t	ppp_ipcp_pr_ipcomp;

static const	struct ppp_fsm_optdesc ipcp_opt_desc[] = {
	{ "IP-Addr",	IPCP_OPT_IP,	4, 4,	1,	ppp_ipcp_pr_ip },
	{ "IP-Comp",	IPCP_OPT_COMP,	4, 4,	1,	ppp_ipcp_pr_ipcomp },
	{ "DNS1",	IPCP_OPT_DNS1,	4, 4,	1,	ppp_ipcp_pr_ip },
	{ "DNS2",	IPCP_OPT_DNS2,	4, 4,	1,	ppp_ipcp_pr_ip },
	{ "NBNS1",	IPCP_OPT_NBNS1,	4, 4,	1,	ppp_ipcp_pr_ip },
	{ "NBNS2",	IPCP_OPT_NBNS2,	4, 4,	1,	ppp_ipcp_pr_ip },
	{ NULL,		0,		0, 0,	0,	NULL }
};

/* FSM type for IPCP */
static ppp_fsm_type_destroy_t		ppp_ipcp_destroy;
static ppp_fsm_type_build_conf_req_t	ppp_ipcp_build_conf_req;
static ppp_fsm_type_recv_conf_req_t	ppp_ipcp_recv_conf_req;
static ppp_fsm_type_recv_conf_rej_t	ppp_ipcp_recv_conf_rej;
static ppp_fsm_type_recv_conf_nak_t	ppp_ipcp_recv_conf_nak;

const struct	ppp_fsm_type ppp_fsm_ipcp = {
	"IPCP",
	PPP_PROTO_IPCP,
	IPCP_SUPPORTED_CODES,
	IPCP_REQUIRED_CODES,
	ipcp_opt_desc,
	NULL,
	ppp_ipcp_destroy,
	ppp_ipcp_build_conf_req,
	ppp_ipcp_recv_conf_req,
	ppp_ipcp_recv_conf_rej,
	ppp_ipcp_recv_conf_nak,
	NULL,
	NULL,
	NULL,
	NULL
};

/* IPCP instance state */
struct ipcp {
	struct ppp_ipcp_config	conf;		/* initial config */
	struct ppp_ipcp_req	req;		/* current request state */
	struct ppp_node		*node;		/* ng_ppp(4) node */
};

/* VJC compression header */
struct ipcp_vjc {
	u_int16_t	proto;
	u_char		maxchan;
	u_char		compcid;
};

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

struct ppp_fsm_instance *
ppp_ipcp_create(struct ppp_ipcp_config *conf, struct ppp_node *node)
{
	struct ppp_fsm_instance *inst;
	struct ppp_ipcp_req *req;
	struct ipcp *ipcp = NULL;

	/* Construct instance object */
	if ((inst = MALLOC(IPCP_MTYPE, sizeof(*inst))) == NULL)
		return (NULL);
	memset(inst, 0, sizeof(*inst));
	inst->type = &ppp_fsm_ipcp;

	/* Attach private data */
	if ((ipcp = MALLOC(IPCP_MTYPE, sizeof(*ipcp))) == NULL)
		goto fail;
	memset(ipcp, 0, sizeof(*ipcp));
	ipcp->conf = *conf;
	ipcp->node = node;
	inst->arg = ipcp;

	/* Initialize local request state */
	req = &ipcp->req;
	req->ip[PPP_SELF] = conf->ip[PPP_SELF];
	req->vjc[PPP_SELF].enabled = 1;
	req->vjc[PPP_SELF].maxchan = NG_VJC_MAX_CHANNELS - 1;
	req->vjc[PPP_SELF].compcid = IPCP_ALLOW_SELF_COMPCID;
	req->ask_dns = conf->do_dns[PPP_SELF];
	req->ask_nbns = conf->do_nbns[PPP_SELF];

	/* Done */
	return (inst);

fail:
	/* Clean up after failure */
	if (ipcp != NULL)
		FREE(IPCP_MTYPE, ipcp);
	FREE(IPCP_MTYPE, inst);
	return (NULL);
}

/*
 * Get IPCP request state.
 */
void
ppp_ipcp_get_req(struct ppp_fsm *fsm, struct ppp_ipcp_req *req)
{
	struct ppp_fsm_instance *const inst = ppp_fsm_get_instance(fsm);
	struct ipcp *const ipcp = inst->arg;

	assert(inst->type == &ppp_fsm_ipcp);
	memcpy(req, &ipcp->req, sizeof(*req));
}

/***********************************************************************
			FSM CALLBACKS
***********************************************************************/

static void
ppp_ipcp_destroy(struct ppp_fsm_instance *inst)
{
	struct ipcp *const ipcp = inst->arg;

	FREE(IPCP_MTYPE, ipcp);
	FREE(IPCP_MTYPE, inst);
}

static int
ppp_ipcp_build_conf_req(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *opts)
{
	struct ipcp *const ipcp = (struct ipcp *)fsm->arg;
	struct ppp_ipcp_req *const req = &ipcp->req;
	int i;

	/* Add requested config options */
	if (ppp_fsm_option_add(opts, IPCP_OPT_IP, 4, &req->ip[PPP_SELF]) == -1)
		return (-1);
	if (req->vjc[PPP_SELF].enabled) {
		struct ipcp_vjc vjc;

		vjc.proto = htons(PPP_PROTO_VJCOMP);
		vjc.maxchan = req->vjc[PPP_SELF].maxchan;
		vjc.compcid = req->vjc[PPP_SELF].compcid;
		if (ppp_fsm_option_add(opts,
		    IPCP_OPT_COMP, sizeof(vjc), &vjc) == -1)
			return (-1);
	}
	if (req->ask_dns) {
		for (i = 0; i < 2; i++) {
			if (ppp_fsm_option_add(opts,
			    ipcp_dns_opts[i], 4, &req->dns[i]) == -1)
				return (-1);
		}
	}
	if (req->ask_nbns) {
		for (i = 0; i < 2; i++) {
			if (ppp_fsm_option_add(opts,
			    ipcp_nbns_opts[i], 4, &req->nbns[i]) == -1)
				return (-1);
		}
	}

	/* Done */
	return (0);
}

static int
ppp_ipcp_recv_conf_req(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *crq, struct ppp_fsm_options *nak,
	struct ppp_fsm_options *rej)
{
	struct ipcp *const ipcp = (struct ipcp *)fsm->arg;
	struct ppp_ipcp_config *const conf = &ipcp->conf;
	struct ppp_ipcp_req *const req = &ipcp->req;
	int saw_ip = 0;
	int i;

	/* Initialize peer's request state */
	req->ip[PPP_PEER].s_addr = 0;
	memset(&req->vjc[PPP_PEER], 0, sizeof(req->vjc[PPP_PEER]));

	/* Process options */
	for (i = 0; i < crq->num; i++) {
		const struct ppp_fsm_option *const opt = &crq->opts[i];

		switch (opt->type) {
		case IPCP_OPT_IP:
		    {
			struct in_addr ip;

			saw_ip = 1;
			memcpy(&ip, opt->data, 4);
			if ((ip.s_addr & conf->mask[PPP_PEER].s_addr)
			    != (conf->ip[PPP_PEER].s_addr
			      & conf->mask[PPP_PEER].s_addr)) {
				if (ppp_fsm_option_add(nak, opt->type,
				    4, &conf->ip[PPP_PEER]) == -1)
					return (-1);
				break;
			}
			req->ip[PPP_PEER] = ip;
			break;
		    }
		case IPCP_OPT_COMP:
		    {
			struct ipcp_vjc vjc;
			int nakit = 0;

			memcpy(&vjc, opt->data, sizeof(vjc));
			if (ntohs(vjc.proto) != PPP_PROTO_VJCOMP)
				goto reject;
			if (vjc.maxchan < NG_VJC_MIN_CHANNELS - 1) {
				vjc.maxchan = NG_VJC_MIN_CHANNELS - 1;
				nakit = 1;
			}
			if (vjc.maxchan > NG_VJC_MAX_CHANNELS - 1) {
				vjc.maxchan = NG_VJC_MAX_CHANNELS - 1;
				nakit = 1;
			}
#if !IPCP_ALLOW_PEER_COMPCID
			if (vjc.compcid) {
				vjc.compcid = 0;
				nakit = 1;
			}
#endif
			if (nakit) {
				if (ppp_fsm_option_add(nak,
				    opt->type, sizeof(vjc), &vjc) == -1)
					return (-1);
				break;
			}
			req->vjc[PPP_PEER].enabled = 1;
			req->vjc[PPP_PEER].maxchan = vjc.maxchan;
			req->vjc[PPP_PEER].compcid = vjc.compcid;
			break;
		    }
		case IPCP_OPT_DNS1:
		case IPCP_OPT_DNS2:
		    {
			const int i = (opt->type == IPCP_OPT_DNS2);
			struct in_addr ip;

			if (!conf->do_dns[PPP_PEER]
			    || conf->dns[i].s_addr == 0)
				goto reject;
			memcpy(&ip, opt->data, 4);
			if (ip.s_addr != conf->dns[i].s_addr) {
				if (ppp_fsm_option_add(nak, opt->type,
				    4, &conf->dns[i]) == -1)
					return (-1);
				break;
			}
			break;
		    }
		case IPCP_OPT_NBNS1:
		case IPCP_OPT_NBNS2:
		    {
			const int i = (opt->type == IPCP_OPT_NBNS2);
			struct in_addr ip;

			if (!conf->do_nbns[PPP_PEER]
			    || conf->nbns[i].s_addr == 0)
				goto reject;
			memcpy(&ip, opt->data, 4);
			if (ip.s_addr != conf->nbns[i].s_addr) {
				if (ppp_fsm_option_add(nak, opt->type,
				    4, &conf->nbns[i]) == -1)
					return (-1);
				break;
			}
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

	/* Make sure we saw an IP address option */
	if (!saw_ip) {
		errno = EINVAL;
		return (-1);
	}

	/* Done */
	return (0);
}

static int
ppp_ipcp_recv_conf_rej(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *rej)
{
	struct ipcp *const ipcp = (struct ipcp *)fsm->arg;
	struct ppp_ipcp_req *const req = &ipcp->req;
	int i;

	for (i = 0; i < rej->num; i++) {
		const struct ppp_fsm_option *const opt = &rej->opts[i];

		switch (opt->type) {
		case IPCP_OPT_IP:
			errno = EINVAL;
			return (-1);
		case IPCP_OPT_COMP:
			req->vjc[PPP_SELF].enabled = 0;
			break;
		case IPCP_OPT_DNS1:
		case IPCP_OPT_DNS2:
			req->ask_dns = 0;
			break;
		case IPCP_OPT_NBNS1:
		case IPCP_OPT_NBNS2:
			req->ask_nbns = 0;
			break;
		default:
			break;
		}
	}

	/* Done */
	return (0);
}

static int
ppp_ipcp_recv_conf_nak(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *nak)
{
	struct ipcp *const ipcp = (struct ipcp *)fsm->arg;
	struct ppp_ipcp_config *const conf = &ipcp->conf;
	struct ppp_ipcp_req *const req = &ipcp->req;
	int i;

	for (i = 0; i < nak->num; i++) {
		const struct ppp_fsm_option *const opt = &nak->opts[i];

		switch (opt->type) {
		case IPCP_OPT_IP:
		    {
			struct in_addr ip;

			memcpy(&ip, opt->data, 4);
			if ((ip.s_addr & conf->mask[PPP_SELF].s_addr)
			    != (conf->ip[PPP_SELF].s_addr
			      & conf->mask[PPP_SELF].s_addr))
				break;
			req->ip[PPP_SELF] = ip;
			break;
		    }
		case IPCP_OPT_COMP:
		    {
			struct ipcp_vjc vjc;

			memcpy(&vjc, opt->data, sizeof(vjc));
			if (ntohs(vjc.proto) != PPP_PROTO_VJCOMP)
				break;
			if (vjc.maxchan < NG_VJC_MIN_CHANNELS - 1)
				vjc.maxchan = NG_VJC_MIN_CHANNELS - 1;
			if (vjc.maxchan > NG_VJC_MAX_CHANNELS - 1)
				vjc.maxchan = NG_VJC_MAX_CHANNELS - 1;
			req->vjc[PPP_SELF].maxchan = vjc.maxchan;
#if IPCP_ALLOW_SELF_COMPCID
			if (vjc.compcid)
				req->vjc[PPP_SELF].compcid = 1;
#endif
			break;
		    }
		case IPCP_OPT_DNS1:
		case IPCP_OPT_DNS2:
		    {
			const int i = (opt->type == IPCP_OPT_DNS2);

			memcpy(&req->dns[i], opt->data, 4);
			break;
		    }
		case IPCP_OPT_NBNS1:
		case IPCP_OPT_NBNS2:
		    {
			const int i = (opt->type == IPCP_OPT_NBNS2);

			memcpy(&req->nbns[i], opt->data, 4);
			break;
		    }
		default:
			break;
		}
	}

	/* Done */
	return (0);
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

static void
ppp_ipcp_pr_ip(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	struct in_addr ip;

	if (opt->len < 4) {
		snprintf(buf, bmax, "<truncated>");
		return;
	}
	memcpy(&ip, opt->data, 4);
	snprintf(buf, bmax, "%s", inet_ntoa(ip));
}

static void
ppp_ipcp_pr_ipcomp(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	struct ipcp_vjc vjc;

	if (opt->len < 2) {
		snprintf(buf, bmax, "<truncated>");
		return;
	}
	memcpy(&vjc, opt->data, 2);
	if (ntohs(vjc.proto) != PPP_PROTO_VJCOMP) {
		snprintf(buf, bmax, "?0x%04x", ntohs(vjc.proto));
		return;
	}
	if (opt->len < sizeof(vjc)) {
		snprintf(buf, bmax, "VJCOMP %s", "<truncated>");
		return;
	}
	memcpy(&vjc, opt->data, sizeof(vjc));
	snprintf(buf, bmax, "VJCOMP %d channels, %s comp-cid",
	    vjc.maxchan + 1, vjc.compcid ? "allow" : "no");
}

