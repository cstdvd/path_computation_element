/*
 * pd_port.h
 *
 * PD common portability declarations, library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_PORT_H__
#define __PDEL_PD_PORT_H__

/* Public Test */
#define PD_PORT_INCLUDED 1

/*
 * Baseline assumes these headers, at least for portability tests.
 */
#include <sys/types.h>
#ifdef BUILDING_PDEL
#include <sys/param.h>
#endif
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#ifdef WIN32
#include <stdlib.h>	/* size_t lives here in Win32	*/
#endif

/*
 * Certain Network bits must be included before this header if 
 * using network functions, this is mainly
 * to avoid the pain on Win32 platforms of winsock2.h on every file.
 */

#ifdef SOCK_STREAM
#define PD_NET_ENABLE 1
#endif

/*******************
 * 
 * Port Setup
 *
 *******************/

/** BSD Common **/

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define HAVE_SIN_LEN 1
#define PDEL_NET_SUPPORT 1
#define HAVE_FUNOPEN 1
#define HAVE_BPF 1
#define HAVE_RUNETYPE 1
#define HAVE_COLLATE 1
#else
#ifndef __linux__
#define NEED_ETHER_ADDR_LEN 1
#endif
#endif

/** Solaris **/
#ifdef __SunOS__
#define NEED_BSD_U_TYPES 1
#define HAVE_RUNETYPE 1
#define HAVE_COLLATE 1
#endif

/** LINUX **/

#ifdef __linux__
/*
 * We need to be a bit smarter on some of these as a few show up 
 * in later glibc versions.
 */
#define HAVE_FUNOPENCOOKIE 1
#define NEED_MERGESORT 1
#define NEED_HEAPSORT 1
#define NEED_SO_RESUSEPORT 1
#define NEED_STRLCAT 1
#define NEED_STRLCPY 1
#define NEED_INFTIM 1
#define NEED_O_EXLOCK 1
#define HAVE_RUNETYPE 1
#define HAVE_COLLATE 1
#endif

/** CYGWIN **/
#ifdef __CYGWIN__
#define NEED_INET6 1
#define NEED_TVTS 1
#define NEED_FUNOPEN 1
#define NEED_INET6_ADDRSTRLEN 1
#define NEED_MERGESORT 1
#define NEED_HEAPSORT 1
#define NEED_INFTIM 1
#endif

/** Win32 Common **/

#ifdef WIN32
#define NEED_MERGESORT 1
#define NEED_HEAPSORT 1
#define NEED_SO_RESUSEPORT 1
#define NEED_STRLCAT 1
#define NEED_STRLCPY 1
#define NEED_FUNOPEN 1
#define NEED_UID_T 1
#define NEED_GID_T 1
#define NEED_BSD_INT_TYPES 1
#define NEED_BSD_VINT_TYPES 1
#define NEED_SOCKLEN_T 1
#define NEED___OFF_T 1
#define NEED_MAXHOSTNAMELEN 1
#define NEED_USLEEP 1
#define __va_list va_list
#if 0
#define NEED_INET6_ADDRSTRLEN 1
#endif
#endif

/** Win32 - MinGW  **/
#ifdef __MINGW32__
#endif

/** Win32 - MSVCRT  **/
#ifdef _MSC_VER
#define NEED_STDINT_UTYPES 1
#define NEED_STDINT_TYPES 1
#define NEED__SIZE_T 1
#define NEED_UINTMAX_T 1
#define NEED_INTMAX_T 1
#define NEED__BOOL 1
#define NEED_MODE_T 1
#define NEED_STRTOIMAX 1
#define NEED_STRTOUMAX 1
#define NEED_STRTOLL 1
#define NEED_STRTOULL 1
#define NEED_STRCASECMP 1
#define NEED_STRNCASECMP 1
#define NEED_VSNPRINTF 1
#define NEED_SNPRINTF 1
#define NEED_WINDLL_DECLSPEC 1
#endif

/** Misc. Common **/

#if !defined(HAVE_FUNOPEN) && !defined(HAVE_FUNOPENCOOKIE)
#define NEED_FUNOPEN 1
#define NEED_O_EXLOCK 1
#endif

#ifndef LLONG_MAX
#define NEED_LLONG_MAX 1
#endif

#ifndef LLONG_MIN
#define NEED_LLONG_MIN 1
#endif

#ifdef WIN32
#define NEED_POLL 1
#else
#define HAVE_POLL 1
#endif

#ifdef NEED_POLL
#define	NEED_INFTIM 1
#endif

/** Vararg Macro Hacks  **/

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define PD_VA_MACRO_GNU 1
#elif defined(HAVE_C99)
#define PD_VA_MACRO_C99 1
#elif defined(_MSC_VER) && _MSC_VER >= 1400 /* Microsoft Visual C++ 2005 or newer */
#define PD_VA_MACRO_MSVC 1
#else
#define PD_VA_MACRO_NONE 1
#endif

#ifdef BUILDING_PDEL
#define NEED_VA_UNDERSCORE 1
#endif

#ifdef NEED_VA_UNDERSCORE
#define _ __PD_COMMA__
#define __PD_COMMA__ ,
#endif

/*******************
 * 
 * Attributes
 *
 *******************/


#ifndef __dead2
#ifdef __GNUC__
#define __dead2         __attribute__((__noreturn__))
#else
#define __dead2
#endif
#endif

#ifndef __packed
#ifdef __GNUC__
#define	__packed	__attribute__((__packed__))
#else
#define __packed
#endif
#endif

#ifndef __unused
#ifdef __GNUC__
#define	__unused	__attribute__((__unused__))
#else
#define __unused
#endif
#endif

#ifndef __printflike
# if defined(__GNUC__)
#  ifdef __linux__
#   define __printflike(fmtarg, firstvararg) \
__THROW __attribute__ ((__format__ (__printf__, fmtarg, firstvararg)));
#  else
#   define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#  endif
# else
#  define __printflike(fmtarg, firstvararg)
# endif
#endif

#ifndef __scanflike
#if defined(__GNUC__) && !defined(__linux__)
#define __scanflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#else
#define __scanflike(fmtarg, firstvararg)
#endif
#endif

/* GCC 2.95 and later have "__restrict"; C99 compilers have
   "restrict", and "configure" may have defined "restrict".  */
#ifndef __restrict
# if ! (2 < __GNUC__ || (2 == __GNUC__ && 95 <= __GNUC_MINOR__))
#  if defined restrict || 199901L <= __STDC_VERSION__
#   define __restrict restrict
#  else
#   define __restrict
#  endif
# endif
#endif

#ifndef __const
#define __const const
#endif

#ifndef __weak_reference
#define __weak_reference(sym,alias) \
  struct __pd_struct_hack_ ## sym ;
#endif

#ifndef __FreeBSD__
#define __FBSDID(s)	static const char *pd_fbsdid   __attribute__ ((unused)) = (s);
#endif

/* DLL Export/Import Tricks */
#if defined(NEED_WINDLL_DECLSPEC) && (defined(PDEL_SHARED) && PDEL_SHARED != 0)

#ifdef BUILDING_PDEL
#define PD_EXPORT __declspec(dllexport)
#define PD_IMPORT __declspec(dllexport) extern
#else
#define PD_IMPORT __declspec(dllimport) extern
#endif

#else
#define PD_EXPORT
#define PD_IMPORT extern
#endif

/*******************
 * 
 * Header Bits
 *
 *******************/

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#else
#define __BEGIN_DECLS
#endif
#endif

#ifndef __END_DECLS
#ifdef __cplusplus
#define __END_DECLS };
#else
#define __END_DECLS
#endif
#endif

/*******************
 * 
 * String Funcs
 *
 *******************/

#ifdef NEED_STRLCPY
#define strlcpy(dst, src, size)	snprintf((dst), (size), "%s", (src))
#endif

#ifdef NEED_STRLCAT
#define strlcat(dst, src, size)	snprintf((dst) + strlen(dst), \
					 (size) - strlen(dst), "%s", (src))
#endif

#ifdef NEED_STRTOLL
 #ifdef WIN32
 /* Gross Hack Alert */
  #if _MSC_VER < 1300
   #define strtoll(p, e, b) ((*(e) = (char*)(p) + (((b) == 10) ? strspn((p), "0123456789") : 0)), _atoi64(p))
  #else
   #define strtoll(p, e, b) _strtoi64(p, e, b) 
  #endif
 #endif
#endif

#ifdef NEED_STRTOIMAX
#define strtoimax(p, e, b) (intmax_t)(strtoll((p), (e), (b)))
#endif

#ifdef NEED_STRTOUMAX
#define strtoumax(p, e, b) (uintmax_t)(strtoull((p), (e), (b)))
#endif

#ifdef NEED_STRTOULL
 #ifdef WIN32
 /* Gross Hack Alert */
  #if _MSC_VER < 1300
   #define strtoull(p, e, b) ((*(e) = (char*)(p) + (((b) == 10) ? strspn((p), "0123456789") : 0)), (unsigned long long) _atoi64(p))
  #else
   #define strtoull(p, e, b) _strtoui64(p, e, b) 
  #endif
 #endif
#endif

#ifdef NEED_STRCASECMP
#ifdef WIN32
#define strcasecmp	_stricmp
#endif
#endif

#ifdef NEED_STRNCASECMP
#ifdef WIN32
#define strncasecmp	_strnicmp
#endif
#endif

/*******************
 * 
 * Stdio Funcs
 *
 *******************/

#ifdef NEED_VSNPRINTF
#ifdef WIN32
#define vsnprintf _vsnprintf
#endif
#endif

#ifdef NEED_SNPRINTF
#ifdef WIN32
#define snprintf _snprintf
#endif
#endif

#ifdef HAVE_FUNOPENCOOKIE
#include <libio.h>
#define funopen1(c, m, rf, wf, sf, cf)					\
	fopencookie(c, m,						\
	    ({								\
		_IO_cookie_io_functions_t _funcs;			\
		_funcs.read = (cookie_read_function_t *)(rf);		\
		_funcs.write = (cookie_write_function_t *)(wf);		\
		_funcs.seek = (cookie_seek_function_t *)(sf);		\
		_funcs.close = (cookie_close_function_t *)(cf);		\
		_funcs;							\
	     }))
#define funopen(c, rf, wf, sf, cf) \
			funopen1(c, "r+", rf, wf, sf, cf)
#define fropen(c, rf)	funopen1(c, "r", rf, NULL, NULL, NULL)
#define fwopen(c, wf)	funopen1(c, "w", NULL, wf, NULL, NULL)

/*
 * This is needed to that various non-standard cookie hooks can get at the 
 * cookie.  This structure hasn't changed from glibc 2.0 through 2.3.5 (the
 * most recent version.
 *
 * Another way to do this is the cookie is always before the read cookie 
 * func, so one could search the memory after the basic IO structure too.
 */

struct _IO_jump_t;
struct _IO_FILE_plus
{
  _IO_FILE file;
  const struct _IO_jump_t *vtable;
};

/* Special file type for fopencookie function.  */
struct _IO_cookie_file
{
  struct _IO_FILE_plus __fp;
  void *__cookie;
  _IO_cookie_io_functions_t __io_functions;
};

#endif

/*******************
 * 
 * Various Constants
 *
 *******************/

#ifdef NEEDTCASOFT
#define TCSASOFT 0
#endif

#ifdef NEED_LLONG_MAX
#ifdef INT64_MAX
#define LLONG_MAX	INT64_MAX
#else
#define LLONG_MAX	9223372036854775807LL
#endif
#endif

#ifdef NEED_LLONG_MIN
#ifdef INT64_MIN
#define LLONG_MIN	INT64_MIN
#else
#define LLONG_MIN	(-LLONG_MAX - 1LL)
#endif
#endif

#if defined(NEED_O_EXLOCK) && !defined(O_EXLOCK)
#define O_EXLOCK 0
#endif

#ifdef NEED_INFTIM
#define	INFTIM (-1)
#endif

#ifdef NEED_MAXHOSTNAMELEN
#ifdef WIN32
#define MAXHOSTNAMELEN 1024	/* NI_MAXHOST */
#endif
#endif


/*******************
 * 
 * Stdlib Funcs
 *
 *******************/

/*
 * Linux has only qsort(), sigh.
 *
 * merge and heapsort have returns, but qsort never fails, so fudge 
 * into a non-void espression with the comma operator.
 */
#ifdef NEED_MERGESORT
#define mergesort(b, n, s, c)	(qsort((b), (n), (s), (c)), 0)
#endif

#ifdef NEED_HEAPSORT
#define heapsort(b, n, s, c)	(qsort((b), (n), (s), (c)), 0)
#endif

/*******************
 * 
 * Ctype Funcs
 *
 *******************/

#ifndef ishexnumber
#define ishexnumber(x) isxdigit(x)
#endif
#ifndef ishex
#define ishex(x) isxdigit(x)
#endif

/*******************
 * 
 * Sockaddr Funcs
 *
 *******************/

/*
 * Hide non-BSD's sin_lenlessness.
 */
#ifdef HAVE_SIN_LEN
#define PD_SOCKADDR_SETLEN(sa, len) ((sa)->sa_len = (len))
#define PD_SOCKADDRIN_SETLEN(sa, len) ((sa)->sin_len = (len))
#define PD_SOCKADDRUN_SETLEN(sa, len) ((sa)->sun_len = (len))
#else
#define PD_SOCKADDR_SETLEN(s, l) 
#define PD_SOCKADDRIN_SETLEN(s, l) 
#define PD_SOCKADDRUN_SETLEN(s, l) 
#endif

/*******************
 * 
 * Net Funcs
 *
 *******************/
#ifdef NEED_ETHER_ADDR_LEN
#define ETHER_ADDR_LEN	6
#endif

/*
 * Silly Linux socket implementation.
 */
#if defined(NEED_SO_RESUSEPORT) && !defined(SO_REUSEPORT)
#define SO_REUSEPORT SO_REUSEADDR
#endif

/*******************
 * 
 * Unistd Funcs
 *
 *******************/
#ifdef NEED_USLEEP
#ifdef WIN32
#define usleep(usec)	Sleep((usec) / 1000 > 0 ? (usec) / 1000 : 1)
#endif
#endif

/*******************
 * 
 * IPv6 Funcs
 *
 *******************/
/* Borrowed from MinGW ws2tcpip.h */
#ifdef NEED_INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifdef NEED_INET6

#define AF_INET6        23              /* IP version 6 */
#define PF_INET6        AF_INET6

/* ipv6 */ 
/* These require XP or .NET Server or use of add-on IPv6 stacks on NT 4
  or higher */

/* This is based on the example given in RFC 2553 with stdint types
   changed to BSD types.  For now, use these  field names until there
   is some consistency in MS docs. In this file, we only use the
   in6_addr structure start address, with casts to get the right offsets
   when testing addresses */
  
/* Don't define this if we think we've seen any common IPv6 headers */
#ifndef IN6ADDR_ANY_INIT
struct in6_addr {
    union {
        u_char	_S6_u8[16];
        u_short	_S6_u16[8];
        u_long	_S6_u32[4];
        } _S6_un;
};
#endif
#endif


/*******************
 * 
 * Errnos
 *
 *******************/
#ifndef EFTYPE
#define EFTYPE		EINVAL
#endif

#ifndef EPROGMISMATCH
#define EPROGMISMATCH	EINVAL
#endif

/*******************
 * 
 * Ordinal Types
 *
 *******************/
/* Adapted from FreeBSD */
#ifdef NEED_BSD_VINT_TYPES
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
#endif

#ifdef NEED_STDINT_UTYPES
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;
#else
#include <stdint.h>
#endif

#ifdef NEED_STDINT_TYPES
typedef char int8_t;
typedef short int16_t;
typedef long int32_t;
typedef long long int64_t;
#else
#include <stdint.h>
#endif

#ifdef NEED_BSD_INT_TYPES
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
#endif /* _BSDTYPES2_DEFINED */

#ifdef NEED__BOOL
typedef int	_Bool;
#endif

#ifdef NEED_MODE_T
typedef int	mode_t;
#endif

#ifdef NEED_SOCKLEN_T
typedef int socklen_t;
#endif

#ifdef NEED___OFF_T
typedef off_t	__off_t;
#endif

#ifdef NEED_UINTMAX_T
typedef u_int64_t	uintmax_t;
#endif
#ifdef NEED_INTMAX_T
typedef int64_t		intmax_t;
#endif

#ifdef NEED__SIZE_T
#ifdef WIN32
#ifdef _WIN64
typedef unsigned __int64 __size_t;
#else
typedef unsigned int __size_t;
#endif
#else
typedef size_t __size_t;
#endif
#endif

/*******************
 * 
 * Misc Bits
 *
 *******************/

/* Punt on a few missing types  */
#ifdef NEED_UID_T
typedef int	uid_t;
#endif
#ifdef NEED_GID_T
typedef int	gid_t;
#endif

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define _ALIGNBYTES	(sizeof(int) - 1)
#endif
#ifndef _ALIGN
#define _ALIGN(p)	(((unsigned)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)
#endif

#ifndef ALIGNBYTES
#define ALIGNBYTES	_ALIGNBYTES
#endif
#ifndef ALIGN
#define ALIGN(p)	_ALIGN(p)
#endif

#ifndef MIN
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y)	((x) > (y) ? (x) : (y))
#endif

#ifndef roundup
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif

#ifndef roundup2
#define roundup2(x, y)  (((x)+((y)-1))&(~((y)-1))) /* if y is pow of 2 */
#endif


#endif
