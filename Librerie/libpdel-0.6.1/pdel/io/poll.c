/*
 * poll.c - Emulate poll with select.
 *
 * Author: Mark Gooderum <mark@jumpweb.com>
 */


#include <sys/select.h>
#include <stdio.h>

#include "pdel/pd_poll.h"

int
pd_poll(pd_pollfd *ufds, pd_nfds_t nfds, int timeout)
{
#ifdef HAVE_POLL
	return(poll(ufds, nfds, timeout));
#else
	unsigned int idx;
	int maxfd, fd;
	int r;
	int errval;
#ifdef WIN32
	int any_fds_set = 0;
#endif
	fd_set readfds, writefds, exceptfds;
#ifdef USING_FAKE_TIMEVAL
#undef timeval
#undef tv_sec
#undef tv_usec
#endif
	struct timeval _timeout;
	_timeout.tv_sec = timeout/1000;
	_timeout.tv_usec = (timeout%1000)*1000;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);

	maxfd = -1;
	for (idx = 0; idx < nfds; ++idx) {
		ufds[idx].revents = 0;
		fd = ufds[idx].fd;
		if (fd > maxfd) {
		  maxfd = fd;
#ifdef WIN32
		  any_fds_set = 1;
#endif
		}
		if (ufds[idx].events & (PD_POLLIN | PD_POLLRDNORM)) {
			FD_SET(fd, &readfds);
		}
		if (ufds[idx].events & (PD_POLLRDBAND | PD_POLLRDNORM)) {
			FD_SET(fd, &exceptfds);
		}
		if (ufds[idx].events & 
		    (PD_POLLOUT | PD_POLLWRNORM | PD_POLLWRBAND)) {
			FD_SET(fd, &writefds);
		}
#if 0
		FD_SET(fd, &exceptfds); 
#endif
	}
#ifdef WIN32
	if (!any_fds_set) {
		Sleep(timeout);
		return 0;
	}
#endif
	r = select(maxfd+1, &readfds, &writefds, &exceptfds,
		   timeout == -1 ? NULL : &_timeout);
	if (r <= 0) {
#ifdef WIN32
		errval = WSAGetLastError();
		errno = errval;
#else
		errval = errno;
#endif
		fprintf(stderr, "select(mf=%d, to=%d)=%d, errno=%d\n",
			maxfd, timeout, r, errval);
		return r;
	}
	r = 0;
	for (idx = 0; idx < nfds; ++idx) {
		fd = ufds[idx].fd;
		if (FD_ISSET(fd, &readfds))
			ufds[idx].revents |= 
			  ((PD_POLLIN | PD_POLLRDNORM) & ufds[idx].events);
		if (FD_ISSET(fd, &writefds))
			ufds[idx].revents |= 
			  ((PD_POLLOUT | PD_POLLWRNORM) & ufds[idx].events);
		if (FD_ISSET(fd, &exceptfds))
			ufds[idx].revents |= 
			  ((PD_POLLRDBAND | PD_POLLWRBAND) & ufds[idx].events);
		if (ufds[idx].revents)
			++r;
	}
	return r;
#endif
}

