
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PDEL_DEBUG_H_
#define _PDEL_PDEL_DEBUG_H_

/*
 * This flag globally enables/disables debugging.
 */
#define PDEL_DEBUG			0

#if PDEL_DEBUG
/*
 * Flags that enable various debugging options.
 */
enum {
	PDEL_DEBUG_HTTP,
	PDEL_DEBUG_HTTP_HDRS,
	PDEL_DEBUG_HTTP_CONNECTION_CACHE,
	PDEL_DEBUG_HTTP_SERVLET_COOKIEAUTH,
	PDEL_DEBUG_PEVENT,
	PDEL_DEBUG_MUTEX,
	PDEL_DEBUG_TMPL,
};

/*
 * Runtime variable which controls debugging.
 */
#define PDEL_DEBUG_FLAGS (0 						\
	| (1 << PDEL_DEBUG_HTTP)					\
	| (1 << PDEL_DEBUG_PEVENT)					\
	| (1 << PDEL_DEBUG_MUTEX)					\
	| 0)

#endif	/* PDEL_DEBUG */

/*
 * Macro to test whether a specific debug flag is enabled.
 */
#if PDEL_DEBUG
#define PDEL_DEBUG_ENABLED(f)						\
	((PDEL_DEBUG_FLAGS & (1 << PDEL_DEBUG_ ## f)) != 0)
#else
#define PDEL_DEBUG_ENABLED(f)		(0)
#endif

/*
 * Macro for printing some debugging output.
 */
#if PDEL_DEBUG
#ifdef PD_VA_MACRO_GNU
#define DBG(c, fmt, args...)						\
	do {								\
		char buf[240];						\
		const char *s;						\
		int r, n;						\
									\
		if (PDEL_DEBUG_ENABLED(c)) {				\
			snprintf(buf, sizeof(buf),			\
			    "%p[%s]: " fmt "\n", pthread_self(),	\
			    __FUNCTION__ , ## args);			\
			for (r = strlen(buf), s = buf;			\
			    r > 0; r -= n, s += n) {			\
				if ((n = write(2, s, r)) == -1)		\
					break;				\
			}						\
		}							\
	} while (0)
#elif defined(PD_VA_MACRO_C99)
#define DBG(c, fmt, ...)						\
	do {								\
		char buf[240];						\
		const char *s;						\
		int r, n;						\
									\
		if (PDEL_DEBUG_ENABLED(c)) {				\
			snprintf(buf, sizeof(buf),			\
			    "%p[%s]: " fmt "\n", pthread_self(),	\
			    __FUNCTION__ , ## __VA_ARGS__);			\
			for (r = strlen(buf), s = buf;			\
			    r > 0; r -= n, s += n) {			\
				if ((n = write(2, s, r)) == -1)		\
					break;				\
			}						\
		}							\
	} while (0)
#elif defined(PD_VA_MACRO_MSVC)
#define DBG(c, fmt, ...)						\
	do {								\
		char buf[240];						\
		const char *s;						\
		int r, n;						\
									\
		if (PDEL_DEBUG_ENABLED(c)) {				\
			snprintf(buf, sizeof(buf),			\
			    "%p[%s]: " fmt "\n", pthread_self(),	\
			    __FUNCTION__ , __VA_ARGS__);			\
			for (r = strlen(buf), s = buf;			\
			    r > 0; r -= n, s += n) {			\
				if ((n = write(2, s, r)) == -1)		\
					break;				\
			}						\
		}							\
	} while (0)
#else
#define DBG(c, fmt, args)						\
	do {								\
		char buf[240];						\
		const char *s;						\
		int r, n;						\
									\
		if (PDEL_DEBUG_ENABLED(c)) {				\
			fprintf(stderr, "[" __FUNCTION__ "]: " fmt, args ); \
		}							\
	} while (0)
#endif
#else
#ifdef PD_VA_MACRO_NONE
#define DBG(c, fmt, args)		do { } while (0)
#elif defined(PD_VA_MACRO_C99) || defined(PD_VA_MACRO_MSVC)
#define DBG(c, fmt, ...)	do { } while (0)
#else
#define DBG(c, fmt, args)	do { } while (0)
#endif
#endif	/* !PDEL_DEBUG */

/*
 * Mutex macros with additional debugging.
 */
#if PDEL_DEBUG

#define MUTEX_LOCK(m, c)						\
	do {								\
		int _r;							\
									\
		DBG(MUTEX, "locking %s", #m);				\
		_r = pthread_mutex_lock(m);				\
		assert(_r == 0);					\
		(c)++;							\
		DBG(MUTEX, "%s locked -> %d", #m, (c));			\
	} while (0)

#define MUTEX_UNLOCK(m, c)						\
	do {								\
		int _r;							\
									\
		(c)--;							\
		DBG(MUTEX, "unlocking %s -> %d", #m, (c));		\
		_r = pthread_mutex_unlock(m);				\
		assert(_r == 0);					\
		DBG(MUTEX, "%s unlocked", #m);				\
	} while (0)

#define MUTEX_TRYLOCK(m, c)						\
	do {								\
		int _r;							\
									\
		DBG(MUTEX, "try locking %s", #m);			\
		_r = pthread_mutex_trylock(m);				\
		if (_r == 0) {						\
			(c)++;						\
			DBG(MUTEX, "%s locked -> %d", #m, (c));		\
		} else							\
			DBG(MUTEX, "%s lock failed", #m);		\
		errno = _r;						\
	} while (0)

#else	/* !PDEL_DEBUG */

#define MUTEX_LOCK(m, c)	(void)pthread_mutex_lock(m)
#define MUTEX_UNLOCK(m, c)	(void)pthread_mutex_unlock(m)
#define MUTEX_TRYLOCK(m, c)	(errno = pthread_mutex_trylock(m))

#endif	/* !PDEL_DEBUG */

#endif	/* _PDEL_PDEL_DEBUG_H_ */
