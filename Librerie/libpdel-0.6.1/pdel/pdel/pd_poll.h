/*
 * pd_poll.h - Wrapper emulate poll with select.
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com>
 */

/*-
 * Copyright (c) 1997 Peter Wemm <peter@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/poll.h,v 1.13 2002/07/10 04:47:25 mike Exp $
 */

#ifndef __PD_POLL_H__
#define	__PD_POLL_H__ 1

#include <sys/cdefs.h>

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef HAVE_POLL
#include <poll.h>

#define	PD_POLLIN	POLLIN	
#define	PD_POLLPRI	POLLPRI	
#define	PD_POLLOUT	POLLOUT	
#define	PD_POLLRDNORM	POLLRDNORM	
#define	PD_POLLWRNORM	POLLWRNORM	
#define	PD_POLLRDBAND	POLLRDBAND	
#define	PD_POLLWRBAND	POLLWRBAND	

#if __BSD_VISIBLE
#define	PD_POLLINIGNEOF	POLLINIGNEOF	
#endif

#define	PD_POLLERR	POLLERR	
#define	PD_POLLHUP	POLLHUP	
#define	PD_POLLNVAL	POLLNVAL	

#if __BSD_VISIBLE

#define	PD_POLLSTANDARD	POLLSTANDARD	
#define	PD_INFTIM		INFTIM		

#endif

typedef struct pollfd	pd_pollfd;
#ifdef __CYGWIN__
typedef unsigned int	nfds_t;
#endif
typedef	nfds_t		pd_nfds_t;

#else /* ! HAVE_POLL */

/*
 * This file is intended to be compatible with the traditional poll.h.
 */

typedef	unsigned int	pd_nfds_t;

/*
 * This structure is passed as an array to pd_poll(2).
 */
typedef struct pd_pollfd {
	int	fd;		/* which file descriptor to pd_poll */
	short	events;		/* events we are interested in */
	short	revents;	/* events found on return */
} pd_pollfd;

/*
 * Requestable events.  If pd_poll(2) finds any of these set, they are
 * copied to revents on return.
 * XXX Note that FreeBSD doesn't make much distinction between PD_POLLPRI
 * and PD_POLLRDBAND since none of the file types have distinct priority
 * bands - and only some have an urgent "mode".
 * XXX Note PD_POLLIN isn't really supported in true SVSV terms.  Under SYSV
 * PD_POLLIN includes all of normal, band and urgent data.  Most pd_poll handlers
 * on FreeBSD only treat it as "normal" data.
 */
#define	PD_POLLIN	0x0001		/* any readable data available */
#define	PD_POLLPRI	0x0002		/* OOB/Urgent readable data */
#define	PD_POLLOUT	0x0004		/* file descriptor is writeable */
#define	PD_POLLRDNORM	0x0040		/* non-OOB/URG data available */
#define	PD_POLLWRNORM	PD_POLLOUT		/* no write type differentiation */
#define	PD_POLLRDBAND	0x0080		/* OOB/Urgent readable data */
#define	PD_POLLWRBAND	0x0100		/* OOB/Urgent data can be written */

#if __BSD_VISIBLE
/* General FreeBSD extension (currently only supported for sockets): */
#define	PD_POLLINIGNEOF	0x2000		/* like PD_POLLIN, except ignore EOF */
#endif

/*
 * These events are set if they occur regardless of whether they were
 * requested.
 */
#define	PD_POLLERR	0x0008		/* some pd_poll error occurred */
#define	PD_POLLHUP	0x0010		/* file descriptor was "hung up" */
#define	PD_POLLNVAL	0x0020		/* requested events "invalid" */

#if __BSD_VISIBLE

#define	PD_POLLSTANDARD	(PD_POLLIN|PD_POLLPRI|PD_POLLOUT|PD_POLLRDNORM|PD_POLLRDBAND|\
			 PD_POLLWRBAND|PD_POLLERR|PD_POLLHUP|PD_POLLNVAL)

/*
 * Request that pd_poll() wait forever.
 * XXX in SYSV, this is defined in stropts.h, which is not included
 * by pd_poll.h.
 */
#define	PD_INFTIM		(-1)

#endif	/* _BSD_VISIBLE */

#endif  /* ifdef HAVE_POLL else */

__BEGIN_DECLS
int	pd_poll(pd_pollfd *_pfd, pd_nfds_t _nfds, int _timeout);
__END_DECLS


#endif /* !_PD_POLL_H_ */
