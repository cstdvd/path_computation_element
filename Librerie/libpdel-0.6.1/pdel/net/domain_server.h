
/*
 * domain_server.h
 *
 * Spawn a server listening on a domain socket.  This is directly 
 * adapted from tcp_server.h.
 *
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 *         Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef _PDEL_IO_DOMAIN_SERVER_H_
#define _PDEL_IO_DOMAIN_SERVER_H_

#ifndef PDEL_BASE_INCLUDED
#include <pdel/pd_base.h>
#endif

struct sockaddr_un;
struct domain_server;
struct domain_connection;
struct pevent_ctx;

/*
 * Application handlers for a single connection. The setup method
 * should return a non-NULL cookie if successful, which will be
 * available from domain_server_get_conn_cookie().
 */
typedef void	*domain_setup_t(struct domain_connection *conn);
typedef void	domain_handler_t(struct domain_connection *conn);
typedef void	domain_teardown_t(struct domain_connection *conn);

__BEGIN_DECLS

/* Functions */
extern struct	domain_server *domain_server_start(struct pevent_ctx *ctx,
			void *cookie, const char *mtype,
			const char *path, mode_t mode, 
			uid_t owner, gid_t group,
			u_int max_conn, u_int conn_timeout,
			domain_setup_t *setup, domain_handler_t *handler,
			domain_teardown_t *teardown);
extern void	domain_server_stop(struct domain_server **servp);
extern void	*domain_server_get_cookie(struct domain_server *serv);

extern struct	domain_server *domain_connection_get_server(
			struct domain_connection *conn);
extern void	*domain_connection_get_cookie(struct domain_connection *conn);
extern int	domain_connection_get_fd(struct domain_connection *conn);
extern FILE	*domain_connection_get_fp(struct domain_connection *conn);
extern struct	domain_connection *domain_connection_first(struct domain_server *serv);
extern struct	domain_connection *domain_connection_next(
			struct domain_connection *conn);
extern void	domain_connection_get_peer(struct domain_connection *conn,
			struct sockaddr_un *sin);

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

#endif	/* _PDEL_IO_DOMAIN_SERVER_H_ */

