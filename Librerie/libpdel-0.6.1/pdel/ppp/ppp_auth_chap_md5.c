
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

#include <openssl/md5.h>

#define CHAP_MD5_CHALLENGE_LEN	16

static ppp_auth_chap_set_id_t	ppp_auth_chap_md5_set_id;
static ppp_auth_chap_hash_t	ppp_auth_chap_md5_hash;
static ppp_auth_chap_equal_t	ppp_auth_chap_md5_equal;
static ppp_auth_chap_final_t	ppp_auth_chap_md5_final;

const struct ppp_auth_chap_type ppp_auth_chap_md5 = {
	ppp_auth_chap_md5_set_id,
	ppp_auth_chap_md5_hash,
	ppp_auth_chap_md5_equal,
	ppp_auth_chap_md5_final,
	0,
	CHAP_MD5_CHALLENGE_LEN,
	offsetof(struct ppp_auth_cred_chap_md5, hash),
	MD5_DIGEST_LENGTH
};

static void
ppp_auth_chap_md5_set_id(struct ppp_auth_cred_chap *cred, u_char id)
{
	cred->u.md5.id = id;
}

static void
ppp_auth_chap_md5_hash(struct ppp_auth_cred_chap *chap,
	const void *secret, size_t slen)
{
	struct ppp_auth_cred_chap_md5 *const md5 = &chap->u.md5;
	MD5_CTX ctx;

	MD5_Init(&ctx);
	MD5_Update(&ctx, &md5->id, 1);
	MD5_Update(&ctx, secret, slen);
	MD5_Update(&ctx, &chap->chal_data, chap->chal_len);
	MD5_Final(md5->hash, &ctx);
}

static int
ppp_auth_chap_md5_equal(struct ppp_auth_cred_chap *chap1,
	struct ppp_auth_cred_chap *chap2)
{
	struct ppp_auth_cred_chap_md5 *const md5_1 = &chap1->u.md5;
	struct ppp_auth_cred_chap_md5 *const md5_2 = &chap2->u.md5;

	return (memcmp(md5_1->hash, md5_2->hash, MD5_DIGEST_LENGTH) == 0);
}

static int
ppp_auth_chap_md5_final(struct ppp_auth_cred_chap *cred, struct ppp_log *log,
	int valid, const u_char *payload, size_t len, const u_char *authresp)
{
	char buf[256];

	/* Display message */
	ppp_util_ascify(buf, sizeof(buf), payload, len);
	ppp_log_put(log, LOG_INFO, "message: %s", buf);
	return (0);
}

