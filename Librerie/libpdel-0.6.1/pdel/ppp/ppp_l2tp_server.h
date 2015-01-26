
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PPP_L2TP_PDEL_PPP_PPP_L2TP_SERVER_H_
#define _PPP_L2TP_PDEL_PPP_PPP_L2TP_SERVER_H_

struct ppp_engine;
struct ppp_l2tp_peer;
struct ppp_channel;

#define L2TP_PORT		1701

/*
 * Callback for client to decide whether establishing a new L2TP
 * connection from ip:port is ok or not.
 *
 * Should return a non-NULL cookie to let the channel be established.
 * This argument becomes the 'carg' parameter to the 'plumb' and
 * 'destroy' methods below; the destroy method below is guaranteed
 * to be called exactly once if 'admit' returns a non-NULL cookie
 * (unless ppp_l2tp_server_close() is called first).
 *
 * The 'peer' may be used in a call to ppp_l2tp_server_close() to
 * close the associated L2TP connection before 'destroy' is called.
 * If so, 'destroy' will not be called. Calling ppp_l2tp_server_close()
 * after 'destroy' has been called will cause a crash.
 *
 * If desired, a name for this link (for logging purposes) may be
 * printed into the 'name' buffer, which has size 'nsize'.
 *
 * Note: we only allow one L2TP session to be established within
 * each control connection. So this will not be called if there is
 * already a connection with ip:port.
 */
typedef void	*ppp_l2tp_server_admit_t(void *arg, struct ppp_l2tp_peer *peer,
			struct in_addr ip, u_int16_t port,
			struct ppp_auth_config *auth, char *name, size_t nsize);

/*
 * Do any NAT mapping required to set up a reverse mapping from
 * our address at self_ip:self_port back to peer_ip:peer_port.
 * The original port we received the initial packet on is orig_port.
 *
 * This method is optional.
 */
typedef int	ppp_l2tp_server_natmap_t(void *arg,
			struct in_addr self_ip, u_int16_t self_port,
			u_int16_t orig_port, struct in_addr peer_ip,
			u_int16_t peer_port);

/*
 * Callback to tear down client state associated with a L2TP connection.
 *
 * After this function is called, "carg" will not be used again.
 */
typedef void	ppp_l2tp_server_destroy_t(void *arg, void *carg);

/*
 * Info supplied by client library to L2TP server.
 */
struct ppp_l2tp_server_info {
	void				*arg;
	const char			*vendor;
	ppp_l2tp_server_admit_t		*admit;
	ppp_l2tp_server_natmap_t	*natmap;
	ppp_l2tp_server_destroy_t	*destroy;
};

__BEGIN_DECLS

/* Functions */
extern int	ppp_l2tp_server_start(struct ppp_engine *engine,
			const struct ppp_l2tp_server_info *info,
			struct in_addr ip, u_int16_t port, u_int max_conn);
extern void	*ppp_l2tp_server_get_client_info(struct ppp_channel *chan);
extern void	ppp_l2tp_server_close(struct ppp_engine *engine,
			struct ppp_l2tp_peer **peerp);
extern void	ppp_l2tp_server_stop(struct ppp_engine *engine);

__END_DECLS

#endif	/* _PPP_L2TP_PDEL_PPP_PPP_L2TP_SERVER_H_ */
