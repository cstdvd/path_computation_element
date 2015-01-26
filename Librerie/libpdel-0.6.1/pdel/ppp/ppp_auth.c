
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_auth_pap.h"
#include "ppp/ppp_auth_chap.h"

/***********************************************************************
			AUTH TYPE FUNCTIONS
***********************************************************************/

/* Types of CHAP */
#define CHAP_PROTO_MD5		0x05
#define CHAP_PROTO_MSV1		0x80
#define CHAP_PROTO_MSV2		0x81

#define PRBYT(x)	((PPP_PROTO_ ## x) >> 8), ((PPP_PROTO_ ## x) & 0xff)

/* Keep consistent with "ppp_auth.h" */
static const	struct ppp_auth_type ppp_auth_types[PPP_AUTH_MAX] = {
    {
	"None",
	PPP_AUTH_NONE,
	NULL,
	NULL,
	0
    },
    {
	"PAP",
	PPP_AUTH_PAP,
	ppp_auth_pap_start,
	ppp_auth_pap_cancel,
	ppp_auth_pap_input,
	2, { PRBYT(PAP) }
    },
    {
	"CHAP-MSv1",
	PPP_AUTH_CHAP_MSV1,
	ppp_auth_chap_start,
	ppp_auth_chap_cancel,
	ppp_auth_chap_input,
	3, { PRBYT(CHAP), CHAP_PROTO_MSV1 }
    },
    {
	"CHAP-MSv2",
	PPP_AUTH_CHAP_MSV2,
	ppp_auth_chap_start,
	ppp_auth_chap_cancel,
	ppp_auth_chap_input,
	3, { PRBYT(CHAP), CHAP_PROTO_MSV2 }
    },
    {
	"CHAP-MD5",
	PPP_AUTH_CHAP_MD5,
	ppp_auth_chap_start,
	ppp_auth_chap_cancel,
	ppp_auth_chap_input,
	3, { PRBYT(CHAP), CHAP_PROTO_MD5 }
    },
};

/*
 * Find auth type descriptor by LCP option.
 */
const struct ppp_auth_type *
ppp_auth_by_option(const struct ppp_fsm_option *opt)
{
	int i;

	for (i = 1; i < PPP_AUTH_MAX; i++) {
		const struct ppp_auth_type *const auth = &ppp_auth_types[i];

		if (auth->index != PPP_AUTH_NONE	/* saftey first! */
		    && opt->len == auth->len
		    && memcmp(opt->data, auth->data, auth->len) == 0)
			return (auth);
	}
	return (NULL);
}

/*
 * Find auth type descriptor by index.
 */
const struct ppp_auth_type *
ppp_auth_by_index(enum ppp_auth_index index)
{
	if (index <= 0 || index > PPP_AUTH_MAX) {
		errno = EINVAL;
		return (NULL);
	}
	return (&ppp_auth_types[index]);
}

/*
 * Print out an auth type option.
 */
void
ppp_auth_print(const struct ppp_fsm_optdesc *desc,
	const struct ppp_fsm_option *opt, char *buf, size_t bmax)
{
	const struct ppp_auth_type *const auth = ppp_auth_by_option(opt);

	if (auth != NULL) {
		snprintf(buf, bmax, "%s", auth->name);
		return;
	}
	if (opt->len < 2) {
		snprintf(buf, bmax, "<truncated>");
		return;
	}
	snprintf(buf, bmax, "?0x%02x%02x", opt->data[0], opt->data[1]);
}

