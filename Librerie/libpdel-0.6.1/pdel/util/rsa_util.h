
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_RSA_UTIL_H_
#define _PDEL_UTIL_RSA_UTIL_H_

__BEGIN_DECLS

/*
 * Create an RSA signature of an MD5 hash value using the private key file.
 */
extern int	rsa_util_sign(const char *privkeyfile,
			const u_char *md5, u_char *sig, size_t siglen);

/*
 * Verify an RSA signature of an MD5 hash value using the public key file.
 */
extern int	rsa_util_verify(const char *pubkeyfile, const u_char *md5,
			const u_char *sig, size_t siglen);

/*
 * Verify an RSA signature of an MD5 hash value using the private key file
 * (which also contains the public key).
 */
extern int	rsa_util_verify_priv(const char *privkeyfile,
			const u_char *md5, const u_char *sig, size_t siglen);

__END_DECLS

#endif	/* _PDEL_UTIL_RSA_UTIL_H_ */
