
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_IPCP_H_
#define _PDEL_PPP_PPP_IPCP_H_

struct ppp_fsm;
struct ppp_node;

/*
 * Configuration for an IPCP FSM
 */
struct ppp_ipcp_config {
	struct in_addr	ip[2];
	struct in_addr	mask[2];
	u_char		do_dns[2];
	struct in_addr	dns[2];
	u_char		do_nbns[2];
	struct in_addr	nbns[2];
};

/*
 * IPCP request state
 */
struct ppp_ipcp_vjc {
	u_char		enabled;
	u_char		maxchan;
	u_char		compcid;
};

struct ppp_ipcp_req {
	struct in_addr		ip[2];
	struct ppp_ipcp_vjc	vjc[2];
	u_char			ask_dns;
	u_char			ask_nbns;
	struct in_addr		dns[2];
	struct in_addr		nbns[2];
};

__BEGIN_DECLS

/* FSM type for IPCP */
extern const	struct ppp_fsm_type ppp_fsm_ipcp;

/* Functions */
extern struct	ppp_fsm_instance *ppp_ipcp_create(struct ppp_ipcp_config *conf,
			struct ppp_node *node);
extern void	ppp_ipcp_get_req(struct ppp_fsm *ipcp,
			struct ppp_ipcp_req *req);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_IPCP_H_ */
