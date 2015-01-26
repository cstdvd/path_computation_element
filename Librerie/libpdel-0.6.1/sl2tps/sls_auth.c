
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "sls_global.h"
#include "sls_config.h"

/*
 * PPP authorization callbacks and configuration.
 */
static ppp_auth_acquire_t		sls_auth_acquire;
static ppp_auth_check_t			sls_auth_check;

static struct ppp_auth_meth sls_auth_meth = {
	sls_auth_acquire,
	sls_auth_check,
};

const struct ppp_auth_config sls_auth_config = {
	&sls_auth_meth,
	{
		(1 << PPP_AUTH_CHAP_MSV1)
		| (1 << PPP_AUTH_CHAP_MSV2)
		| (1 << PPP_AUTH_CHAP_MD5),
		(1 << PPP_AUTH_NONE)
	}
};

static int
sls_auth_acquire(struct ppp_link *link,
	struct ppp_auth_cred *creds, struct ppp_auth_resp *resp)
{
	errno = EPROTONOSUPPORT;
	return (-1);
}

static int
sls_auth_check(struct ppp_link *link,
	const struct ppp_auth_cred *creds, struct ppp_auth_resp *resp)
{
	static const struct sls_user *user;
	const char *username;
	int i;

	/* Get username */
	switch (creds->type) {
	case PPP_AUTH_PAP:
		username = creds->u.pap.name;
		break;
	case PPP_AUTH_CHAP_MSV1:
	case PPP_AUTH_CHAP_MSV2:
	case PPP_AUTH_CHAP_MD5:
		username = creds->u.chap.name;
		break;
	default:
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "unsupported auth type");
		return -1;
	}

	/* Find user */
	for (i = 0; i < sls_curconf->users.length
	    && strcmp(sls_curconf->users.elems[i].name, username) != 0; i++);
	if (i == sls_curconf->users.length) {
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "unknown user \"%s\"", username);
		return -1;
	}
	user = &sls_curconf->users.elems[i];

	/* Check credentials */
	switch (creds->type) {
	case PPP_AUTH_PAP:
	    {
		const struct ppp_auth_cred_pap *const pap = &creds->u.pap;

		if (strcmp(pap->password, user->password) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "wrong password");
			return -1;
		}
		break;
	    }
	case PPP_AUTH_CHAP_MSV1:
	    {
		const struct ppp_auth_cred_chap *const chap = &creds->u.chap;
		const struct ppp_auth_cred_chap_msv1 *const rsp = &chap->u.msv1;
		u_char buf[PPP_MSOFT_NT_HASH_LEN];

		/* Check response */
		if (!rsp->use_nt) {		/* disallow lan-man hash */
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "LAN-MAN hash unacceptable");
			return -1;
		}
		ppp_msoft_nt_challenge_response(chap->chal_data,
		    user->password, buf);
		if (memcmp(rsp->nt_hash, buf, sizeof(buf)) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "MSCHAPv1 hash is invalid");
			return -1;
		}

		/* Derive MPPE keys */
		ppp_msoft_init_key_v1(0, user->password,
		    chap->chal_data, resp->mppe.msv1.key_64);
		ppp_msoft_init_key_v1(1, user->password,
		    chap->chal_data, resp->mppe.msv1.key_128);
		break;
	    }
	case PPP_AUTH_CHAP_MSV2:
	    {
		const struct ppp_auth_cred_chap *const chap = &creds->u.chap;
		const struct ppp_auth_cred_chap_msv2 *const rsp = &chap->u.msv2;
		u_char buf[PPP_MSOFT_NT_HASH_LEN];

		/* Check response */
		ppp_msoft_generate_nt_response(chap->chal_data,
		    rsp->peer_chal, chap->name, user->password, buf);
		if (memcmp(rsp->nt_response, buf, sizeof(buf)) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "MSCHAPv2 hash is invalid");
			return -1;
		}

		/* Generate expected authenticator response for reply */
		ppp_msoft_generate_authenticator_response(user->password,
		    rsp->nt_response, rsp->peer_chal, chap->chal_data,
		    chap->name, resp->authresp);

		/* Derive MPPE keys */
		for (i = 0; i < 2; i++) {
			ppp_msoft_init_key_v2(i, user->password,
			    rsp->nt_response, resp->mppe.msv2.keys[i]);
		}
		break;
	    }
	case PPP_AUTH_CHAP_MD5:
	    {
		const struct ppp_auth_cred_chap *const chap = &creds->u.chap;
		struct ppp_auth_cred_chap temp;

		strlcpy(temp.name, chap->name, sizeof(temp.name));
		temp.chal_len = chap->chal_len;
		memcpy(temp.chal_data, chap->chal_data, chap->chal_len);
		temp.u.md5.id = chap->u.md5.id;
		(*ppp_auth_chap_md5.hash)(&temp,
		    user->password, strlen(user->password));
		if (memcmp(temp.u.md5.hash,
		    chap->u.md5.hash, sizeof(temp.u.md5.hash)) != 0) {
			snprintf(resp->errmsg, sizeof(resp->errmsg),
			    "invalid MD5 hash");
			return -1;
		}
		break;
	    }
	default:
		snprintf(resp->errmsg, sizeof(resp->errmsg),
		    "unsupported auth check");
		errno = EPROTONOSUPPORT;
		return -1;
	}

	/* Authorization successful */
	return 0;
}

