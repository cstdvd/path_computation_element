
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/param.h>
#include <sys/syslog.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/md5.h>
#include <openssl/err.h>
#include <openssl/md5.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/rsa_util.h"
#include "util/typed_mem.h"

static int	rsa_util_verify_rsa(RSA *rsa, const u_char *md5,
			const u_char *sig, size_t siglen);

/*
 * Create an RSA signature given an RSA private key filename.
 *
 * We assume the thing to be signed is an MD5 hash.
 *
 * Returns the signature length, or -1 if error.
 */
int
rsa_util_sign(const char *privkeyfile,
	const u_char *md5, u_char *sig, size_t siglen)
{
	u_char *vbuf = NULL;			/* encrypted signature (md5) */
	FILE *fp = NULL;
	RSA *rsa = NULL;
	int rtn = -1;
	int vlen;

	/* Open file */
	if ((fp = fopen(privkeyfile, "r")) == NULL) {
		fprintf(stderr, "%s: %s\n", privkeyfile, strerror(errno));
		goto done;
	}

	/* Read private key */
	if ((rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL)) == NULL) {
		ERR_print_errors_fp(stderr);
		goto done;
	}

	/* Check buffer length */
	if (siglen < RSA_size(rsa)) {
		errno = EINVAL;
		goto done;
	}

	/* Encrypt using private key */
	if ((vlen = RSA_private_encrypt(MD5_DIGEST_LENGTH,
	    (u_char *)md5, sig, rsa, RSA_PKCS1_PADDING)) <= 0) {
		ERR_print_errors_fp(stderr);
		goto done;
	}

	/* OK */
	rtn = vlen;

done:
	/* Clean up */
	FREE(TYPED_MEM_TEMP, vbuf);
	RSA_free(rsa);
	if (fp != NULL)
		fclose(fp);

	/* Return result */
	return (rtn);
}

/*
 * Verify an RSA signature given a public key filename.
 *
 * We assume the signature is a signature of an MD5 hash, and therefore
 * the input to the signature had length MD5_DIGEST_LENGTH.
 */
int
rsa_util_verify(const char *pubkeyfile, const u_char *md5,
	const u_char *sig, size_t siglen)
{
	EVP_PKEY *pkey = NULL;
	RSA *rsa = NULL;
	BIO *bio = NULL;
	int match = 0;

	/* Read in public key */
	if ((bio = BIO_new(BIO_s_file())) == NULL) {
		ERR_print_errors_fp(stderr);
		goto done;
	}
	if (BIO_read_filename(bio, pubkeyfile) <= 0)
		goto done;
	if ((pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
		ERR_print_errors_fp(stderr);
		goto done;
	}

	/* Get RSA public key */
	if ((rsa = EVP_PKEY_get1_RSA(pkey)) == NULL) {
		ERR_print_errors_fp(stderr);
		goto done;
	}

	/* Verify */
	match = rsa_util_verify_rsa(rsa, md5, sig, siglen);

done:
	/* Clean up */
	RSA_free(rsa);
	EVP_PKEY_free(pkey);
	BIO_free(bio);

	/* Return result */
	return (match);
}

/*
 * Verify an RSA signature given a private key filename.
 *
 * We assume the signature is a signature of an MD5 hash, and therefore
 * the input to the signature had length MD5_DIGEST_LENGTH.
 */
int
rsa_util_verify_priv(const char *privkeyfile, const u_char *md5,
	const u_char *sig, size_t siglen)
{
	FILE *fp = NULL;
	RSA *rsa = NULL;
	int match = 0;

	/* Open file */
	if ((fp = fopen(privkeyfile, "r")) == NULL) {
		fprintf(stderr, "%s: %s\n", privkeyfile, strerror(errno));
		goto done;
	}

	/* Read private key */
	if ((rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL)) == NULL) {
		ERR_print_errors_fp(stderr);
		goto done;
	}
	ERR_clear_error();

	/* Verify */
	match = rsa_util_verify_rsa(rsa, md5, sig, siglen);

done:
	/* Clean up */
	RSA_free(rsa);
	if (fp != NULL)
		fclose(fp);

	/* Return result */
	return (match);
}

/*
 * Verify an RSA signature given an RSA public key.
 *
 * We assume the signature is a signature of an MD5 hash, and therefore
 * the input to the signature had length MD5_DIGEST_LENGTH.
 */
static int
rsa_util_verify_rsa(RSA *rsa, const u_char *md5,
	const u_char *sig, size_t siglen)
{
	u_char *vbuf = NULL;			/* decrypted signature */
	int match = 0;
	int vlen;

	/* Decrypt using public key */
	if ((vbuf = MALLOC(TYPED_MEM_TEMP, MAX(siglen, RSA_size(rsa)))) == NULL)
		goto done;
	if ((vlen = RSA_public_decrypt(siglen,
	    (u_char *)sig, vbuf, rsa, RSA_PKCS1_PADDING)) <= 0) {
		ERR_clear_error();
		goto done;
	}

	/* Compare decrypted signature with original hash value */
	if (vlen == MD5_DIGEST_LENGTH
	    && memcmp(md5, vbuf, MD5_DIGEST_LENGTH) == 0)
		match = 1;

done:
	/* Clean up */
	FREE(TYPED_MEM_TEMP, vbuf);

	/* Return result */
	return (match);
}


