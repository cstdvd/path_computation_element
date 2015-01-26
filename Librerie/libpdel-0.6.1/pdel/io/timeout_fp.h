
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_TIMEOUT_FP_H_
#define _PDEL_IO_TIMEOUT_FP_H_

__BEGIN_DECLS

/*
 * Create a stream that imposes a timeout on reads or writes.
 *
 * 'timeout' is in milliseconds; zero or negative means no timeout.
 */
extern FILE	*timeout_fdopen(int fd, const char *mode, int timeout);

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

#endif	/* _PDEL_IO_TIMEOUT_FP_H_ */
