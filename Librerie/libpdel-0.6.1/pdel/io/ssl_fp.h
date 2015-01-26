
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_SSL_FP_H_
#define _PDEL_IO_SSL_FP_H_

/*
 * Logger type supplied to ssl_fdopen() and ssl_log()
 */
typedef void	ssl_logger_t(void *arg, int sev, const char *fmt, ...);

__BEGIN_DECLS

/*
 * Like fdopen(3), but the stream is encrypted using the supplied
 * SSL connection context.
 *
 * "server" should be one for an SSL server, zero for an SSL client.
 *
 * If "timeout" is non-zero, then it sets an idle timeout value in seconds.
 */
extern FILE	*ssl_fdopen(SSL_CTX *ssl_ctx, int fd, int server,
			const char *mtype, ssl_logger_t *logger,
			void *logarg, u_int timeout);

/*
 * Routine for logging any error from OpenSSL.
 */
extern void	ssl_log(ssl_logger_t *logger, void *arg);

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

#endif	/* _PDEL_IO_SSL_FP_H_ */
