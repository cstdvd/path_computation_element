
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_msoft.h"

#include <openssl/md4.h>
#include <openssl/des.h>
#include <openssl/sha.h>

/* Magic constants */
#define MS_MAGIC_1	"This is the MPPE Master Key"
#define MS_MAGIC_2	"On the client side, this is the send key;" \
			" on the server side, it is the receive key."
#define MS_MAGIC_3	"On the client side, this is the receive key;" \
			" on the server side, it is the send key."
#define MS_AR_MAGIC_1	"Magic server to client signing constant"
#define MS_AR_MAGIC_2	"Pad to make it do more than one iteration"

/* Internal functions */
static void	ppp_msoft_challenge_response(const u_char *chal,
			const char *pwHash, u_char *hash);
static void	ppp_msoft_des_encrypt(const u_char *clear,
			u_char *key0, u_char *cypher);
static void	ppp_msoft_challenge_hash(const u_char *peerchal,
			const u_char *authchal, const char *username,
			u_char *hash);

/*
 * ppp_msoft_lm_password_hash()
 *
 * password	ASCII password
 * hash		16 byte output LanManager hash
 */
void
ppp_msoft_lm_password_hash(const char *password, u_char *hash)
{
	const char *const clear = "KGS!@#$%%";
	u_char up[14];
	int k;

	memset(&up, 0, sizeof(up));
	for (k = 0; k < sizeof(up) && password[k]; k++)
		up[k] = toupper(password[k]);

	ppp_msoft_des_encrypt(clear, &up[0], &hash[0]);
	ppp_msoft_des_encrypt(clear, &up[7], &hash[8]);
}

/*
 * ppp_msoft_nt_password_hash()
 *
 * password	ASCII (NOT Unicode) password
 * hash		16 byte output NT hash
 */
void
ppp_msoft_nt_password_hash(const char *password, u_char *hash)
{
	u_int16_t unipw[128];
	const char *s;
	int unipwLen;
	MD4_CTX	ctx;

	/* Convert password to Unicode */
	for (unipwLen = 0, s = password;
	    unipwLen < sizeof(unipw) / 2 && *s; s++)
		unipw[unipwLen++] = htons(*s << 8);

	/* Compute MD4 of Unicode password */
	MD4_Init(&ctx);
	MD4_Update(&ctx, (u_char *)unipw, unipwLen * sizeof(*unipw));
	MD4_Final(hash, &ctx);
}

/*
 * ppp_msoft_nt_challenge_response()
 *
 * chal		8 byte challenge
 * password	ASCII (NOT Unicode) password
 * hash		24 byte response
 */
void
ppp_msoft_nt_challenge_response(const u_char *chal,
	const char *password, u_char *hash)
{
	u_char pwHash[16];

	ppp_msoft_nt_password_hash(password, pwHash);
	ppp_msoft_challenge_response(chal, pwHash, hash);
}

/*
 * ppp_msoft_challenge_response()
 *
 * chal		8 byte challenge
 * pwHash	16 byte password hash
 * hash		24 byte response
 */
static void
ppp_msoft_challenge_response(const u_char *chal,
	const char *pwHash, u_char *hash)
{
	u_char buf[21];
	int i;

	/* Initialize buffer */
	memset(&buf, 0, sizeof(buf));
	memcpy(buf, pwHash, 16);

	/* Use DES to hash the hash */
	for (i = 0; i < 3; i++) {
		u_char *const key = &buf[i * 7];
		u_char *const output = &hash[i * 8];

		ppp_msoft_des_encrypt(chal, key, output);
	}
}

/*
 * ppp_msoft_des_encrypt()
 *
 * clear	8 byte cleartext
 * key		7 byte key
 * cypher	8 byte cyphertext
 */
static void
ppp_msoft_des_encrypt(const u_char *clear, u_char *key0, u_char *cypher)
{
	des_key_schedule ks;
	u_char key[8];

	/*
	 * Create DES key
	 *
	 * Note: we don't bother setting the parity bit because
	 * the des_set_key() algorithm does that for us. A different
	 * algorithm may care though.
	 */
	key[0] = key0[0] & 0xfe;
	key[1] = (key0[0] << 7) | (key0[1] >> 1);
	key[2] = (key0[1] << 6) | (key0[2] >> 2);
	key[3] = (key0[2] << 5) | (key0[3] >> 3);
	key[4] = (key0[3] << 4) | (key0[4] >> 4);
	key[5] = (key0[4] << 3) | (key0[5] >> 5);
	key[6] = (key0[5] << 2) | (key0[6] >> 6);
	key[7] = key0[6] << 1;
	des_set_key((des_cblock *)key, ks);

	/* Encrypt using key */
	des_ecb_encrypt((des_cblock *)clear, (des_cblock *)cypher, ks, 1);
}

/*
 * ppp_msoft_get_start_key()
 */
void
ppp_msoft_get_start_key(const u_char *chal, u_char *hash)
{
	u_char sha1[SHA_DIGEST_LENGTH];
	SHA_CTX ctx;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, hash, 16);
	SHA1_Update(&ctx, hash, 16);
	SHA1_Update(&ctx, chal, 8);
	SHA1_Final(sha1, &ctx);
	memcpy(hash, sha1, 16);
}

/*
 * ppp_msoft_generate_nt_response()
 *
 * authchal	16 byte authenticator challenge
 * peerchal	16 byte peer challenge
 * username	ASCII username
 * password	ASCII (NOT Unicode) password
 * hash		24 byte response
 */
void
ppp_msoft_generate_nt_response(const u_char *authchal, const u_char *peerchal,
	const char *username, const char *password, u_char *hash)
{
	u_char chal[8];
	u_char pwHash[16];

	ppp_msoft_challenge_hash(peerchal, authchal, username, chal);
	ppp_msoft_nt_password_hash(password, pwHash);
	ppp_msoft_challenge_response(chal, pwHash, hash);
}

/*
 * ppp_msoft_challenge_hash()
 *
 * peerchal	16 byte peer challenge
 * authchal	16 byte authenticator challenge
 * username	ASCII username
 * hash		8 byte response
 */
static void
ppp_msoft_challenge_hash(const u_char *peerchal, const u_char *authchal,
	const char *username, u_char *hash)
{
	u_char sha1[SHA_DIGEST_LENGTH];
	SHA_CTX ctx;
	const char *slash;

	/* Strip off NT domain (if any) */
	if ((slash = strrchr(username, '\\')) != NULL)
		username = slash + 1;
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, peerchal, 16);
	SHA1_Update(&ctx, authchal, 16);
	SHA1_Update(&ctx, username, strlen(username));
	SHA1_Final(sha1, &ctx);
	memcpy(hash, sha1, 8);
}

/*
 * Compute master key for MPPE
 */
void
ppp_msoft_get_master_key(const u_char *resp, u_char *hash)
{
	u_char sha1[SHA_DIGEST_LENGTH];
	SHA_CTX ctx;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, hash, 16);
	SHA1_Update(&ctx, resp, 24);
	SHA1_Update(&ctx, MS_MAGIC_1, 27);
	SHA1_Final(sha1, &ctx);
	memcpy(hash, sha1, 16);
}

/*
 * Compute asymmetric start key for MPPE (MS-CHAPv2 authentication)
 *
 * which	Zero for server xmit/client recv, 1 for server recv/client xmit
 */
void
ppp_msoft_get_asymetric_start_key(int which, const u_char *master, u_char *key)
{
	u_char sha1[SHA_DIGEST_LENGTH];
	u_char sha_pad[2][40];
	SHA_CTX ctx;

	/* Generate start key */
	memset(&sha_pad[0], 0x00, sizeof(sha_pad[0]));
	memset(&sha_pad[1], 0xf2, sizeof(sha_pad[1]));
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, master, 16);
	SHA1_Update(&ctx, sha_pad[0], 40);
	SHA1_Update(&ctx, which ? MS_MAGIC_2 : MS_MAGIC_3, 84);
	SHA1_Update(&ctx, sha_pad[1], 40);
	SHA1_Final(sha1, &ctx);
	memcpy(key, sha1, 16);
}

/*
 * Initialize MPPE key based on MS-CHAPv1 credentials.
 *
 * e128		Non-zero for 128 bit key, zero for 64 bit key.
 * pass		Originating side's password
 * chal		Answering side's challenge
 * key		16 byte output buffer
 */
void
ppp_msoft_init_key_v1(int e128,
	const char *pass, const u_char *chal, u_char *key)
{
	if (e128) {					/* 128 bit key */
		u_char hash[16];
		MD4_CTX ctx;

		ppp_msoft_nt_password_hash(pass, hash);
		MD4_Init(&ctx);
		MD4_Update(&ctx, hash, 16);
		MD4_Final(hash, &ctx);
		ppp_msoft_get_start_key(chal, hash);
		memcpy(key, hash, 16);
	} else						/* 40 or 56 bit key */
		ppp_msoft_lm_password_hash(pass, key);
}

/*
 * Initialize MPPE key based on MS-CHAPv2 credentials.
 *
 * which	Zero for server xmit/client recv, 1 for server recv/client xmit
 * pass		Originating side's password
 * resp		Answering side's challenge response hash
 * key		16 byte output buffer
 */
void
ppp_msoft_init_key_v2(int which,
	const char *pass, const u_char *resp, u_char *key)
{
	u_char hash[16];
	MD4_CTX ctx;

	ppp_msoft_nt_password_hash(pass, hash);
	MD4_Init(&ctx);
	MD4_Update(&ctx, hash, 16);
	MD4_Final(hash, &ctx);
	ppp_msoft_get_master_key(resp, hash);
	ppp_msoft_get_asymetric_start_key(which, hash, key);
}

/*
 * Generate response to MS-CHAPv2 piggy-backed challenge.
 *
 * "authresp" must point to a 20 byte buffer.
 */
void
ppp_msoft_generate_authenticator_response(const char *password,
	const u_char *ntresp, const u_char *peerchal,
	const u_char *authchal, const char *username, u_char *authresp)
{
	u_char hash[16];
	u_char digest[SHA_DIGEST_LENGTH];
	u_char chal[8];
	MD4_CTX md4ctx;
	SHA_CTX shactx;

	ppp_msoft_nt_password_hash(password, hash);

	MD4_Init(&md4ctx);
	MD4_Update(&md4ctx, hash, 16);
	MD4_Final(hash, &md4ctx);

	SHA1_Init(&shactx);
	SHA1_Update(&shactx, hash, 16);
	SHA1_Update(&shactx, ntresp, 24);
	SHA1_Update(&shactx, MS_AR_MAGIC_1, 39);
	SHA1_Final(digest, &shactx);

	ppp_msoft_challenge_hash(peerchal, authchal, username, chal);

	SHA1_Init(&shactx);
	SHA1_Update(&shactx, digest, sizeof(digest));
	SHA1_Update(&shactx, chal, 8);
	SHA1_Update(&shactx, MS_AR_MAGIC_2, 41);
	SHA1_Final(authresp, &shactx);
}

/*
 * Verify server's piggy-backed response.
 *
 * Returns 1 if OK, otherwise zero.
 */
int
ppp_msoft_check_authenticator_response(const char *password,
	const u_char *ntresp, const u_char *peerchal, const u_char *authchal,
	const char *username, const char *rechash)
{
	char authresp[43];

	ppp_msoft_generate_authenticator_response(password, ntresp,
	    peerchal, authchal, username, authresp);
	return (strcmp(rechash, authresp) == 0);
}


