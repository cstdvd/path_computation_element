
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_BOUNDARY_FP_H_
#define _PDEL_IO_BOUNDARY_FP_H_

__BEGIN_DECLS

/*
 * Create a FILE * that reads from another FILE *, stopping
 * when it reads in the supplied boundary string.
 *
 * If the boundary string is too long, NULL with EINVAL is returned.
 *
 * The underlying stream is not closed when the returned stream is closed
 * unless 'closeit' is non-zero.
 */
extern FILE	*boundary_fopen(FILE *fp, const char *boundary, int closeit);

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

#endif	/* _PDEL_IO_BOUNDARY_FP_H_ */
