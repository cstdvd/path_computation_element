
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "io/ssl_fp.h"
#include "util/typed_mem.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_internal.h"

#define HTTP_CLIENT_TIMEOUT	90	/* timeout in seconds */

/* Cleanup state for http_client_connect() */
struct http_client_connect_state {
	int	sock;
};

/* HTTP client */
struct http_client {
	char			*user_agent;	/* client user agent name */
	SSL_CTX			*ssl;		/* ssl context */
	http_logger_t		*logger;	/* error logging routine */
	struct http_connection_cache
				*cache;		/* cached connections */
	u_int			max_conn;	/* max number of connections */
	pthread_mutex_t		mutex;		/* mutex for "condvar" */
	pthread_cond_t		condvar;	/* connection condition var */
	u_int			num_conn;	/* number active connections */
	TAILQ_HEAD(,http_client_connection)
				list;		/* active connections */
};

/* HTTP client connection */
struct http_client_connection {
	struct http_client	*client;	/* client who owns me */
	struct sockaddr_in	peer;		/* peer address */
	struct http_connection	*conn;		/* connection to server */
	char			*reason;	/* reason for failure */
	u_char			got_response;	/* response read from server */
	TAILQ_ENTRY(http_client_connection)
				next;		/* next in client list */
};

/* Internal functions */
static void	http_client_connect_cleanup(void *arg);

/*********************************************************************
			HTTP_CLIENT FUNCTIONS
*********************************************************************/

/*
 * Create a new client.
 */
struct http_client *
http_client_create(struct pevent_ctx *ctx, const char *user_agent,
	u_int max_conn, u_int max_cache, u_int max_cache_idle,
	http_logger_t *logger)
{
	struct http_client *client;
	int got_mutex = 0;
	int got_cond = 0;

	/* Sanity check */
	if (max_conn <= max_cache) {
		errno = EINVAL;
		return (NULL);
	}

	/* Create new client */
	if ((client = MALLOC("http_client", sizeof(*client))) == NULL)
		return (NULL);
	memset(client, 0, sizeof(*client));
	client->logger = logger;
	client->max_conn = max_conn;
	TAILQ_INIT(&client->list);

	/* Copy user agent */
	if ((client->user_agent = STRDUP("http_client.user_agent",
	    user_agent)) == NULL)
		goto fail;

	/* Initialize connection cache */
	if ((client->cache = _http_connection_cache_create(
	    ctx, max_cache, max_cache_idle)) == NULL)
		goto fail;

	/* Initialize mutex */
	if ((errno = pthread_mutex_init(&client->mutex, NULL)) != 0)
		goto fail;
	got_mutex = 1;

	/* Initialize condition variable */
	if ((errno = pthread_cond_init(&client->condvar, NULL)) != 0)
		goto fail;
	got_cond = 1;

	/* Done */
	return (client);

fail:
	/* Clean up after failure */
	if (got_cond)
		pthread_cond_destroy(&client->condvar);
	if (got_mutex)
		pthread_mutex_destroy(&client->mutex);
	_http_connection_cache_destroy(&client->cache);
	FREE("http_client.user_agent", client->user_agent);
	return (NULL);
}

/*
 * Destroy an HTTP client.
 *
 * This will return -1 with errno = EBUSY if there are any
 * associated connections still active.
 */
int
http_client_destroy(struct http_client **clientp)
{
	struct http_client *client = *clientp;
	int r;

	/* Sanity */
	if (client == NULL)
		return (0);

	/* Acquire mutex */
	r = pthread_mutex_lock(&client->mutex);
	assert(r == 0);

	/* Check for active connections */
	if (client->num_conn != 0) {
		r = pthread_mutex_unlock(&client->mutex);
		assert(r == 0);
		errno = EBUSY;
		return (-1);
	}

	/* Shut it down */
	if (client->ssl != NULL)
		SSL_CTX_free(client->ssl);
	_http_connection_cache_destroy(&client->cache);
	r = pthread_mutex_unlock(&client->mutex);
	assert(r == 0);
	pthread_cond_destroy(&client->condvar);
	pthread_mutex_destroy(&client->mutex);
	FREE("http_client.user_agent", client->user_agent);
	FREE("http_client", client);
	*clientp = NULL;
	return (0);
}

/*********************************************************************
		HTTP_CLIENT_CONNECTION FUNCTIONS
*********************************************************************/

/*
 * Create a new HTTP connection object from this client.
 */
struct http_client_connection *
http_client_connect(struct http_client *client,
	struct in_addr ip, u_int16_t port, int https)
{
	struct http_client_connection *cc = NULL;
	struct http_client_connect_state state;
	struct sockaddr_in sin;
	FILE *fp = NULL;
	int sock = -1;
	int ret;
	int r;

	/* Acquire mutex, release if canceled */
	r = pthread_mutex_lock(&client->mutex);
	assert(r == 0);
	pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock,
	    &client->mutex);

	/* Initialize SSL context for this client if not already done */
	if (https && client->ssl == NULL) {

		/* Initialize SSL stuff */
		_http_ssl_init();

		/* Initialize new SSL context for this client */
		if ((client->ssl = SSL_CTX_new(
		    SSLv23_client_method())) == NULL) {
			ssl_log(NULL, NULL);
			goto fail;
		}
	}

	/* Set up peer address */
	memset(&sin, 0, sizeof(sin));
#ifdef HAVE_SIN_LEN
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_addr = ip;
	sin.sin_port = htons(port);

	/* See if we have a cached connection to this server */
	if (client->cache != NULL
	    && _http_connection_cache_get(client->cache, &sin,
	      https ? client->ssl : NULL, &fp, &sock) == 0)
		goto connected;

	/* Wait if too many connections already exist */
	while (client->num_conn >= client->max_conn)
		pthread_cond_wait(&client->condvar, &client->mutex);

	/* Get socket */
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		(*client->logger)(LOG_ERR,
		    "%s: %s", "socket", strerror(errno));
		goto fail;
	}

	/* Release mutex */
	r = pthread_mutex_unlock(&client->mutex);
	assert(r == 0);

	/* Don't leak socket if thread is canceled */
	state.sock = sock;
	pthread_cleanup_push(http_client_connect_cleanup, &state);

	/* Connect to peer */
	ret = connect(sock, (struct sockaddr *)&sin, sizeof(sin));

	/* Remove cleanup hook */
	pthread_cleanup_pop(0);

	/* Acquire mutex */
	r = pthread_mutex_lock(&client->mutex);
	assert(r == 0);

	/* Check if connected */
	if (ret == -1) {
		(*client->logger)(LOG_ERR, "failed to connect to %s:%u: %s",
		    inet_ntoa(ip), port, strerror(errno));
		goto fail;
	}

connected:
	/* Create new client connection object */
	if ((cc = MALLOC("http_client_connection", sizeof(*cc))) == NULL)
		goto fail;
	memset(cc, 0, sizeof(*cc));
	cc->client = client;
	cc->peer = sin;

	/* Create new connection */
	if ((cc->conn = _http_connection_create(fp, sock, 0, ip, port,
	    client->ssl, client->logger, HTTP_CLIENT_TIMEOUT)) == NULL)
		goto fail;
	fp = NULL;
	sock = -1;

	/* Try to do keep-alive */
	cc->conn->keep_alive = 1;

	/* Add to client connection list */
	TAILQ_INSERT_TAIL(&client->list, cc, next);
	client->num_conn++;

	/* Set some default request headers */
	if (http_request_set_header(cc->conn->req, 0,
	      HTTP_HEADER_USER_AGENT, "%s", client->user_agent) == -1
	    || http_request_set_header(cc->conn->req, 0,
	      HTTP_HEADER_HOST, "%s:%u", inet_ntoa(ip), port) == -1
	    || http_request_set_header(cc->conn->req, 0,
	      HTTP_HEADER_ACCEPT, "*/*") == -1
	    || http_request_set_header(cc->conn->req, 0,
	      HTTP_HEADER_ACCEPT_CHARSET, "iso-8859-1") == -1
	    || http_request_set_header(cc->conn->req, 0,
	      HTTP_HEADER_ACCEPT_ENCODING, "identity") == -1)
		goto fail;

	/* Done */
	goto done;

fail:
	/* Cleanup after failure */
	if (cc != NULL) {
		if (cc->conn != NULL)
			_http_connection_free(&cc->conn);
		FREE("http_client_connection", cc);
		cc = NULL;
	}
	if (fp != NULL)
		fclose(fp);
	else if (sock != -1)
		(void)pd_close(sock);

done:;
	/* Done */
	pthread_cleanup_pop(1);
	return (cc);
}

/*
 * Get local IP address.
 */
struct in_addr
http_client_get_local_ip(struct http_client_connection *cc)
{
	return (cc->conn->local_ip);
}

/*
 * Get local port.
 */
u_int16_t
http_client_get_local_port(struct http_client_connection *cc)
{
	return (cc->conn->local_port);
}

/*
 * Get request associated with client.
 */
struct http_request *
http_client_get_request(struct http_client_connection *cc)
{
	return (cc->conn->req);
}

/*
 * Get response associated with client connection.
 */
struct http_response *
http_client_get_response(struct http_client_connection *cc)
{
	struct http_request *const req = cc->conn->req;
	struct http_response *resp = cc->conn->resp;
	char buf[128];

	/* Already got it? */
	if (cc->got_response)
		return (resp);

	/* Send request headers (if not sent already) */
	if (http_request_send_headers(req) == -1) {
		snprintf(buf, sizeof(buf), "Error sending request: %s",
		    strerror(errno));
		return (NULL);
	}

	/* Send back body (it was buffered) */
	_http_message_send_body(req->msg);

	/* Read response from server */
	cc->got_response = 1;
	if (_http_response_read(cc->conn, buf, sizeof(buf)) == -1)
		resp = NULL;

	/* Save reason message */
	cc->reason = STRDUP("http_client_connection.reason", buf);
	return (resp);
}

/*
 * Close a client connection. The connection is cached if appropriate.
 */
void
http_client_close(struct http_client_connection **ccp)
{
	struct http_client_connection *const cc = *ccp;
	struct http_connection *conn;
	struct http_client *client;
	struct http_response *resp;
	int r;

	/* Sanity */
	if (cc == NULL)
		return;
	*ccp = NULL;

	/* Get client and connection */
	client = cc->client;
	conn = cc->conn;

	/* Acquire client mutex */
	r = pthread_mutex_lock(&client->mutex);
	assert(r == 0);

	/* Cache connection socket if appropriate */
	if (conn->keep_alive
	    && client->cache != NULL
	    && (resp = http_client_get_response(cc)) != NULL
	    && _http_head_want_keepalive(resp->msg->head)
	    && _http_connection_cache_put(client->cache,
	      &cc->peer, client->ssl, conn->fp, conn->sock) == 0) {
		conn->fp = NULL;
		conn->sock = -1;
	}

	/* Remove from active connection list and wakeup next waiting thread */
	TAILQ_REMOVE(&client->list, cc, next);
	if (client->num_conn-- == client->max_conn)
		pthread_cond_signal(&client->condvar);

	/* Release client mutex */
	r = pthread_mutex_unlock(&client->mutex);
	assert(r == 0);

	/* Shutdown this connection */
	_http_connection_free(&cc->conn);
	FREE("http_client_connection.reason", cc->reason);
	FREE("http_client_connection", cc);
}

/*
 * Get response error/reason string.
 */
const char *
http_client_get_reason(struct http_client_connection *cc)
{
	if (cc->reason == NULL)
		return ("Uknown error");
	return (cc->reason);
}

/*********************************************************************
			INTERNAL FUNCTIONS
*********************************************************************/

/*
 * Cleanup for http_client_connect() if thread is canceled.
 */
static void
http_client_connect_cleanup(void *arg)
{
	const struct http_client_connect_state *const state = arg;

	pd_close(state->sock);
}

