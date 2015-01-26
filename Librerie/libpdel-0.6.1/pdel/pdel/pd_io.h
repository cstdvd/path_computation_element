/*
 * pd_io.h - Misc PD I/O routines.
 *
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com>
 */


#ifndef __PDEL_PD_IO_H__
#define __PDEL_PD_IO_H__

#ifndef PD_BASE_INCLUDED
#include <pdel/pd_base.h>
#endif

/* Public Test */
#define PD_IO_INCLUDED 1

/* Macros */

/*
 * pipe(2)/socketpair() replacment that uses IP sockets.
 *
 * Particularly useful on Windows which can only select on sockets and
 * doesn't have Domain sockets.
 */

int
pd_socketpair(int af, int stype, int prot, int *sds);

/*
 * Wrapper for ftruncate that works on Win32.
 */
int 
pd_ftruncate(int fd, off_t len);

/*
 * Wrapper for read/write/close that work with files and sockets on Win32.
 */

int
pd_read(int s, char *buf, int len);

int
pd_write(int s, const char *buf, int len);

int
pd_close(int s);

#endif
