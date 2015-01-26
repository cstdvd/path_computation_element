/*
 * socketpair.c - Emulate pipe with IP sockets so they are selectable
 *
 * This is however a functioning socketpair, so it can create a pair of UDP
 * sockets too.
 *
 * Author: Mark Gooderum <mark@jumpweb.com>
 */

#ifndef WIN32
#define HAVE_SOCKETPAIR 1
#endif

#include <errno.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "pdel/pd_io.h"

int
pd_socketpair(int af, int stype, int prot, int *sds)
{
#ifdef HAVE_SOCKETPAIR
	return(socketpair(af, stype, prot, sds));
#else
	struct sockaddr_in	sa1, sa2, sa3;
	int s1, s2, s3;
	int sz;
	int flags;
#ifdef WIN32
	u_long iMode;
#endif
	const char *emsg;

	if (af != AF_INET) {
		return(EDOM);
	}
	s1 = s2 = s3 = -1;
#ifdef HAVE_SIN_LEN
	sa1.sin_len = sizeof(s1);
#endif
	sa1.sin_family = AF_INET;
	sa1.sin_port = INADDR_ANY;
	sa1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if ((s1 = socket(af, stype, 0)) < 0) {
		emsg = "socket(s1)";
		goto sp_fail;
	}
	if (bind(s1, (struct sockaddr *)&sa1, sizeof(sa1)) == -1) {
		emsg = "bind(s1)";
		goto sp_fail;
	}
	if (listen(s1, 2) < 0) {
		emsg = "listen(s1)";
		goto sp_fail;
	}
	if ((s2 = socket(af, stype, 0)) < 0) {
		emsg = "socket(s2)";
		goto sp_fail;
	}
	sz = sizeof(sa2);
	if (getsockname(s1, (struct sockaddr *)&sa2, &sz) < 0) {
		emsg = "getsockname(s1)";
		goto sp_fail;
	}
	/* Set sd I/O to non-blocking */
#ifdef WIN32
	iMode = 1;
	if (ioctlsocket(s2, FIONBIO, &iMode)) {
		emsg = "ioctlsocket(NONBLOCK s2)";
		goto sp_fail;
	}
#else
#ifdef F_GETFL
	if ((flags = fcntl(s2, F_GETFL, 0)) == -1) {
		emsg = "fcntl(GETFL s2)";
		goto sp_fail;
	}
	if (((flags & O_NONBLOCK) == 0)
	    && (fcntl(s2, F_SETFL, flags | O_NONBLOCK) == -1)) {
		emsg = "fcntl(SETFL NONBLOCK s2)";
		goto sp_fail;
	}
#endif
#endif
	if (connect(s2,  (struct sockaddr *) &sa2, sizeof(sa2)) == -1) {
	  /* XXXX: TO DO - check for EINPROGRESS */
	}
	sz = sizeof(sa3);
	if ((s3 = accept(s1, (struct sockaddr *) &sa3, &sz)) < 0) {
		emsg = "accept(s2)";
		goto sp_fail;
	}
#ifdef WIN32
	iMode = 0;
	if (ioctlsocket(s2, FIONBIO, &iMode)) {
		emsg = "ioctlsocket(restore s2)";
		goto sp_fail;
	}
#else
#ifdef F_GETFL
	if (fcntl(s2, F_SETFL, flags) == -1) {
		emsg = "fcntl(restore s2)";
		goto sp_fail;
	}
#endif
#endif
	pd_close(s1);
	sds[0] = s2;
	sds[1] = s3;
	return(0);

sp_fail:
#ifdef WIN32
	flags = WSAGetLastError();
#endif
	flags = errno;
	fprintf(stderr, "pd_socketpair(): %s failed - %d\n", emsg, flags);

	if (s1 >= 0) {
		pd_close(s1);
	}
	if (s2 >= 0) {
		pd_close(s2);
	}
	if (s1 >= 0) {
		pd_close(s3);
	}
	errno = flags;
	return(-1);
#endif
}
