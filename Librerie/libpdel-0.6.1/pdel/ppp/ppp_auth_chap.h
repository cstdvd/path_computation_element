
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_AUTH_CHAP_H_
#define _PDEL_PPP_PPP_AUTH_CHAP_H_

struct ppp_log;
struct ppp_auth_cred_chap;

/*
 * CHAP function to set ID in credentials structure
 */
typedef void	ppp_auth_chap_set_id_t(struct ppp_auth_cred_chap *cred,
			u_char id);

/*
 * CHAP hash function
 *
 * Should put the hash value into "cred", using the id and challenge
 * value in "cred" and the supplied secret.
 *
 * This function is for client code's use.
 */
typedef void	ppp_auth_chap_hash_t(struct ppp_auth_cred_chap *cred,
			const void *secret, size_t slen);

/*
 * CHAP compare function
 *
 * Should compare the two hash values and return 1 if they
 * agree, otherwise zero.
 *
 * This function is for client code's use.
 */
typedef int	ppp_auth_chap_equal_t(struct ppp_auth_cred_chap *cred1,
			struct ppp_auth_cred_chap *cred2);

/*
 * CHAP final function
 *
 * Handle any final action such as Microsoft ACK overloading in MS-CHAPv2.
 * This is called after recieving an ACK when we are authenticating
 * to the peer.
 */
typedef int	ppp_auth_chap_final_t(struct ppp_auth_cred_chap *cred,
			struct ppp_log *log, int valid, const u_char *payload,
			size_t len, const u_char *authresp);

/* CHAP type methods */
struct ppp_auth_chap_type {
	ppp_auth_chap_set_id_t	*set_id;	/* set 'id' in cred */
	ppp_auth_chap_hash_t	*hash;		/* hash method */
	ppp_auth_chap_equal_t	*equal;		/* compare method */
	ppp_auth_chap_final_t	*final;		/* final method */
	u_char			cfixed;		/* fixed challenge length */
	u_int			clen;		/* challenge length */
	u_int			roff;		/* response offset */
	u_int			rlen;		/* response length */
};

__BEGIN_DECLS

/* Supported CHAP types */
extern const struct ppp_auth_chap_type	ppp_auth_chap_md5;
extern const struct ppp_auth_chap_type	ppp_auth_chap_msv1;
extern const struct ppp_auth_chap_type	ppp_auth_chap_msv2;

__END_DECLS

#ifdef _PDEL_PPP_PRIVATE_H_

/* MS-CHAP errors */
struct mschap_err {
	int		err;
	const char	*msg;
};

__BEGIN_DECLS

/* Auth type methods for CHAP */
extern ppp_authtype_start_t	ppp_auth_chap_start;
extern ppp_authtype_cancel_t	ppp_auth_chap_cancel;
extern ppp_authtype_input_t	ppp_auth_chap_input;

extern const struct mschap_err ppp_mschap_errs[];

__END_DECLS

#endif	/* _PDEL_PPP_PRIVATE_H_ */

#endif	/* _PDEL_PPP_PPP_AUTH_CHAP_H_ */
