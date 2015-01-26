
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_CCP_H_
#define _PDEL_PPP_PPP_CCP_H_

struct ppp_fsm;
struct ppp_node;

/*
 * Configuration for a CCP FSM
 */
struct ppp_ccp_config {
	u_char			mppc[2];
	u_char			mppe40[2];
	u_char			mppe56[2];
	u_char			mppe128[2];
	u_char			mppe_stateless[2];
};

/*
 * CCP request state
 */
struct ppp_ccp_req {
	u_char			mppc[2];
	u_char			mppe40[2];
	u_char			mppe56[2];
	u_char			mppe128[2];
	u_char			mppe_stateless[2];
};

__BEGIN_DECLS

/* FSM type for CCP */
extern const	struct ppp_fsm_type ppp_fsm_ccp;

/* Functions */
extern struct	ppp_fsm_instance *ppp_ccp_create(struct ppp_ccp_config *conf,				struct ppp_node *node);
extern void	ppp_ccp_get_req(struct ppp_fsm *ccp, struct ppp_ccp_req *req);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_CCP_H_ */
