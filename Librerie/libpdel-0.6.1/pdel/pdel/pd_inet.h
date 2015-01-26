/*
 * pd_inet.h
 *
 * PD misc. Inet library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_INET_H__
#define __PDEL_PD_INET_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#if !defined(WIN32) && !defined(__CYGWIN__)
#include <arpa/nameser.h>
#endif
#ifdef WIN32
#include <ws2tcpip.h>
#endif

#ifndef NS_INT8SZ
#define NS_INT8SZ	1	/* #/bytes of data in a u_int8_t */
#define NS_INT16SZ	2	/* #/bytes of data in a u_int16_t */
#define NS_INT32SZ	4	/* #/bytes of data in a u_int32_t */
#define NS_INADDRSZ	4	/* IPv4 T_A */
#define NS_IN6ADDRSZ	16	/* IPv6 T_AAAA */
#endif

/* int
 * inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *	Paul Vixie, 1996.
 */
int
pd_inet_pton(int af, const char * __restrict src, void * __restrict dst);

/* char *
 * pd_inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char *
pd_inet_ntop(int af, const void * __restrict src, char * __restrict dst,
	     socklen_t size);

#endif



