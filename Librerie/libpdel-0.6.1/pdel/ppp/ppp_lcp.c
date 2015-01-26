
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_fsm.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_lcp.h"
#include "ppp/ppp_util.h"

/* Memory type */
#define LCP_MTYPE		"lcp"

/* LCP configuration options */
enum lcp_option {
	LCP_OPT_VENDOR		=0,	/* Vendor specific */
	LCP_OPT_MRU		=1,	/* Maximum-Receive-Unit */
	LCP_OPT_ACCMAP		=2,	/* Async-Control-Character-Map */
	LCP_OPT_AUTH		=3,	/* Authentication-Protocol */
	LCP_OPT_QUAL		=4,	/* Quality-Protocol */
	LCP_OPT_MAGIC		=5,	/* Magic-Number */
	LCP_OPT_PFCMP		=7,	/* Protocol-Field-Compression */
	LCP_OPT_ACFCMP		=8,	/* Address&Ctrl-Field-Compression */
	LCP_OPT_FCSALT		=9,	/* FCS-Alternatives */
	LCP_OPT_SDP		=10,	/* Self-Dscribing-Padding */
	LCP_OPT_NBMODE		=11,	/* Numbered-Mode */
	LCP_OPT_MULTILINK	=12,	/* Multi-link procedure (?) */
	LCP_OPT_CALLBK		=13,	/* Callback */
	LCP_OPT_CTTIME		=14,	/* Connect time */
	LCP_OPT_COMPFR		=15,	/* Compound-Frames */
	LCP_OPT_NDS		=16,	/* Nominal-Data-Encapsulation */
	LCP_OPT_MRRU		=17,	/* Multi-link MRRU size */
	LCP_OPT_SHSEQ		=18,	/* Short seq number header */
	LCP_OPT_EDISC		=19,	/* Unique endpoint discrimiator */
	LCP_OPT_PROPR		=20,	/* Proprietary */
	LCP_OPT_DCEID		=21	/* DCE-Identifier */
};

/* Supported and required FSM codes for normal LCP */
#define LCP_SUPPORTED_CODES		\
	  (1 << FSM_CODE_CONFIGREQ)	\
	| (1 << FSM_CODE_CONFIGACK)	\
	| (1 << FSM_CODE_CONFIGNAK)	\
	| (1 << FSM_CODE_CONFIGREJ)	\
	| (1 << FSM_CODE_TERMREQ)	\
	| (1 << FSM_CODE_TERMACK)	\
	| (1 << FSM_CODE_CODEREJ)	\
	| (1 << FSM_CODE_PROTOREJ)	\
	| (1 << FSM_CODE_ECHOREQ)	\
	| (1 << FSM_CODE_ECHOREP)	\
	| (1 << FSM_CODE_DISCREQ)	\
	| (1 << FSM_CODE_IDENT)		\
	| (1 << FSM_CODE_TIMEREM)
#define LCP_REQUIRED_CODES		\
	  (1 << FSM_CODE_CONFIGREQ)	\
	| (1 << FSM_CODE_CONFIGACK)	\
	| (1 << FSM_CODE_CONFIGNAK)	\
	| (1 << FSM_CODE_CONFIGREJ)	\
	| (1 << FSM_CODE_TERMREQ)	\
	| (1 << FSM_CODE_TERMACK)	\
	| (1 << FSM_CODE_CODEREJ)	\
	| (1 << FSM_CODE_PROTOREJ)	\
	| (1 << FSM_CODE_ECHOREQ)	\
	| (1 << FSM_CODE_ECHOREP)

/* Supported and required FSM codes for LCP sent over a multilink bundle */
#define MP_LCP_SUPPORTED_CODES		\
	  (1 << FSM_CODE_CODEREJ)	\
	| (1 << FSM_CODE_PROTOREJ)	\
	| (1 << FSM_CODE_ECHOREQ)	\
	| (1 << FSM_CODE_ECHOREP)	\
	| (1 << FSM_CODE_DISCREQ)	\
	| (1 << FSM_CODE_IDENT)		\
	| (1 << FSM_CODE_TIMEREM)
#define MP_LCP_REQUIRED_CODES		\
	  (1 << FSM_CODE_CODEREJ)	\
	| (1 << FSM_CODE_PROTOREJ)	\
	| (1 << FSM_CODE_ECHOREQ)	\
	| (1 << FSM_CODE_ECHOREP)

struct eid_type {
	u_char		min;
	u_char		max;
	const char	*desc;
};

static const	struct eid_type eid_types[PPP_EID_CLASS_MAX] = {
	{ 0,	0,		"Null"	},	/* PPP_EID_CLASS_NULL */
	{ 1,	PPP_EID_MAXLEN,	"Local"	},	/* PPP_EID_CLASS_LOCAL */
	{ 4,	4,		"IP"	},	/* PPP_EID_CLASS_IP */
	{ 6,	6,		"MAC"	},	/* PPP_EID_CLASS_MAC */
	{ 4,	PPP_EID_MAXLEN,	"Magic"	},	/* PPP_EID_CLASS_MAGIC */
	{ 1,	PPP_EID_MAXLEN,	"E.164"	},	/* PPP_EID_CLASS_E164 */
};

static opt_pr_t	lcp_pr_eid;

/* FSM options descriptors */
const struct	ppp_fsm_optdesc lcp_opt_desc[] = {
	{ "Vendor",	LCP_OPT_VENDOR,	4, 255,	0,	NULL },
	{ "MRU",	LCP_OPT_MRU,	2, 2,	1,	ppp_fsm_pr_int16 },
	{ "ACCM",	LCP_OPT_ACCMAP,	4, 4,	1,	ppp_fsm_pr_hex32 },
	{ "Auth",	LCP_OPT_AUTH,	2, 255,	1,	ppp_auth_print},
	{ "Qual",	LCP_OPT_QUAL,	0, 255,	0,	NULL },
	{ "Magic",	LCP_OPT_MAGIC,	4, 4,	1,	ppp_fsm_pr_hex32 },
	{ "PFComp",	LCP_OPT_PFCMP,	0, 0,	1,	NULL },
	{ "ACFComp",	LCP_OPT_ACFCMP,	0, 0,	1,	NULL },
	{ "FCSAlt",	LCP_OPT_FCSALT,	1, 1,	0,	NULL },
	{ "SDP",	LCP_OPT_SDP,	1, 1,	0,	NULL },
	{ "NumMode",	LCP_OPT_NBMODE,	0, 255,	0,	NULL },
	{ "Callback",	LCP_OPT_CALLBK,	1, 255,	0,	NULL },
	{ "CnctTime",	LCP_OPT_CTTIME,	0, 255,	0,	NULL },
	{ "CompFrames",	LCP_OPT_COMPFR,	0, 255,	0,	NULL },
	{ "NDEncap",	LCP_OPT_NDS,	0, 255,	0,	NULL },
	{ "MP-MRRU",	LCP_OPT_MRRU,	2, 2,	1,	ppp_fsm_pr_int16 },
	{ "MP-ShortSq",	LCP_OPT_SHSEQ,	0, 0,	1,	NULL },
	{ "EID",	LCP_OPT_EDISC,	1,
				1 + PPP_EID_MAXLEN, 1,	lcp_pr_eid },
	{ "Proprietry",	LCP_OPT_PROPR,	0, 255,	0,	NULL },
	{ "DCE-Ident",	LCP_OPT_DCEID,	0, 255,	0,	NULL },
	{ NULL,		0,		0, 0,	0,	NULL }
};

/* Default configuration options */
static /*const*/ u_char lcp_default_mru[2] = {
	(LCP_DEFAULT_MRU >> 8), (LCP_DEFAULT_MRU & 0xff)
};
static /*const*/ u_char lcp_default_accm[4] = {
	0xff, 0xff, 0xff, 0xff
};
static /*const*/ u_char lcp_default_eid[1] = {
	PPP_EID_CLASS_NULL
};

/* Default configuration option list */
static /*const*/ struct ppp_fsm_option lcp_opt_default_list[] = {
	{ LCP_OPT_MRU,		sizeof(lcp_default_mru), lcp_default_mru },
	{ LCP_OPT_ACCMAP,	sizeof(lcp_default_accm), lcp_default_accm },
	{ LCP_OPT_EDISC,	sizeof(lcp_default_eid), lcp_default_eid },
};
static const	struct ppp_fsm_options lcp_opt_default = {
	sizeof(lcp_opt_default_list) / sizeof(*lcp_opt_default_list),
	lcp_opt_default_list
};

/* FSM type for LCP */
static ppp_fsm_type_destroy_t		ppp_lcp_destroy;
static ppp_fsm_type_build_conf_req_t	ppp_lcp_build_conf_req;
static ppp_fsm_type_recv_conf_req_t	ppp_lcp_recv_conf_req;
static ppp_fsm_type_recv_conf_rej_t	ppp_lcp_recv_conf_rej;
static ppp_fsm_type_recv_conf_nak_t	ppp_lcp_recv_conf_nak;
static ppp_fsm_type_get_magic_t		ppp_lcp_get_magic;

const struct	ppp_fsm_type ppp_fsm_lcp = {
	"LCP",
	PPP_PROTO_LCP,
	LCP_SUPPORTED_CODES,
	LCP_REQUIRED_CODES,
	lcp_opt_desc,
	&lcp_opt_default,
	ppp_lcp_destroy,
	ppp_lcp_build_conf_req,
	ppp_lcp_recv_conf_req,
	ppp_lcp_recv_conf_rej,
	ppp_lcp_recv_conf_nak,
	ppp_lcp_get_magic,
	NULL,
	NULL,
	NULL
};

const struct	ppp_fsm_type ppp_fsm_mp_lcp = {
	"LCP",
	PPP_PROTO_LCP,
	MP_LCP_SUPPORTED_CODES,
	MP_LCP_REQUIRED_CODES,
	NULL,
	NULL,
	ppp_lcp_destroy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/* LCP instance state */
struct lcp {
	struct ppp_lcp_config	conf;		/* initial config */
	struct ppp_lcp_req	req;		/* current request state */
};

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

struct ppp_fsm_instance *
ppp_lcp_create(struct ppp_lcp_config *conf)
{
	struct ppp_fsm_instance *inst;
	struct ppp_lcp_req *req;
	struct lcp *lcp = NULL;
	int nauth[2];
	int i;

	/* Construct instance object */
	if ((inst = MALLOC(LCP_MTYPE, sizeof(*inst))) == NULL)
		return (NULL);
	memset(inst, 0, sizeof(*inst));
	inst->type = (conf != NULL) ? &ppp_fsm_lcp : &ppp_fsm_mp_lcp;

	/* Attach private data */
	if ((lcp = MALLOC(LCP_MTYPE, sizeof(*lcp))) == NULL)
		goto fail;
	memset(lcp, 0, sizeof(*lcp));
	inst->arg = lcp;

	/* No configuration required for MP LCP's */
	if (conf == NULL)
		goto no_conf;

	/* Sanity check and normalize configuration */
	lcp->conf = *conf;
	conf = &lcp->conf;
	for (i = 0; i < 2; i++) {
		if (conf->max_mru[i] == 0)
			conf->max_mru[i] = LCP_MAX_MRU;
		if (conf->max_mrru[i] == 0)
			conf->max_mrru[i] = LCP_MAX_MRRU;
		conf->min_mru[i] = MAX(conf->min_mru[i], LCP_MIN_MRU);
		conf->max_mru[i] = MIN(conf->max_mru[i], LCP_MAX_MRU);
		conf->min_mrru[i] = MAX(conf->min_mrru[i], LCP_MIN_MRRU);
		conf->max_mrru[i] = MIN(conf->max_mrru[i], LCP_MAX_MRRU);
	}

	/* Multilink cannot be both enabled and denied */
	if (!conf->multilink[PPP_PEER]) {
		conf->shortseq[PPP_SELF] = 0;
		conf->shortseq[PPP_PEER] = 0;
		if (conf->multilink[PPP_SELF]) {
			errno = EINVAL;
			goto fail;
		}
	}

	/* Check MRU values are self-consistent */
	if (conf->min_mru[PPP_SELF] > conf->max_mru[PPP_SELF]
	    || conf->min_mru[PPP_PEER] > conf->max_mru[PPP_PEER]
	    || conf->min_mrru[PPP_SELF] > conf->max_mrru[PPP_SELF]
	    || conf->min_mrru[PPP_PEER] > conf->max_mrru[PPP_PEER]) {
		errno = EINVAL;
		goto fail;
	}

	/* Check MRRU values are self-consistent (if multilink is possible) */
	if (conf->multilink[PPP_PEER]
	    && (conf->min_mrru[PPP_SELF] > conf->max_mrru[PPP_SELF]
	      || conf->min_mrru[PPP_PEER] > conf->max_mrru[PPP_PEER])) {
		errno = EINVAL;
		goto fail;
	}

	/* Sanity check EID */
	if (conf->eid.class >= PPP_EID_CLASS_MAX
	    || conf->eid.length < eid_types[conf->eid.class].min
	    || conf->eid.length > eid_types[conf->eid.class].max) {
		errno = EINVAL;
		goto fail;
	}

	/*
	 * At least one type of authentication (including "none")
	 * must be enabled in each direction.
	 */
	nauth[PPP_SELF] = 0;
	nauth[PPP_PEER] = 0;
	for (i = 0; i < PPP_AUTH_MAX; i++) {
		if (conf->auth[PPP_SELF][i] != 0)
			nauth[PPP_SELF]++;
		if (conf->auth[PPP_PEER][i] != 0)
			nauth[PPP_PEER]++;
	}
	if (nauth[PPP_SELF] == 0 || nauth[PPP_PEER] == 0) {
		errno = EINVAL;
		goto fail;
	}

	/* Initialize local request state */
	req = &lcp->req;
	req->mru[PPP_SELF] = conf->max_mru[PPP_SELF];
	req->accm[PPP_SELF] = conf->accm;
	req->magic[PPP_SELF] = random() ^ time(NULL) ^ (getpid() << 16);
	req->acfcomp[PPP_SELF] = conf->acfcomp[PPP_SELF];
	req->pfcomp[PPP_SELF] = conf->pfcomp[PPP_SELF];
	for (i = PPP_AUTH_MAX - 1; i >= 0; i--) {
		if (conf->auth[PPP_SELF][i] != 0) {
			req->auth[PPP_SELF] = i;
			break;
		}
	}
	req->multilink[PPP_SELF] = conf->multilink[PPP_SELF];
	req->mrru[PPP_SELF] = conf->max_mrru[PPP_SELF];
	req->shortseq[PPP_SELF] = conf->shortseq[PPP_SELF];
	req->eid[PPP_SELF] = conf->eid;

no_conf:
	/* Done */
	return (inst);

fail:
	/* Clean up after failure */
	if (lcp != NULL)
		FREE(LCP_MTYPE, lcp);
	FREE(LCP_MTYPE, inst);
	return (NULL);
}

/*
 * Get LCP request state.
 */
void
ppp_lcp_get_req(struct ppp_fsm *fsm, struct ppp_lcp_req *req)
{
	struct ppp_fsm_instance *const inst = ppp_fsm_get_instance(fsm);
	struct lcp *const lcp = inst->arg;

	assert(inst->type == &ppp_fsm_lcp
	    || inst->type == &ppp_fsm_mp_lcp);
	memcpy(req, &lcp->req, sizeof(*req));
}

/***********************************************************************
			FSM CALLBACKS
***********************************************************************/

static void
ppp_lcp_destroy(struct ppp_fsm_instance *inst)
{
	struct lcp *const lcp = inst->arg;

	FREE(LCP_MTYPE, lcp);
	FREE(LCP_MTYPE, inst);
}

static int
ppp_lcp_build_conf_req(struct ppp_fsm_instance *fsm,
	struct ppp_fsm_options *opts)
{
	struct lcp *const lcp = (struct lcp *)fsm->arg;
	struct ppp_lcp_req *const req = &lcp->req;
	u_int16_t val16;
	u_int32_t val32;

	/* Do MRU, ACCM, ACF compression, PF compression, and magic # */
	val16 = htons(req->mru[PPP_SELF]);
	if (ppp_fsm_option_add(opts, LCP_OPT_MRU, 2, &val16) == -1)
		return (-1);
	val32 = htonl(req->accm[PPP_SELF]);
	if (ppp_fsm_option_add(opts, LCP_OPT_ACCMAP, 4, &val32) == -1)
		return (-1);
	if (req->acfcomp[PPP_SELF]
	    && ppp_fsm_option_add(opts, LCP_OPT_ACFCMP, 0, NULL) == -1)
		return (-1);
	if (req->pfcomp[PPP_SELF]
	    && ppp_fsm_option_add(opts, LCP_OPT_PFCMP, 0, NULL) == -1)
		return (-1);
	val32 = htonl(req->magic[PPP_SELF]);
	if (ppp_fsm_option_add(opts, LCP_OPT_MAGIC, 4, &val32) == -1)
		return (-1);
	if (req->auth[PPP_SELF] != PPP_AUTH_NONE) {
		const struct ppp_auth_type *const a
		    = ppp_auth_by_index(req->auth[PPP_SELF]);

		if (ppp_fsm_option_add(opts,
		    LCP_OPT_AUTH, a->len, a->data) == -1)
			return (-1);
	}

	/* Do multi-link stuff */
	if (req->multilink[PPP_SELF]) {
		val16 = htons(req->mrru[PPP_SELF]);
		if (ppp_fsm_option_add(opts, LCP_OPT_MRRU, 2, &val16) == -1)
			return (-1);
		if (req->shortseq[PPP_SELF]
		    && ppp_fsm_option_add(opts, LCP_OPT_SHSEQ, 0, NULL) == -1)
			return (-1);
	}

	/* Do endpoint descriminator */
	if (req->eid[PPP_SELF].class != PPP_EID_CLASS_NULL) {
		u_char eid_buf[1 + PPP_EID_MAXLEN];

		eid_buf[0] = req->eid[PPP_SELF].class;
		memcpy(eid_buf + 1,
		    req->eid[PPP_SELF].value, req->eid[PPP_SELF].length);
		if (ppp_fsm_option_add(opts, LCP_OPT_EDISC,
		    1 + req->eid[PPP_SELF].length, eid_buf) == -1)
			return (-1);
	}

	/* Done */
	return (0);
}

static int
ppp_lcp_recv_conf_req(struct ppp_fsm_instance *fsm, struct ppp_fsm_options *crq,
	struct ppp_fsm_options *nak, struct ppp_fsm_options *rej)
{
	struct lcp *const lcp = (struct lcp *)fsm->arg;
	struct ppp_lcp_config *const conf = &lcp->conf;
	struct ppp_lcp_req *const req = &lcp->req;
	int i;

	/* Initialize peer's request state */
	req->mru[PPP_PEER] = LCP_DEFAULT_MRU;
	req->accm[PPP_PEER] = ~0;
	req->acfcomp[PPP_PEER] = 0;
	req->pfcomp[PPP_PEER] = 0;
	req->magic[PPP_PEER] = 0;
	req->auth[PPP_PEER] = PPP_AUTH_NONE;
	req->mrru[PPP_PEER] = LCP_DEFAULT_MRRU;
	req->multilink[PPP_PEER] = 0;
	req->shortseq[PPP_PEER] = 0;
	req->eid[PPP_PEER].class = PPP_EID_CLASS_NULL;
	req->eid[PPP_PEER].length = 0;

	/* Process options */
	for (i = 0; i < crq->num; i++) {
		const struct ppp_fsm_option *const opt = &crq->opts[i];

		switch (opt->type) {
		case LCP_OPT_MRU:
		    {
			u_int16_t mru;

			memcpy(&mru, opt->data, 2);
			mru = ntohs(mru);
			if (mru < conf->min_mru[PPP_PEER]) {
				mru = htons(conf->min_mru[PPP_PEER]);
				if (ppp_fsm_option_add(nak, opt->type,
				    sizeof(mru), &mru) == -1)
					return (-1);
				break;
			}
			if (mru > conf->max_mru[PPP_PEER]) {
				mru = htons(conf->max_mru[PPP_PEER]);
				if (ppp_fsm_option_add(nak, opt->type,
				    sizeof(mru), &mru) == -1)
					return (-1);
				break;
			}
			req->mru[PPP_PEER] = mru;
			break;
		    }
		case LCP_OPT_ACCMAP:
		    {
			memcpy(&req->accm[PPP_PEER], opt->data, 4);
			req->accm[PPP_PEER] = ntohl(req->accm[PPP_PEER]);
			break;
		    }
		case LCP_OPT_PFCMP:
		    {
			if (!conf->pfcomp[PPP_PEER])
				goto reject;
			req->pfcomp[PPP_PEER] = 1;
			break;
		    }
		case LCP_OPT_ACFCMP:
		    {
			if (!conf->acfcomp[PPP_PEER])
				goto reject;
			req->acfcomp[PPP_PEER] = 1;
			break;
		    }
		case LCP_OPT_MAGIC:
		    {
			u_int32_t magic;

			memcpy(&magic, opt->data, 4);
			magic = ntohl(magic);
			if (magic == req->magic[PPP_SELF]) {
				errno = ELOOP;		/* indicate loopback */
				return (-1);
			}
			req->magic[PPP_PEER] = magic;
			break;
		    }
		case LCP_OPT_AUTH:
		    {
			const struct ppp_auth_type *a = ppp_auth_by_option(opt);
			int i;

			/* Sanity check */
			if (a == NULL)
				break;

			/* Check if we accept peer's auth proposal */
			if (conf->auth[PPP_PEER][a->index] != 0) {
				req->auth[PPP_PEER] = a->index;
				break;
			}

			/* See what other type we can do and nak with it */
			for (i = 1; i < PPP_AUTH_MAX; i++) {
				if (conf->auth[PPP_PEER][i] != 0) {
					a = ppp_auth_by_index(i);
					if (ppp_fsm_option_add(nak, opt->type,
					    a->len, a->data) == -1)
						return (-1);
					break;
				}
			}

			/* No auth type is acceptable */
			goto reject;
		    }
		case LCP_OPT_MRRU:
		    {
			u_int16_t mrru;

			if (!conf->multilink[PPP_PEER])
				goto reject;
			memcpy(&mrru, opt->data, 2);
			mrru = ntohs(mrru);
			if (mrru < conf->min_mrru[PPP_PEER]) {
				mrru = htons(conf->min_mrru[PPP_PEER]);
				if (ppp_fsm_option_add(nak,
				    opt->type, 2, &mrru) == -1)
					return (-1);
				break;
			}
			if (mrru > conf->max_mrru[PPP_PEER]) {
				mrru = htons(conf->max_mrru[PPP_PEER]);
				if (ppp_fsm_option_add(nak,
				    opt->type, 2, &mrru) == -1)
					return (-1);
				break;
			}
			req->multilink[PPP_PEER] = 1;
			req->multilink[PPP_SELF] = 1;
			req->mrru[PPP_PEER] = mrru;
			break;
		    }
		case LCP_OPT_SHSEQ:
		    {
			if (!conf->multilink[PPP_PEER]
			    || !conf->shortseq[PPP_PEER])
				goto reject;
			req->shortseq[PPP_PEER] = 1;
			break;
		    }
		case LCP_OPT_EDISC:
		    {
			struct ppp_eid eid;

			if (opt->len < 1)
				goto reject;
			eid.class = opt->data[0];
			eid.length = opt->len - 1;
			if (eid.class >= PPP_EID_CLASS_MAX
			    || eid.length < eid_types[eid.class].min
			    || eid.length > eid_types[eid.class].max)
				goto reject;
			memcpy(eid.value, opt->data + 1, eid.length);
			req->eid[PPP_PEER] = eid;
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

	/* If no auth requested, but we require auth, nak for it */
	if (req->auth[PPP_PEER] == PPP_AUTH_NONE
	    && !conf->auth[PPP_PEER][PPP_AUTH_NONE]) {
		for (i = 1; i < PPP_AUTH_MAX; i++) {
			if (conf->auth[PPP_PEER][i] != 0) {
				const struct ppp_auth_type *const a
				    = ppp_auth_by_index(i);

				if (ppp_fsm_option_add(nak,
				    LCP_OPT_AUTH, a->len, a->data) == -1)
					return (-1);
			}
		}
	}

	/* Do same for multilink? */

	/* Done */
	return (0);
}

static int
ppp_lcp_recv_conf_rej(struct ppp_fsm_instance *fsm, struct ppp_fsm_options *rej)
{
	struct lcp *const lcp = (struct lcp *)fsm->arg;
	struct ppp_lcp_config *const conf = &lcp->conf;
	struct ppp_lcp_req *const req = &lcp->req;
	int i;

	for (i = 0; i < rej->num; i++) {
		const struct ppp_fsm_option *const opt = &rej->opts[i];

		switch (opt->type) {
		case LCP_OPT_MRU:
			req->mru[PPP_SELF] = LCP_DEFAULT_MRU;
			break;
		case LCP_OPT_ACCMAP:
			req->accm[PPP_SELF] = ~0;
			break;
		case LCP_OPT_PFCMP:
			req->pfcomp[PPP_SELF] = 0;
			break;
		case LCP_OPT_ACFCMP:
			req->acfcomp[PPP_SELF] = 0;
			break;
		case LCP_OPT_MAGIC:
			req->magic[PPP_SELF] = 0;
			break;
		case LCP_OPT_AUTH:
		    {
			const struct ppp_auth_type *a = ppp_auth_by_option(opt);
			int i;

			/* Sanity check */
			if (a == NULL)
				break;

			/* Mark this auth type as rejected by peer */
			req->auth_rej[a->index] = 1;

			/* Find next acceptable type not rejected by peer */
			for (i = 0; i < PPP_AUTH_MAX; i++) {
				if (conf->auth[PPP_SELF][i]
				    && !req->auth_rej[i]) {
					req->auth[PPP_SELF] = i;
					break;
				}
			}

			/* None found? Can't continue */
			if (i == PPP_AUTH_MAX) {
				errno = EINVAL;
				return (-1);
			}
			break;
		    }
		case LCP_OPT_MRRU:
			req->multilink[PPP_SELF] = 0;
			/* fall through */
		case LCP_OPT_SHSEQ:
			req->shortseq[PPP_SELF] = 0;
			break;
		case LCP_OPT_EDISC:
			req->eid[PPP_SELF].class = PPP_EID_CLASS_NULL;
			req->eid[PPP_SELF].length = 0;
			break;
		default:
			break;
		}
	}

	/* Done */
	return (0);
}

static int
ppp_lcp_recv_conf_nak(struct ppp_fsm_instance *fsm, struct ppp_fsm_options *nak)
{
	struct lcp *const lcp = (struct lcp *)fsm->arg;
	struct ppp_lcp_config *const conf = &lcp->conf;
	struct ppp_lcp_req *const req = &lcp->req;
	int i;

	for (i = 0; i < nak->num; i++) {
		const struct ppp_fsm_option *const opt = &nak->opts[i];

		switch (opt->type) {
		case LCP_OPT_MRU:
		    {
			u_int16_t mru;

			memcpy(&mru, opt->data, 2);
			mru = ntohs(mru);
			if (mru < conf->min_mru[PPP_SELF])
				mru = conf->min_mru[PPP_SELF];
			if (mru > conf->max_mru[PPP_SELF])
				mru = conf->max_mru[PPP_SELF];
			req->mru[PPP_SELF] = mru;
			break;
		    }
		case LCP_OPT_ACCMAP:
		    {
			memcpy(&req->accm[PPP_SELF], opt->data, 4);
			req->accm[PPP_SELF] = ntohl(req->accm[PPP_SELF]);
			break;
		    }
		case LCP_OPT_PFCMP:
		    {
			if (conf->pfcomp[PPP_SELF])
				req->pfcomp[PPP_SELF] = 1;
			break;
		    }
		case LCP_OPT_ACFCMP:
		    {
			if (conf->acfcomp[PPP_SELF])
				req->acfcomp[PPP_SELF] = 1;
			break;
		    }
		case LCP_OPT_AUTH:
		    {
			const struct ppp_auth_type *a = ppp_auth_by_option(opt);

			if (a != NULL && conf->auth[PPP_SELF][a->index])
				req->auth[PPP_SELF] = a->index;
			break;
		    }
		case LCP_OPT_MRRU:
		    {
			u_int16_t mrru;

			memcpy(&mrru, opt->data, 2);
			mrru = ntohs(mrru);
			if (mrru < conf->min_mrru[PPP_SELF])
				mrru = conf->min_mrru[PPP_SELF];
			if (mrru > conf->max_mrru[PPP_SELF])
				mrru = conf->max_mrru[PPP_SELF];
			req->mrru[PPP_SELF] = mrru;
			break;
		    }
		case LCP_OPT_SHSEQ:
		    {
			if (conf->shortseq[PPP_SELF])
				req->shortseq[PPP_SELF] = 1;
			break;
		    }
		default:
			break;
		}
	}

	/* Done */
	return (0);
}

static u_int32_t
ppp_lcp_get_magic(struct ppp_fsm_instance *fsm, int dir)
{
	struct lcp *const lcp = (struct lcp *)fsm->arg;
	struct ppp_lcp_req *const req = &lcp->req;

	return (req->magic[dir]);
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

static void
lcp_pr_eid(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	const struct eid_type *type;
	struct ppp_eid eid;
	int i;

	if (opt->len < 1) {
		strlcpy(buf, "<truncated>", bmax);
		return;
	}
	eid.class = opt->data[0];
	if (eid.class >= PPP_EID_CLASS_MAX) {
		snprintf(buf, bmax, "<invalid class %u>", eid.class);
		return;
	}
	type = &eid_types[eid.class];
	eid.length = opt->len - 1;
	if (eid.length < type->min || eid.length > type->max) {
		snprintf(buf, bmax,
		    "[%s] <invalid length %d>", type->desc, eid.length);
		return;
	}
	memcpy(eid.value, opt->data + 1, eid.length);
	snprintf(buf, bmax, "%s:", type->desc);
	switch (eid.class) {
	case PPP_EID_CLASS_NULL:
		break;
	case PPP_EID_CLASS_IP:
	    {
		struct in_addr ip;

		memcpy(&ip, eid.value, sizeof(ip));
		strlcpy(buf + strlen(buf), inet_ntoa(ip), bmax - strlen(buf));
		break;
	    }
	case PPP_EID_CLASS_LOCAL:
		for (i = 0; i < eid.length
		    && (isprint(eid.value[i]) || isspace(eid.value[i])); i++);
		if (i == eid.length) {
			ppp_util_ascify(buf + strlen(buf),
			    bmax - strlen(buf), eid.value, eid.length);
			break;
		}
		/* FALL THROUGH */
	default:
		for (i = 0; i < eid.length; i++) {
			snprintf(buf + strlen(buf), bmax - strlen(buf),
			    "%s%02x", (i > 0) ? ":" : "", eid.value[i]);
		}
		break;
	}
}

