
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_COUNT_FP_H_
#define _PDEL_IO_COUNT_FP_H_

__BEGIN_DECLS

/*
 * Create a stream that reads a fixed number of bytes from an underlying
 * stream.
 *
 * The underlying stream is NOT closed when the returned stream is closed.
 */
extern FILE	*count_fopen(FILE *fp, off_t count, int closeit);

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

#endif	/* _PDEL_IO_COUNT_FP_H_ */
