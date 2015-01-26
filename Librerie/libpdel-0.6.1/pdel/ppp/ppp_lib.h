
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_LIB_H_
#define _PDEL_PPP_PPP_LIB_H_

/************************************************************************
			PPP PUBLIC DEFINITIONS
************************************************************************/

/*
 * Self: from peer -> self, or self requests for self
 * Peer: from self -> peer, or peer requests for peer
 */
#define PPP_SELF		0
#define PPP_PEER		1

/*
 * PPP Endpoint ID's
 */
#define PPP_EID_CLASS_NULL	0
#define PPP_EID_CLASS_LOCAL	1
#define PPP_EID_CLASS_IP	2
#define PPP_EID_CLASS_MAC	3
#define PPP_EID_CLASS_MAGIC	4
#define PPP_EID_CLASS_E164	5
#define PPP_EID_CLASS_MAX	6

#define PPP_EID_MAXLEN		20

struct ppp_eid {
	u_char		class;
	u_char		length;
	u_char		value[PPP_EID_MAXLEN];
};

#endif	/* _PDEL_PPP_PPP_LIB_H_ */
