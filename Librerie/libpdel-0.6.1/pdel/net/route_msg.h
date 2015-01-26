
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_NET_ROUTE_MSG_H_
#define _PDEL_NET_ROUTE_MSG_H_

#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

struct route_msg;

__BEGIN_DECLS

extern struct	route_msg *route_msg_create(void);
extern void	route_msg_destroy(struct route_msg **msgp);
extern int	route_msg_get_type(struct route_msg *msg);
extern void	route_msg_set_type(struct route_msg *msg, int type);
extern int	route_msg_get_index(struct route_msg *msg);
extern void	route_msg_set_index(struct route_msg *msg, int index);
extern int	route_msg_get_flags(struct route_msg *msg);
extern void	route_msg_set_flags(struct route_msg *msg, int flags);
extern int	route_msg_get_error(struct route_msg *msg);
extern pid_t	route_msg_get_pid(struct route_msg *msg);
extern int	route_msg_get_seq(struct route_msg *msg);
extern const	struct sockaddr *route_msg_get_dest(struct route_msg *msg);
extern int	route_msg_set_dest(struct route_msg *msg,
			const struct sockaddr *dest);
extern const	struct sockaddr *route_msg_get_gateway(struct route_msg *msg);
extern int	route_msg_set_gateway(struct route_msg *msg,
			const struct sockaddr *gateway);
extern const	struct sockaddr *route_msg_get_netmask(struct route_msg *msg);
extern int	route_msg_set_netmask(struct route_msg *msg,
			const struct sockaddr *netmask);
extern const	struct sockaddr *route_msg_get_genmask(struct route_msg *msg);
extern int	route_msg_set_genmask(struct route_msg *msg,
			const struct sockaddr *genmask);
extern const	struct sockaddr *route_msg_get_ifp(struct route_msg *msg);
extern int	route_msg_set_ifp(struct route_msg *msg,
			const struct sockaddr *ifp);
extern const	struct sockaddr *route_msg_get_ifa(struct route_msg *msg);
extern int	route_msg_set_ifa(struct route_msg *msg,
			const struct sockaddr *ifa);
extern const	struct sockaddr *route_msg_get_author(struct route_msg *msg);
extern int	route_msg_set_author(struct route_msg *msg,
			const struct sockaddr *author);
extern const	struct sockaddr *route_msg_get_brd(struct route_msg *msg);
extern int	route_msg_set_brd(struct route_msg *msg,
			const struct sockaddr *brd);
extern int	route_msg_decode(const u_char *data, size_t dlen,
			struct route_msg ***listp, const char *mtype);
extern int	route_msg_encode(const struct route_msg *msg,
			u_char *data, size_t dlen);
extern void	route_msg_print(struct route_msg *msg, FILE *fp);
extern int	route_msg_send(struct route_msg *msg, int sock);
extern int	route_msg_recv(struct route_msg ***listp,
			int sock, const char *mtype);

__END_DECLS

#endif	/* _PDEL_NET_ROUTE_MSG_H_ */

