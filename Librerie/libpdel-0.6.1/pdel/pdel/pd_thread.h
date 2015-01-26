/*
 * pd_thread.h
 *
 * PD thread releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_THREAD_H__
#define __PDEL_PD_THREAD_H__

#ifndef PD_PORT_INCLUDED
#include <pdel/pd_port.h>
#endif

/* Public Test */
#define PD_THREAD_INCLUDED 1

#include <pthread.h>

/*
 * pthread_t is not guaranteed to be a pointer, but many things assume
 * it is, so we need some helpers for portable comparisons.
 */

PD_IMPORT pthread_t pd_null_pthread;

#ifdef PTW32_VERSION

/* GNU pthread-win32 */
#define PD_PTHREAD_NULL	pd_null_pthread

#else

#ifdef __linux__
#define PD_PTHREAD_NULL 0
#else
#define PD_PTHREAD_NULL ((void *)0)
#endif

#endif

__BEGIN_DECLS

/*
 * Compare pthreads for equality.
 */
int
pd_pthread_equal(const pthread_t p1, const pthread_t p2);

int
pd_pthread_isnull(const pthread_t p1);

#ifdef PTW32_VERSION

#define pd_pthread_equal(p1, p2)	\
	(((p1).p == (p2).p) && ((p1).x == (p2).x))

#define pd_pthread_isnull(p1)		\
	(((p1).p == ((void *)0)) && ((p1).x == 0))

#else

#define pd_pthread_equal(p1, p2)	((p1) == (p2))
#define pd_pthread_isnull(p1)		((p1) == PD_PTHREAD_NULL)

#endif

__END_DECLS


#endif
