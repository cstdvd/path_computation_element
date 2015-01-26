/*
 * Cygwin hacks.h
 * 
 * Portability hacks needed to build libpel.
 *
 * Most of this file has been moved to pd_port.h, only things needed
 * for the implementaiton or that need to get defined _before_ system
 * headers go here. 
 *
 * Cygwin is not regarded as a Native Win32 port as it provides a near
 * complete Unix environment and does not use the Win32 SDK headers or libs.
 */

#ifndef __PORTS_CYGWIN_HACKS_H_
#define __PORTS_CYGWIN_HACKS_H_ 1

#define _XOPEN_SOURCE	600
#define _GNU_SOURCE	1
#define _BSD_SOURCE	1
#define _ISOC99_SOURCE	1
#define __ISO_C_VISIBLE	2000
#define __BSD_VISIBLE	1
#define __POSIX_VISIBLE 2000000

#include <string.h>		/* memcpy(), etc. */
#include <netinet/in.h>		/* htonl(), etc. */

/* fcntl differences */
#define O_EXLOCK 0

/* Map misc internal calls back to public  */
#define _pthread_self		pthread_self
#define _pthread_mutex_lock	pthread_mutex_lock
#define _pthread_mutex_trylock	pthread_mutex_trylock
#define _pthread_mutex_unlock	pthread_mutex_unlock

/* Cygwin has these but is missing the prototypes */
long double
_strtold(const char * __restrict nptr, char ** __restrict endptr);
#define strtold _strtold

/* Cygwin is missing these */

#define fpurge(fp)	fflush(fp)
#define pdp_fpurge(fp)	fflush(fp)

#define __va_list va_list

#endif
