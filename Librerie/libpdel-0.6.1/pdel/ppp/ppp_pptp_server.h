
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_PPTP_SERVER_H_
#define _PDEL_PPP_PPP_PPTP_SERVER_H_

struct ppp_engine;
struct ppp_pptp_peer;
struct ppp_channel;

/*
 * Get the name of the remote peer of a PPTP control connection
 * for the purposes of display in the logs.
 *
 * This method is optional; default logname is "<IP-Addr>:<Port>".
 */
typedef void	ppp_pptp_server_getlogname_t(void *arg, struct in_addr ip,
			u_int16_t port, char *logname, size_t max);

/*
 * Callback for client to decide whether establishing a new PPTP
 * connection from ip:port is ok or not.
 *
 * Should return a non-NULL cookie to let the channel be established.
 * This argument becomes the 'carg' parameter to the 'plumb' and
 * 'destroy' methods below; the destroy method below is guaranteed
 * to be called exactly once if 'admit' returns a non-NULL cookie
 * (unless ppp_pptp_server_close() is called first).
 *
 * The 'peer' may be used in a call to ppp_pptp_server_close() to
 * close the associated PPTP connection before 'destroy' is called.
 * If so, 'destroy' will not be called. Calling ppp_pptp_server_close()
 * after 'destroy' has been called will cause a crash.
 *
 * If desired, a name for this link (for logging purposes) may be
 * printed into the 'name' buffer, which has size 'nsize'.
 *
 * Note: we only allow one PPTP channel to be established within
 * each control connection. So this will not be called if there is
 * already a connection with ip:port.
 */
typedef void	*ppp_pptp_server_admit_t(void *arg, struct ppp_pptp_peer *peer,
			struct in_addr ip, u_int16_t port,
			struct ppp_auth_config *auth, char *name, size_t nsize);

/*
 * Callback to connect the ng_pptpgre(4) netgraph node to whatever
 * node is going to handle the GRE packets.
 *
 * "path" points to the ng_pptpgre(4) node and "hook" is the name of
 * the ng_pptpgre(4) hook to connect (i.e., NG_PPTPGRE_HOOK_LOWER).
 *
 * "ips" points to the local and remote external PPTP tunnel IP addresses.
 */
typedef int	ppp_pptp_server_plumb_t(void *arg, void *carg,
			const char *path, const char *hook,
			const struct in_addr *ips);

/*
 * Callback to tear down client state associated with a PPTP connection.
 * All netgraph nodes, etc. should be removed.
 *
 * When this function is called, the ng_pptpgre(4) node will still exist
 * at "path". After this function returns, it is destroyed.
 *
 * After this function is called, "carg" will not be used again.
 */
typedef void	ppp_pptp_server_destroy_t(void *arg,
			void *carg, const char *path);

/*
 * Info supplied by client library to PPTP server.
 */
struct ppp_pptp_server_info {
	void				*arg;
	const char			*vendor;
	ppp_pptp_server_getlogname_t	*getlogname;
	ppp_pptp_server_admit_t		*admit;
	ppp_pptp_server_plumb_t		*plumb;
	ppp_pptp_server_destroy_t	*destroy;
};

__BEGIN_DECLS

/* Functions */
extern int	ppp_pptp_server_start(struct ppp_engine *engine,
			const struct ppp_pptp_server_info *info,
			struct in_addr ip, u_int16_t port, u_int max_conn);
extern void	*ppp_pptp_server_get_client_info(struct ppp_channel *chan);
extern void	ppp_pptp_server_close(struct ppp_engine *engine,
			struct ppp_pptp_peer **peerp);
extern void	ppp_pptp_server_stop(struct ppp_engine *engine);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_PPTP_SERVER_H_ */
