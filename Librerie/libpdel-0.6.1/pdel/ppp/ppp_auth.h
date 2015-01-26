
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_AUTH_H_
#define _PDEL_PPP_PPP_AUTH_H_

struct ppp_auth_config;
struct ppp_fsm_option;
struct ppp_link;
struct ppp_log;

/* Types of authentication (in reverse order of preference) */
enum ppp_auth_index {
	PPP_AUTH_NONE = 0,
	PPP_AUTH_PAP,
	PPP_AUTH_CHAP_MSV1,
	PPP_AUTH_CHAP_MSV2,
	PPP_AUTH_CHAP_MD5,
	PPP_AUTH_MAX
};

#ifndef MD5_DIGEST_LENGTH
#define MD5_DIGEST_LENGTH		16
#endif

/* Max authorization username and password length */
#define PPP_MAX_AUTHNAME		64
#define PPP_MAX_AUTHPASS		64

/* Max challenge/response data length */
#define PPP_MAX_AUTHVALUE		64

/* Microsoft stuff */
#define PPP_MSOFT_LM_HASH_LEN		24
#define PPP_MSOFT_NT_HASH_LEN		24
#define PPP_MSOFTV1_CHAL_LEN		8
#define PPP_MSOFTV2_CHAL_LEN		16
#define PPP_MSOFT_RESP_LEN		49
#define PPP_MSOFTV2_AUTHRESP_LEN	20

#define PPP_MPPE_DATA_MAX	MAX(PPP_MSOFTV1_CHAL_LEN, PPP_MSOFT_NT_HASH_LEN)

/***********************************************************************
			AUTHORIZATION CREDENTIALS
***********************************************************************/

/* Credentials for PAP */
struct ppp_auth_cred_pap {
	char		name[PPP_MAX_AUTHNAME];
	char		password[PPP_MAX_AUTHPASS];
};

/* Response data for MD5 CHAP */
struct ppp_auth_cred_chap_md5 {
	u_char		id;
	u_char		hash[MD5_DIGEST_LENGTH];
};

/* Response data for MSoft CHAPv1 */
struct ppp_auth_cred_chap_msv1 {
	u_char		lm_hash[PPP_MSOFT_LM_HASH_LEN];
	u_char		nt_hash[PPP_MSOFT_NT_HASH_LEN];
	u_char		use_nt;
};

/* Response data for MSoft CHAPv2 */
struct ppp_auth_cred_chap_msv2 {
	u_char		peer_chal[PPP_MSOFTV2_CHAL_LEN];
	u_char		reserved[8];
	u_char		nt_response[PPP_MSOFT_NT_HASH_LEN];
	u_char		flags;
};

/* Credentials for CHAP */
struct ppp_auth_cred_chap {
	char		name[PPP_MAX_AUTHNAME];
	u_char		chal_len;
	u_char		chal_data[PPP_MAX_AUTHVALUE];
	union {
		struct ppp_auth_cred_chap_md5	md5;
		struct ppp_auth_cred_chap_msv1	msv1;
		struct ppp_auth_cred_chap_msv2	msv2;
	}		u;
};

/* Authorization credentials info */
struct ppp_auth_cred {
	enum ppp_auth_index	type;
	union {
		struct ppp_auth_cred_pap	pap;
		struct ppp_auth_cred_chap	chap;
	}		u;
};

/***********************************************************************
			AUTHORIZATION RESPONSE
***********************************************************************/

/* Microsoft MPPE information derived from CHAP exchange */
struct ppp_auth_mppe_chapv1 {
	u_char		key_64[8];		/* lan-man hash (40, 56 bits) */
	u_char		key_128[16];		/* start key (128 bits) */
};

struct ppp_auth_mppe_chapv2 {
	u_char		keys[2][16];		/* server xmit key is first */
};

union ppp_auth_mppe {
	struct ppp_auth_mppe_chapv1	msv1;
	struct ppp_auth_mppe_chapv2	msv2;
};

/* Authorization response info */
struct ppp_auth_resp {
	u_char			authresp[PPP_MSOFTV2_AUTHRESP_LEN];
	union ppp_auth_mppe	mppe;		/* mppe keys */
	char			errmsg[64];	/* error message */
};

/***********************************************************************
			CREDENTIALS CALLBACKS
***********************************************************************/

/*
 * Function type for acquiring credentials. Any name and/or challenge
 * data will already be present in the credentials structure.
 *
 * Note: if type is PPP_AUTH_CHAP_MSV2, the caller MUST fill in the
 * "authresp" array with the 20 byte MS-CHAPv2 authenticator response.
 *
 * Note: if type is PPP_AUTH_CHAP_MSV1 or PPP_AUTH_CHAP_MSV2, the caller
 * SHOULD fill in the "mppe" structure with the MPPE key(s).
 *
 * Note: this function will be called in a separate thread that may
 * be canceled at any time; it should be prepared to clean up if so.
 *
 * Note: 'resp' has been zeroed out when this function is invoked.
 * The MPPE key fields should remain zeroed out unless valid keys
 * are present.
 *
 * Returns:
 *	 0	Credentials found
 *	-1	Credentials can't be found. Set errno or resp->errmsg.
 */
typedef int	ppp_auth_acquire_t(struct ppp_link *link,
			struct ppp_auth_cred *creds,
			struct ppp_auth_resp *resp);

/*
 * Function type for checking credentials.
 *
 * Note: if type is PPP_AUTH_CHAP_MSV2, the caller must fill in the
 * "authresp" array with the 20 byte MS-CHAPv2 authenticator response.
 *
 * Note: if type is PPP_AUTH_CHAP_MSV1 or PPP_AUTH_CHAP_MSV2, the caller
 * SHOULD fill in the "mppe" structure with the MPPE key(s).
 *
 * Note: this function will be called in a separate thread that may
 * be canceled at any time; it should be prepared to clean up if so.
 *
 * Note: 'resp' has been zeroed out when this function is invoked.
 * The MPPE key fields should remain zeroed out unless valid keys
 * are present.
 *
 * Returns:
 *	 0	Credentials are valid
 *	-1	Credentials can't be validated. Set errno or resp->errmsg.
 */
typedef int	ppp_auth_check_t(struct ppp_link *link,
			const struct ppp_auth_cred *creds,
			struct ppp_auth_resp *resp);

/*
 * Authorization information supplied by caller.
 */
struct ppp_auth_meth {
	ppp_auth_acquire_t	*acquire;
	ppp_auth_check_t	*check;
};

/* Authorization configuration for a link */
struct ppp_auth_config {
	struct ppp_auth_meth	*meth;		/* auth_config callbacks */
	u_int32_t		allow[2];	/* auth types allowed (bits) */
};

/***********************************************************************
			PPP PRIVATE STUFF
***********************************************************************/

#ifdef _PDEL_PPP_PRIVATE_H_

/*
 * Authorization type methods
 */
typedef void	*ppp_authtype_start_t(struct pevent_ctx *ev_ctx,
			struct ppp_link *link, pthread_mutex_t *mutex,
			int dir, u_int16_t *protop, struct ppp_log *log);
typedef void	ppp_authtype_cancel_t(void *arg);
typedef void	ppp_authtype_input_t(void *arg,
			int dir, void *data, size_t len);

/* Authorization type descriptor */
struct ppp_auth_type {
	const char		*name;		/* name */
	enum ppp_auth_index	index;		/* auth type index */
	ppp_authtype_start_t	*start;		/* start method */
	ppp_authtype_cancel_t	*cancel;	/* cancel method */
	ppp_authtype_input_t	*input;		/* input packet method */
	u_int			len;		/* length of option data */
	const u_char		data[8];	/* option data */
};

__BEGIN_DECLS

/* Authorization type functions */
extern const	struct ppp_auth_type *ppp_auth_by_option(
			const struct ppp_fsm_option *opt);
extern const	struct ppp_auth_type *ppp_auth_by_index(
			enum ppp_auth_index index);

extern opt_pr_t	ppp_auth_print;

__END_DECLS

#endif	/* _PDEL_PPP_PRIVATE_H_ */

#endif	/* _PDEL_PPP_PPP_AUTH_H_ */
