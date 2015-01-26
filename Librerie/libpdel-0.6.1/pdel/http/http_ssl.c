
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/queue.h>

#include <pthread.h>

#include <netinet/in.h>

#include <openssl/ssl.h>

#include "http/http_server.h"
#include "http/http_internal.h"

static void	http_ssl_do_init(void);

/*
 * Initialize SSL (once).
 */
void
_http_ssl_init(void)
{
	static pthread_once_t ssl_init_once = PTHREAD_ONCE_INIT;

	pthread_once(&ssl_init_once, http_ssl_do_init);
}

static void
http_ssl_do_init(void)
{
	SSL_load_error_strings();
	SSL_library_init();
}

