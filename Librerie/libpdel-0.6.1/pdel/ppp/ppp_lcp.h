
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_LCP_H_
#define _PDEL_PPP_PPP_LCP_H_

struct ppp_fsm;
struct ppp_node;

/*
 * LCP constants
 */
#define LCP_MIN_MRU		64
#define LCP_MAX_MRU		8192
#define LCP_DEFAULT_MRU		1500

#define LCP_MIN_MRRU		1500
#define LCP_MAX_MRRU		8192
#define LCP_DEFAULT_MRRU	1600

/*
 * Configuration for an LCP FSM
 */
struct ppp_lcp_config {

	u_int16_t	min_mru[2];
	u_int16_t	max_mru[2];
	u_int16_t	min_mrru[2];
	u_int16_t	max_mrru[2];

	u_int32_t	accm;

	u_char		auth[2][PPP_AUTH_MAX];

	u_char		multilink[2];	/* request, both, or neither */
	u_char		shortseq[2];
	u_char		pfcomp[2];
	u_char		acfcomp[2];

	struct ppp_eid	eid;
};

/*
 * LCP request state
 */
struct ppp_lcp_req {
	u_int16_t		mru[2];
	u_int32_t		accm[2];
	u_char			acfcomp[2];
	u_char			pfcomp[2];
	u_int32_t		magic[2];
	enum ppp_auth_index	auth[2];
	u_char			auth_rej[PPP_AUTH_MAX];
	u_char			multilink[2];
	u_int16_t		mrru[2];
	u_char			shortseq[2];
	struct ppp_eid		eid[2];
};

__BEGIN_DECLS

/* FSM types for LCP (normal) and MP LCP (multilink-bundle) */
extern const	struct ppp_fsm_type ppp_fsm_lcp;
extern const	struct ppp_fsm_type ppp_fsm_mp_lcp;

/* LCP option descriptors */
extern const	struct ppp_fsm_optdesc lcp_opt_desc[];

/* Functions */
extern struct	ppp_fsm_instance *ppp_lcp_create(struct ppp_lcp_config *conf);
extern void	ppp_lcp_get_req(struct ppp_fsm *lcp, struct ppp_lcp_req *req);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_LCP_H_ */
