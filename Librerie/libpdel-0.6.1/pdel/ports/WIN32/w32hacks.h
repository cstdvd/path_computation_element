/*
 * Win32 hacks.h
 * 
 * These are hacks common to all the "native" Win32 
 * ports (Cygwin isn't regarded as native).
 */

#ifndef __PORTS_WIN32_W32HACKS_H_
#define __PORTS_WIN32_W32HACKS_H_ 1

#define _XOPEN_SOURCE	600
#define _GNU_SOURCE	1
#define _BSD_SOURCE	1
#define _ISOC99_SOURCE	1
#define __ISO_C_VISIBLE	2000
#define __BSD_VISIBLE	1
#define __POSIX_VISIBLE 2000000
/* Get modern Win32 API */
#define WINVER 0x0501

/* BEGIN/END DECLS */
#include "sys/cdefs.h"

#define   _POSIX2_RE_DUP_MAX      255
#define   _POSIX_RE_DUP_MAX       _POSIX2_RE_DUP_MAX

/* Errors that are Winsock specific errors on Win32 */
#define EPROTOTYPE	WSAEPROTOTYPE
#define EALREADY	WSAEALREADY
#define ENOTCONN	WSAENOTCONN
#define ECONNABORTED	WSAECONNABORTED
#define ECONNRESET	WSAECONNRESET
#define ETIMEDOUT	WSAETIMEDOUT
#define EMSGSIZE	WSAEMSGSIZE
#define EAFNOSUPPORT	WSAEAFNOSUPPORT

/* fcntl differences */
#define O_NONBLOCK 0

/* ipv6 */ 
/* These require XP or .NET Server or use of add-on IPv6 stacks on NT 4
  or higher */


/*
 * WIN32 C runtime library had been made thread-safe
 * without affecting the user interface. Provide
 * mappings from the UNIX thread-safe versions to
 * the standard C runtime library calls.
 * Only provide function mappings for functions that
 * actually exist on WIN32.
 */
#define strtok_r( _s, _sep, _lasts ) \
        ( *(_lasts) = strtok( (_s), (_sep) ) )

/* POSIX requires only at least 100 bytes */
#define UNIX_PATH_LEN   108

typedef struct sockaddr_un {
  unsigned short sun_family;              /* address family AF_LOCAL/AF_UNIX */
  char	         sun_path[UNIX_PATH_LEN]; /* 108 bytes of socket address     */
} sockaddr_un;

/* Map misc internal calls back to public  */
#define _pthread_self		pthread_self
#define _pthread_mutex_lock	pthread_mutex_lock
#define _pthread_mutex_trylock	pthread_mutex_trylock
#define _pthread_mutex_unlock	pthread_mutex_unlock

/* Win32 Calls these something else */
/* shutdown() how types */
#define SHUT_RD		SD_RECEIVE
#define SHUT_WR		SD_SEND
#define SHUT_RDWR	SD_BOTH

/* Win32 has these but is missing the prototypes */
long double
_strtold(const char * __restrict nptr, char ** __restrict endptr);
#define strtold _strtold

/* Win32 is missing these natively */
#ifndef __MINGW32__
#define strtof(np, ep) ((float) strtod((np), (ep)))
#define strtold _strtold
#endif

/* Win32 is missing these */

#define fpurge(fp)	fflush(fp)
#define pdp_fpurge(fp)	fflush(fp)

/* Win32 is missing these and we have to supply the replacement */

#define timegm(t)		pd_timegm(t)
#define strptime(c, f, b)	pd_strptime((c), (f), (b))
#define strsep(s, d)		pd_strsep((s), (d))
#define realpath(s, d)		pd_realpath((s), (d))
#define fsync(f)		_commit(f)
#define inet_ntop(af, src, dst, s)	pd_inet_ntop((af),(src),(dst),(s))
#define inet_pton(af, src, dst)		pd_inet_ntop((af),(src),(dst))
#ifdef __MINGW32__
#define gai_strerror(e)		pd_gai_strerror(e)
#endif
#if 0
#ifdef __MINGW32__
#define localtime_r(in, out)	*(out) = *localtime(in)
#define gmtime_r(in, out)	*(out) = *gmtime(in)
#endif
#endif

#define chown(p, u, g) pd_chown((p), (u), (g))
#endif

