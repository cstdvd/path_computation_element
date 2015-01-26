
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>

#include <openssl/ssl.h>

#include "pdel/pd_poll.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "sys/alog.h"
#include "util/pevent.h"
#include "util/typed_mem.h"

#include "http/http_server.h"
#include "http/http_internal.h"

#define MEM_TYPE_CACHE	"http_connection_cache"
#define MEM_TYPE_CONN	"http_connection_cache.connection"

/* Connection cache structure */
struct http_connection_cache {
	struct pevent_ctx		*ctx;		/* event context */
	u_int				max_num;	/* max # in cache */
	u_int				max_idle;	/* max socket idle */
	u_int				num;		/* # in the cache now */
	struct pevent			*timer;		/* exipiration timer */
	pthread_mutex_t			mutex;		/* mutex */
	TAILQ_HEAD(, cached_connection)	list;		/* connection list */
};

/* Cached connection structure */
struct cached_connection {
	struct sockaddr_in		peer;	/* peer ip and port */
	const SSL_CTX			*ssl;	/* ssl context */
	time_t				expiry;	/* when connection expires */
	int				sock;	/* connected tcp socket */
	FILE				*fp;	/* connected tcp stream */
	TAILQ_ENTRY(cached_connection)	next;	/* next in list */
};

/* Internal functions */
static void	http_connection_cache_extract(
			struct http_connection_cache *cache,
			struct cached_connection *conn, FILE **fpp, int *sockp);
static void	http_connection_cache_start_timer(
			struct http_connection_cache *cache);
static void	http_connection_cache_timeout(void *arg);
static int	http_connection_cache_check(int sock);

/*********************************************************************
			PUBLIC API FUNCTIONS
*********************************************************************/

/*
 * Create a new connection cache.
 */
struct http_connection_cache *
_http_connection_cache_create(struct pevent_ctx *ctx,
	u_int max_num, u_int max_idle)
{
	struct http_connection_cache *cache;

	/* Sanity check */
	if (max_num == 0 || max_idle == 0) {
		errno = EINVAL;
		return (NULL);
	}

	/* Create new cache */
	if ((cache = MALLOC(MEM_TYPE_CACHE, sizeof(*cache))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
	    	return (NULL);
	}

	/* Initialize it */
	memset(cache, 0, sizeof(*cache));
	TAILQ_INIT(&cache->list);
	cache->ctx = ctx;
	cache->max_num = max_num;
	cache->max_idle = max_idle;

	/* Initialize mutex */
	if ((errno = pthread_mutex_init(&cache->mutex, NULL)) != 0) {
		alogf(LOG_ERR, "%s: %m", "pthread_mutex_init");
		FREE(MEM_TYPE_CACHE, cache);
	    	return (NULL);
	}

	/* Done */
	return (cache);
}

/*
 * Destroy a connection cache.
 */
void
_http_connection_cache_destroy(struct http_connection_cache **cachep)
{
	struct http_connection_cache *const cache = *cachep;
	int r;

	/* Sanity */
	if (cache == NULL)
		return;
	*cachep = NULL;

	/* Lock cache */
	r = pthread_mutex_lock(&cache->mutex);
	assert(r == 0);

	/* Close all cached connections */
	while (!TAILQ_EMPTY(&cache->list)) {
		http_connection_cache_extract(cache,
		    TAILQ_FIRST(&cache->list), NULL, NULL);
	}

	/* Cleanup */
	r = pthread_mutex_unlock(&cache->mutex);
	assert(r == 0);
	pthread_mutex_destroy(&cache->mutex);
	FREE(MEM_TYPE_CACHE, cache);
}

/*
 * Get a connection item from the connection cache.
 *
 * Returns 0 and sets *fp and *sock if found (and the connection
 * is removed from the cache), else -1 and errno will be ENOENT.
 */
int
_http_connection_cache_get(struct http_connection_cache *cache,
	const struct sockaddr_in *peer, const SSL_CTX *ssl,
	FILE **fpp, int *sockp)
{
	struct cached_connection *conn;
	struct cached_connection *next;
	int r;

	/* Cache enabled? */
	if (cache == NULL)
		goto not_found;
	DBG(HTTP_CONNECTION_CACHE, "looking in cache (%d entries)"
	    " for %s:%u, ssl=%p", cache->num _ inet_ntoa(peer->sin_addr) _
	   ntohs(peer->sin_port) _ ssl);

	/* Lock cache */
	r = pthread_mutex_lock(&cache->mutex);
	assert(r == 0);

	/* Find oldest matching connection */
	for (conn = TAILQ_FIRST(&cache->list); conn != NULL; conn = next) {
		FILE *fp;
		int sock;

		/* Get next element in list */
		next = TAILQ_NEXT(conn, next);

		/* See if this element matches */
		if (conn->peer.sin_addr.s_addr != peer->sin_addr.s_addr
		    || conn->peer.sin_port != peer->sin_port
		    || conn->ssl != ssl)
			continue;

		/* Remove element from list */
		http_connection_cache_extract(cache, conn, &fp, &sock);

		/* If connection is no longer valid, close and skip it */
		if (!http_connection_cache_check(sock)) {
			DBG(HTTP_CONNECTION_CACHE, "old connection fp=%p"
			    " for %s:%u is no longer valid", fp _
			    inet_ntoa(peer->sin_addr) _ ntohs(peer->sin_port));
			fclose(fp);
			continue;
		}
		DBG(HTTP_CONNECTION_CACHE, "found connection fp=%p for %s:%u",_
		    fp _ inet_ntoa(peer->sin_addr) _ ntohs(peer->sin_port));

		/* Return it */
		*fpp = fp;
		*sockp = sock;

		/* Done */
		r = pthread_mutex_unlock(&cache->mutex);
		assert(r == 0);
		return (0);
	}

	/* Unlock cache */
	r = pthread_mutex_unlock(&cache->mutex);
	assert(r == 0);

not_found:
	DBG(HTTP_CONNECTION_CACHE, "nothing found for %s:%u, ssl=%p",_
	    inet_ntoa(peer->sin_addr) _ ntohs(peer->sin_port) _ ssl);
	errno = ENOENT;
	return (-1);
}

/*
 * Store a connection in the cache. It is assumed that
 * calling 'fclose(fp)' implicitly closes 'sock' as well.
 *
 * Returns zero if stored, else -1 and sets errno (in which case
 * caller is responsible for dealing with 'fp' and 'sock').
 */
int
_http_connection_cache_put(struct http_connection_cache *cache,
	const struct sockaddr_in *peer, const SSL_CTX *ssl, FILE *fp, int sock)
{
	struct cached_connection *conn;
	int r;

	/* Is cache enabled? */
	if (cache == NULL) {
		errno = ENXIO;
		return (-1);
	}

	/* Get a new connection holder */
	if ((conn = MALLOC(MEM_TYPE_CONN, sizeof(*conn))) == NULL)
		return (-1);
	memset(conn, 0, sizeof(*conn));
	conn->peer = *peer;
	conn->ssl = ssl;
	conn->fp = fp;
	conn->sock = sock;

	/* Set expiration time */
	conn->expiry = time(NULL) + cache->max_idle;

	/* Lock cache */
	r = pthread_mutex_lock(&cache->mutex);
	assert(r == 0);

	/* Is there room in the cache? If not, drop oldest one. */
	if (cache->num >= cache->max_num) {
		http_connection_cache_extract(cache,
		    TAILQ_FIRST(&cache->list), NULL, NULL);
	}

	/* Add connection to the cache */
	TAILQ_INSERT_TAIL(&cache->list, conn, next);
	cache->num++;
	DBG(HTTP_CONNECTION_CACHE, "connection fp=%p for %s:%u ssl=%p"
	    " cached (%d total)", fp _ inet_ntoa(conn->peer.sin_addr) _
	    ntohs(conn->peer.sin_port) _ ssl _ cache->num);

	/* Make sure the timer is running */
	if (cache->num == 1)
		http_connection_cache_start_timer(cache);

	/* Unlock cache */
	r = pthread_mutex_unlock(&cache->mutex);
	assert(r == 0);

	/* Done */
	return (0);
}

/*********************************************************************
			INTERNAL FUNCTIONS
*********************************************************************/

/*
 * Remove a cached item from the cache.
 *
 * This assumes the cache is locked.
 */
static void
http_connection_cache_extract(struct http_connection_cache *cache,
	struct cached_connection *conn, FILE **fpp, int *sockp)
{
	struct cached_connection *const oldest = TAILQ_FIRST(&cache->list);

	/* Remove item and decrement count */
	TAILQ_REMOVE(&cache->list, conn, next);
	cache->num--;

	/* If we are the oldest, kill timer in order to restart later */
	if (conn == oldest)
		pevent_unregister(&cache->timer);

	/* Restart timer if any connections left */
	if (cache->num > 0)
		http_connection_cache_start_timer(cache);

	/* Return or close connection */
	if (fpp != NULL) {
		*fpp = conn->fp;
		*sockp = conn->sock;
	} else
		fclose(conn->fp);

	/* Free connection */
	FREE(MEM_TYPE_CONN, conn);
}

/*
 * Start the idle timer based on the oldest item in the cache.
 *
 * This assumes the cache is locked.
 */
static void
http_connection_cache_start_timer(struct http_connection_cache *cache)
{
	struct cached_connection *const oldest = TAILQ_FIRST(&cache->list);
	const time_t now = time(NULL);
	int timeout;

	/* Don't start timer when not appropriate */
	if (cache->num == 0
	    || cache->max_idle == 0
	    || cache->timer != NULL)
		return;

	/* Get time until oldest connection expires */
	timeout = (now < oldest->expiry) ? (oldest->expiry - now) * 1000 : 0;

	/* Start timer */
	if (pevent_register(cache->ctx, &cache->timer, 0,
	    &cache->mutex, http_connection_cache_timeout, cache,
	    PEVENT_TIME, timeout) == -1)
		alogf(LOG_ERR, "%s: %m", "pevent_register");
}

/*
 * Expire a cached connection from the cache.
 *
 * This assumes the cache is locked.
 */
static void
http_connection_cache_timeout(void *arg)
{
	struct http_connection_cache *cache = arg;
	const time_t now = time(NULL);

	/* Remove exipred entries */
	while (!TAILQ_EMPTY(&cache->list)) {
		struct cached_connection *const oldest
		    = TAILQ_FIRST(&cache->list);

		if (oldest->expiry < now)
			break;
		http_connection_cache_extract(cache, oldest, NULL, NULL);
	}

	/* Restart timer if any are left */
	if (cache->num > 0)
		http_connection_cache_start_timer(cache);
}

/*
 * http_connection_cache_check() checks whether a socket is invalid.
 *
 * NOTE: we are not checking if the socket is valid. All we can tell is we
 * think the socket is supposed to be valid because the server said keep-alive.
 * The connection should not be closed, but if it gracefully closed then the
 * server would have sent a FIN, which we will translate in to an EOF. This
 * will mean the socket is readable (because the EOF) is in the pipe. In all
 * other cases, the socket should not be readable because we are not expecting
 * data from the server. If the server sent us anything we should declare the
 * socket invalid. We still don't know if the socket is valid or not because if
 * the server crashed then nothing will be there for us, same as if the
 * connection is still working.
 */
static int
http_connection_cache_check(int sock)
{
	pd_pollfd myfd;

	/* Poll for readability */
	memset(&myfd, 0, sizeof(myfd));
	myfd.fd = sock;
	myfd.events = PD_POLLRDNORM;

	/* Return invalid if readable or other error */
	return (pd_poll(&myfd, 1, 0) == 0);
}

