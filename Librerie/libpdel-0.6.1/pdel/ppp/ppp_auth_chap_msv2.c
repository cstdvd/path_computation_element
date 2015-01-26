
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

#define PATH_RANDOM		"/dev/urandom"
#define HEXVAL(c)		(isdigit(c) ? (c) - '0' : tolower(c) - 'a' + 10)

static ppp_auth_chap_hash_t	ppp_auth_chap_msv2_hash;
static ppp_auth_chap_equal_t	ppp_auth_chap_msv2_equal;
static ppp_auth_chap_final_t	ppp_auth_chap_msv2_final;

/* MS-CHAP version 1 descriptor */
const struct ppp_auth_chap_type ppp_auth_chap_msv2 = {
	NULL,
	ppp_auth_chap_msv2_hash,
	ppp_auth_chap_msv2_equal,
	ppp_auth_chap_msv2_final,
	1,
	PPP_MSOFTV2_CHAL_LEN,
	0,
	PPP_MSOFT_RESP_LEN
};

static void
ppp_auth_chap_msv2_hash(struct ppp_auth_cred_chap *chap,
	const void *secret, size_t slen)
{
	struct ppp_auth_cred_chap_msv2 *const msv2 = &chap->u.msv2;
	const char *user;
	int fd;

	/* Get username without domain part */
	if ((user = strrchr(chap->name, '\\')) != NULL)
		user++;
	else
		user = chap->name;

	/* Create challenge for peer */
	if ((fd = open(PATH_RANDOM, O_RDONLY)) == -1)
		goto nochal;
	if (read(fd, msv2->peer_chal, sizeof(msv2->peer_chal)) == -1) {
		(void)close(fd);
		goto nochal;
	}
	(void)close(fd);

nochal:
	memset(&msv2->reserved, 0, sizeof(msv2->reserved));
	msv2->flags = 0x04;
	ppp_msoft_generate_nt_response(chap->chal_data,
	    msv2->peer_chal, user, secret, msv2->nt_response);
}

static int
ppp_auth_chap_msv2_equal(struct ppp_auth_cred_chap *chap1,
	struct ppp_auth_cred_chap *chap2)
{
	struct ppp_auth_cred_chap_msv2 *const msv2_1 = &chap1->u.msv2;
	struct ppp_auth_cred_chap_msv2 *const msv2_2 = &chap2->u.msv2;

	(void)msv2_1;
	(void)msv2_2;
	return (0);			/* XXX implement me */
}

static int
ppp_auth_chap_msv2_final(struct ppp_auth_cred_chap *cred, struct ppp_log *log,
	int valid, const u_char *payload, size_t len, const u_char *authresp)
{
	u_char servresp[PPP_MSOFTV2_AUTHRESP_LEN];
	char buf[256];
	const char *s;
	int i;

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

	/* Parse out server response */
	if ((s = strstr(buf, "S=")) == NULL)
		return (-1);
	s += 2;
	for (i = 0; i < sizeof(servresp); i++) {
		if (!isxdigit(s[i * 2]) || !isxdigit(s[i * 2 + 1]))
			return (-1);
		servresp[i] = (HEXVAL(s[i * 2]) << 4) | HEXVAL(s[i * 2 + 1]);
	}

	/* Verify response */
	if (memcmp(servresp, authresp, sizeof(servresp)) != 0) {
		ppp_log_put(log, LOG_NOTICE,
		    "server MS-CHAPv2 authentication is invalid");
		errno = EAUTH;
		return (-1);
	}

	/* Display message */
	if ((s = strstr(buf, "M=")) == NULL)
		return (0);
	ppp_util_ascify(buf, sizeof(buf), s, strlen(s));
	ppp_log_put(log, LOG_INFO, "message: %s", buf);
	return (0);
}


