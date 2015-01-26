/*
 * pd_time.h
 *
 * PD time releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_TIME_H__
#define __PDEL_PD_TIME_H__

#ifndef PD_PORT_INCLUDED
#include <pdel/pd_port.h>
#endif

/* Public Test */
#define PD_TIME_INCLUDED 1

#include <sys/time.h>

/*******************
 * 
 * Time Macros
 *
 *******************/

#define PD_TIMEVAL_TO_TIMESPEC(tv, ts) {			\
	(ts)->tv_sec = (tv)->tv_sec;				\
	(ts)->tv_nsec = (tv)->tv_usec * 1000;			\
}
#define PD_TIMESPEC_TO_TIMEVAL(tv, ts) {			\
	(tv)->tv_sec = (ts)->tv_sec;				\
	(tv)->tv_usec = (ts)->tv_nsec / 1000;			\
}

#define pd_timerclear(tvp)		((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define pd_timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define pd_timercmp(tvp, uvp, cmp)					\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))

#define pd_timeradd(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#define pd_timersub(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)

__BEGIN_DECLS

/*******************
 * 
 * Time Funcs
 *
 *******************/

/*
 * Misc. time funcs including those missing on some platforms.
 */
struct tm;
struct timeval;

time_t
pd_timegm(struct tm *t);

int
pd_gettimeofday (struct timeval *tv, void *tz_unused);

char *
pd_strptime(const char *buf,
	    const char *format,
	    struct tm *timeptr);

__END_DECLS


#endif
