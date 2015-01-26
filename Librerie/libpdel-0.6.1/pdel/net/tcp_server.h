
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_TCP_SERVER_H_
#define _PDEL_IO_TCP_SERVER_H_

struct tcp_server;
struct tcp_connection;
struct pevent_ctx;

/*
 * Application handlers for a single connection. The setup method
 * should return a non-NULL cookie if successful, which will be
 * available from tcp_server_get_conn_cookie().
 */
typedef void	*tcp_setup_t(struct tcp_connection *conn);
typedef void	tcp_handler_t(struct tcp_connection *conn);
typedef void	tcp_teardown_t(struct tcp_connection *conn);

__BEGIN_DECLS

/* Functions */
extern struct	tcp_server *tcp_server_start(struct pevent_ctx *ctx,
			void *cookie, const char *mtype, struct in_addr ip,
			u_int16_t port, u_int max_conn, u_int conn_timeout,
			tcp_setup_t *setup, tcp_handler_t *handler,
			tcp_teardown_t *teardown);
extern void	tcp_server_stop(struct tcp_server **servp);
extern void	*tcp_server_get_cookie(struct tcp_server *serv);

extern struct	tcp_server *tcp_connection_get_server(
			struct tcp_connection *conn);
extern void	*tcp_connection_get_cookie(struct tcp_connection *conn);
extern int	tcp_connection_get_fd(struct tcp_connection *conn);
extern FILE	*tcp_connection_get_fp(struct tcp_connection *conn);
extern struct	tcp_connection *tcp_connection_first(struct tcp_server *serv);
extern struct	tcp_connection *tcp_connection_next(
			struct tcp_connection *conn);
extern void	tcp_connection_get_peer(struct tcp_connection *conn,
			struct sockaddr_in *sin);

__END_DECLS


#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

#endif	/* _PDEL_IO_TCP_SERVER_H_ */

