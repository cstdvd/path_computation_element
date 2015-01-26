
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_util.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_auth_chap.h"
#include "ppp/ppp_msoft.h"

static ppp_auth_chap_hash_t	ppp_auth_chap_msv1_hash;
static ppp_auth_chap_equal_t	ppp_auth_chap_msv1_equal;
static ppp_auth_chap_final_t	ppp_auth_chap_msv1_final;

/* MS-CHAP version 1 descriptor */
const struct ppp_auth_chap_type ppp_auth_chap_msv1 = {
	NULL,
	ppp_auth_chap_msv1_hash,
	ppp_auth_chap_msv1_equal,
	ppp_auth_chap_msv1_final,
	1,
	PPP_MSOFTV1_CHAL_LEN,
	0,
	PPP_MSOFT_RESP_LEN
};

/* MS-CHAP error codes */
const struct mschap_err ppp_mschap_errs[] = {
	{ 646,	"Restricted logon hours" },
	{ 647,	"Account disabled" },
	{ 648,	"Password expired" },
	{ 649,	"No dialin permission" },
	{ 691,	"Authentication failure" },
	{ 709,	"Changing password" },
	{ 0,	NULL }
};

static void
ppp_auth_chap_msv1_hash(struct ppp_auth_cred_chap *chap,
	const void *secret, size_t slen)
{
	struct ppp_auth_cred_chap_msv1 *const msv1 = &chap->u.msv1;

	memset(&msv1->lm_hash, 0, sizeof(msv1->lm_hash));
	ppp_msoft_nt_challenge_response(chap->chal_data, secret, msv1->nt_hash);
	msv1->use_nt = 1;
}

static int
ppp_auth_chap_msv1_equal(struct ppp_auth_cred_chap *chap1,
	struct ppp_auth_cred_chap *chap2)
{
	struct ppp_auth_cred_chap_msv1 *const msv1_1 = &chap1->u.msv1;
	struct ppp_auth_cred_chap_msv1 *const msv1_2 = &chap2->u.msv1;

	if (msv1_1->use_nt != 1 || msv1_2->use_nt != 1)
		return (0);
	return (memcmp(msv1_1->nt_hash,
	    msv1_2->nt_hash, PPP_MSOFT_NT_HASH_LEN) == 0);
}

static int
ppp_auth_chap_msv1_final(struct ppp_auth_cred_chap *cred, struct ppp_log *log,
	int valid, const u_char *payload, size_t len, const u_char *authresp)
{
	char buf[256];
	char *s;

	/* Put payload into nul-terminated buffer */
	if (len > sizeof(buf) - 1)
		len = sizeof(buf) - 1;
	memcpy(buf, payload, len);
	buf[len] = '\0';

	/* Handle failure message */
	if (!valid) {
		const struct mschap_err *me;
		int err;

		if ((s = strstr(buf, "E=")) == NULL)
			return (0);
		if (sscanf(s + 2, "%d", &err) != 1)
			return (0);
		for (me = ppp_mschap_errs;
		    me->err != err && me->msg != NULL; me++);
		ppp_log_put(log, LOG_NOTICE, "error #%d: %s",
		    err, me->msg != NULL ? me->msg : "Unknown error");
		return (0);
	}

	/* Display message */
	ppp_util_ascify(buf, sizeof(buf), payload, len);
	ppp_log_put(log, LOG_INFO, "message: %s", buf);
	return (0);
}

