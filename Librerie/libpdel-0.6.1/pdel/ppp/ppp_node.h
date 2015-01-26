
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_NODE_H_
#define _PDEL_PPP_PPP_NODE_H_

struct ppp_node;
struct ppp_log;

typedef void	ppp_node_recv_t(void *arg, u_int link_num, u_int16_t proto,
			u_char *data, size_t len);

typedef void	ppp_node_recvmsg_t(void *arg, struct ng_mesg *msg);

__BEGIN_DECLS

/* Functions */
extern struct	ppp_node *ppp_node_create(struct pevent_ctx *ev_ctx,
			pthread_mutex_t *mutex, struct ppp_log *log);
extern void	ppp_node_destroy(struct ppp_node **nodep);
extern int	ppp_node_write(struct ppp_node *node, u_int link_num,
			u_int16_t proto, const void *data, size_t len);
extern int	ppp_node_connect(struct ppp_node *node, u_int link_num,
			const char *path, const char *hook);
extern int	ppp_node_disconnect(struct ppp_node *node, u_int link_num);

extern const	char *ppp_node_get_path(struct ppp_node *node);
extern int	ppp_node_get_config(struct ppp_node *node,
			struct ng_ppp_node_conf *conf);
extern int	ppp_node_set_config(struct ppp_node *node,
			const struct ng_ppp_node_conf *conf);
extern int	ppp_node_send_msg(struct ppp_node *node,
			const char *relpath, u_int32_t cookie, u_int32_t cmd,
			const void *payload, size_t plen);
extern int	ppp_node_recv_msg(struct ppp_node *node,
			struct ng_mesg *msg, size_t mlen, char *raddr);
extern void	ppp_node_set_recv(struct ppp_node *node,
			ppp_node_recv_t *recv, void *arg);
extern int	ppp_node_set_recvmsg(struct ppp_node *node,
			u_int32_t cookie, u_int32_t cmd,
			ppp_node_recvmsg_t *recvmsg, void *arg);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_NODE_H_ */
