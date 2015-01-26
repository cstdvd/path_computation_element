
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif
#include "pdel/pd_io.h"
#include "pdel/pd_thread.h"

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/pevent.h"
#include "util/typed_mem.h"
#include "io/timeout_fp.h"
#include "net/tcp_server.h"
#include "sys/alog.h"

/* How long to pause when we reach max # connections */
#define TCP_SERVER_PAUSE	250		/* 0.25 sec */

/* Server state */
struct tcp_server {
	struct pevent_ctx	*ctx;		/* event context */
	struct pevent		*conn_event;	/* incoming connection event */
	struct pevent		*wait_event;	/* pause timeout event */
	struct sockaddr_in	addr;		/* server bound address */
	pthread_mutex_t		mutex;		/* server mutex */
	u_int			num_conn;	/* # connections */
	u_int			max_conn;	/* max # connections */
	u_int			conn_timeout;	/* timeout for connections */
	int			sock;		/* listening socket */
	TAILQ_HEAD(, tcp_connection) conn_list;	/* connection list */
	void			*cookie;	/* application private data */
	tcp_setup_t		*setup;		/* connection setup handler */
	tcp_handler_t		*handler;	/* connection handler handler */
	tcp_teardown_t		*teardown;	/* connection teardown handlr */
	const char		*mtype;		/* typed memory type string */
	char			mtype_buf[TYPED_MEM_TYPELEN];
};

/* Connection state */
struct tcp_connection {
	pthread_t		tid;		/* connection thread */
	struct tcp_server	*server;	/* associated server */
	struct sockaddr_in	peer;		/* remote side address */
	u_char			started;	/* thread has started */
	u_char			destruct;	/* object needs teardown */
	int			sock;		/* connection socket */
	FILE			*fp;		/* connection stream (unbuf) */
	TAILQ_ENTRY(tcp_connection) next;	/* next in connection list */
	void			*cookie;	/* application private data */
};

/* Internal functions */
static void	*tcp_server_connection_main(void *arg);
static void	tcp_server_connection_cleanup(void *arg);

static pevent_handler_t	tcp_server_accept;
static pevent_handler_t	tcp_server_restart;

/*
 * Start a new TCP server
 */
struct tcp_server *
tcp_server_start(struct pevent_ctx *ctx, void *cookie, const char *mtype,
	struct in_addr ip, u_int16_t port, u_int max_conn, u_int conn_timeout,
	tcp_setup_t *setup, tcp_handler_t *handler, tcp_teardown_t *teardown)
{
	static const int one = 1;
	struct tcp_server *serv = NULL;

	/* Get new object */
	if ((serv = MALLOC(mtype, sizeof(*serv))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		goto fail;
	}
	memset(serv, 0, sizeof(*serv));
	serv->ctx = ctx;
	serv->cookie = cookie;
	serv->sock = -1;
	serv->max_conn = max_conn;
	serv->conn_timeout = conn_timeout;
	serv->setup = setup;
	serv->handler = handler;
	serv->teardown = teardown;
	TAILQ_INIT(&serv->conn_list);
	if (mtype != NULL) {
		strlcpy(serv->mtype_buf, mtype, sizeof(serv->mtype_buf));
		serv->mtype = serv->mtype_buf;
	}

	/* Create and bind socket */
	if ((serv->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		alogf(LOG_ERR, "%s: %m", "socket");
		goto fail;
	}
#ifdef F_SETFD
	(void)fcntl(serv->sock, F_SETFD, 1);
#endif
	if (setsockopt(serv->sock, SOL_SOCKET,
	    SO_REUSEADDR, (char *)&one, sizeof(one)) == -1) {
		alogf(LOG_ERR, "%s: %m", "setsockopt");
		goto fail;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(serv->sock, SOL_SOCKET,
	    SO_REUSEPORT, (char *)&one, sizeof(one)) == -1) {
		alogf(LOG_ERR, "%s: %m", "setsockopt");
		goto fail;
	}
#endif
	memset(&serv->addr, 0, sizeof(serv->addr));
#ifdef HAVE_SIN_LEN
	serv->addr.sin_len = sizeof(serv->addr);
#endif
	serv->addr.sin_family = AF_INET;
	serv->addr.sin_port = htons(port);
	serv->addr.sin_addr = ip;
	if (bind(serv->sock,
	    (struct sockaddr *)&serv->addr, sizeof(serv->addr)) == -1) {
		alogf(LOG_ERR, "%s: %m", "bind");
		goto fail;
	}
	if (listen(serv->sock, 1024) == -1) {
		alogf(LOG_ERR, "%s: %m", "listen");
		goto fail;
	}

	/* Accept incoming connections */
	if (pevent_register(serv->ctx, &serv->conn_event, PEVENT_RECURRING,
	      &serv->mutex, tcp_server_accept, serv, PEVENT_READ, serv->sock)
	    == -1) {
		alogf(LOG_ERR, "%s: %m", "pevent_register");
		goto fail;
	}

	/* Create mutex */
	if ((errno = pthread_mutex_init(&serv->mutex, NULL)) != 0) {
		alogf(LOG_ERR, "%s: %m", "pthread_mutex_init");
		goto fail;
	}

	/* Done */
	return (serv);

fail:
	/* Clean up and return error */
	if (serv != NULL) {
		pevent_unregister(&serv->conn_event);
		if (serv->sock != -1)
			(void)pd_close(serv->sock);
		FREE(serv->mtype, serv);
	}
	return (NULL);
}

/*
 * Stop a TCP server
 */
void
tcp_server_stop(struct tcp_server **servp)
{
	struct tcp_server *const serv = *servp;
	struct tcp_connection *conn;
	int r;

	/* Sanity */
	if (serv == NULL)
		return;
	*servp = NULL;

	/* Acquire mutex */
	r = pthread_mutex_lock(&serv->mutex);
	assert(r == 0);

	/* Stop accepting new connections */
	pevent_unregister(&serv->conn_event);
	pevent_unregister(&serv->wait_event);

	/* Close listen socket */
	(void)pd_close(serv->sock);
	serv->sock = -1;

	/* Kill all outstanding connections */
	while (!TAILQ_EMPTY(&serv->conn_list)) {

		/* Kill active connections; they will clean up themselves */
		TAILQ_FOREACH(conn, &serv->conn_list, next) {
			if (conn->started && !pd_pthread_isnull(conn->tid)) {
				pthread_cancel(conn->tid);
				/* don't cancel twice */
				conn->tid = pd_null_pthread;
			}
		}

		/* Wait for outstanding connections to complete */
		r = pthread_mutex_unlock(&serv->mutex);
		assert(r == 0);
		usleep(100000);
		r = pthread_mutex_lock(&serv->mutex);
		assert(r == 0);
	}

	/* Free server structure */
	r = pthread_mutex_unlock(&serv->mutex);
	assert(r == 0);
	pthread_mutex_destroy(&serv->mutex);
	FREE(serv->mtype, serv);
}

/*
 * Get connection server.
 */
struct tcp_server *
tcp_connection_get_server(struct tcp_connection *conn)
{
	return (conn->server);
}


/*
 * Get server cookie.
 */
void *
tcp_server_get_cookie(struct tcp_server *serv)
{
	return (serv->cookie);
}

/*
 * Get connection cookie.
 */
void *
tcp_connection_get_cookie(struct tcp_connection *conn)
{
	return (conn->cookie);
}

/*
 * Get connection file descriptor.
 */
int
tcp_connection_get_fd(struct tcp_connection *conn)
{
	return (conn->sock);
}

/*
 * Get connection file stream.
 */
FILE *
tcp_connection_get_fp(struct tcp_connection *conn)
{
	return (conn->fp);
}

/*
 * Get peer's address.
 */
void
tcp_connection_get_peer(struct tcp_connection *conn, struct sockaddr_in *sin)
{
	memcpy(sin, &conn->peer, sizeof(*sin));
}

/*********************************************************************
		    NEW CONNECTION ACCEPTOR
*********************************************************************/

/*
 * Accept a new incoming connection.
 *
 * This will be called with the server mutex acquired.
 */
static void
tcp_server_accept(void *arg)
{
	struct tcp_server *const serv = arg;
	struct tcp_connection *conn;
	socklen_t slen = sizeof(conn->peer);
	struct sockaddr_in sin;
	int sock;

	/* If maximum number of connections reached, pause a while */
	if (serv->max_conn > 0 && serv->num_conn >= serv->max_conn) {
		pevent_unregister(&serv->wait_event);
		pevent_unregister(&serv->conn_event);
		if (pevent_register(serv->ctx, &serv->wait_event, 0,
		    &serv->mutex, tcp_server_restart, serv, PEVENT_TIME,
		    TCP_SERVER_PAUSE) == -1)
			alogf(LOG_ERR, "%s: %m", "pevent_register");
		return;
	}

	/* Accept next connection */
	if ((sock = accept(serv->sock, (struct sockaddr *)&sin, &slen)) == -1) {
		if (errno != ECONNABORTED && errno != ENOTCONN)
			alogf(LOG_ERR, "%s: %m", "accept");
		return;
	}
#ifdef F_SETFD
	(void)fcntl(sock, F_SETFD, 1);
#endif

	/* Create connection state structure */
	if ((conn = MALLOC(serv->mtype, sizeof(*conn))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		(void)pd_close(sock);
		return;
	}
	memset(conn, 0, sizeof(*conn));
	conn->server = serv;
	conn->sock = sock;
	conn->peer = sin;

	/* Put stream on top of file descriptor */
	if ((conn->fp = timeout_fdopen(conn->sock,
	    "r+", serv->conn_timeout)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "timeout_fdopen");
		(void)pd_close(conn->sock);
		FREE(serv->mtype, conn);
		return;
	}
	setbuf(conn->fp, NULL);

	/* Spawn connection thread */
	if ((errno = pthread_create(&conn->tid, NULL,
	    tcp_server_connection_main, conn)) != 0) {
		conn->tid = pd_null_pthread;
		alogf(LOG_ERR, "%s: %m", "pthread_create");
		fclose(conn->fp);
		FREE(serv->mtype, conn);
		return;
	}

	/* Detach thread */
	pthread_detach(conn->tid);

	/* Add connection to list */
	TAILQ_INSERT_TAIL(&serv->conn_list, conn, next);
	serv->num_conn++;
}

/*
 * Start accepting new connections after waiting a while.
 *
 * This will be called with the server mutex acquired.
 */
static void
tcp_server_restart(void *arg)
{
	struct tcp_server *const serv = arg;

	/* Accept incoming connections again */
	pevent_unregister(&serv->wait_event);
	pevent_unregister(&serv->conn_event);
	if (pevent_register(serv->ctx, &serv->conn_event, 0,
	    &serv->mutex, tcp_server_accept, serv, PEVENT_READ,
	    serv->sock) == -1)
		alogf(LOG_ERR, "%s: %m", "pevent_register");
}

/*********************************************************************
		    TCP CONNECTION THREAD
*********************************************************************/

/*
 * Connection thread main entry point.
 */
static void *
tcp_server_connection_main(void *arg)
{
	struct tcp_connection *const conn = arg;
	struct tcp_server *const serv = conn->server;

	/* Push cleanup hook */
	pthread_cleanup_push(tcp_server_connection_cleanup, conn);
	conn->started = 1;			/* now it's ok to cancel me */

	/* Call application's setup routine */
	if (serv->setup != NULL
	    && (conn->cookie = (*serv->setup)(conn)) == NULL)
		goto done;
	conn->destruct = 1;

	/* Invoke application handler */
	(*serv->handler)(conn);

done:;
	/* Done */
	pthread_cleanup_pop(1);
	return (NULL);
}

/*
 * Cleanup routine for tcp_server_connection_main().
 */
static void
tcp_server_connection_cleanup(void *arg)
{
	struct tcp_connection *const conn = arg;
	struct tcp_server *const serv = conn->server;
	int r;

	/* Call application destructor */
	if (conn->destruct && serv->teardown != NULL) {
		conn->destruct = 0;
		(*serv->teardown)(conn);
	}

	/* Close connection */
	if (conn->fp != NULL) {
		(void)fclose(conn->fp);
		conn->sock = -1;
	}

	/* Unlink from server list */
	r = pthread_mutex_lock(&serv->mutex);
	assert(r == 0);
	serv->num_conn--;
	TAILQ_REMOVE(&serv->conn_list, conn, next);
	r = pthread_mutex_unlock(&serv->mutex);
	assert(r == 0);

	/* Release connection object */
	FREE(serv->mtype, conn);
}

