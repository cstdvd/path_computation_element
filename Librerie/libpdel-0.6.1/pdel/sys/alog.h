
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_SYS_ALOG_H_
#define _PDEL_SYS_ALOG_H_

#include <sys/types.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdarg.h>

#include "pdel/pd_port.h"
#include "pdel/pd_regex.h"

#ifndef DEFINE_STRUCTS_ARRAY
#ifdef BUILDING_PDEL
#include "structs/type/array_define.h"
#else
#include "pdel/structs/type/array_define.h"
#endif
#endif

/*
 * Simple support for logging channels. Each channel can log to
 * standard error, local syslog, or remote syslog, and has a minimum
 * log severity filter.
 */

#define ALOG_MAX_CHANNELS	16	/* max number of channels */

/*
 * This structure is used to configure a channel.
 */
typedef struct alog_config {
	const char	*path;		/* logfile filename, or NULL for none */
	const char	*name;		/* syslog id, or null to disable */
	const char	*facility;	/* syslog facility, null for stderr */
	struct in_addr	remote_server;	/* remote server, or 0.0.0.0 local */
	int		min_severity;	/* min severity to actually log */
	int		histlen;	/* how many history entries to save */
} alog_config;

/* Entries in the log history are returned in this form */
typedef struct alog_entry {
	time_t	when;			/* when event was logged */
	int	sev;			/* entry log severity */
	char	msg[1];			/* entry contents (including NUL) */
} alog_entry;

DEFINE_STRUCTS_ARRAY_T(alog_history, struct alog_entry *);

__BEGIN_DECLS

/*
 * Initialize or reconfigure a logging channel.
 *
 *	channel		Between zero and ALOG_MAX_CHANNELS - 1.
 *	conf		Channel configuration.
 */
extern int	alog_configure(int channel, const struct alog_config *conf);

/*
 * Reset a logging channel.
 */
extern int	alog_shutdown(int channel);

/*
 * Set current logging channel.
 */
extern int	alog_set_channel(int channel);

/*
 * Enable/disable debugging on a channel. Everything logged to the
 * channel will be logged to stderr as well.
 */
extern void	alog_set_debug(int channel, int enabled);

/*
 * Get a selection from the log history.
 *
 * The caller's structs array is filled in and is an array of
 * pointers to struct alog_entry.
 *
 * Caller should free the returned array by calling
 * "structs_free(&alog_history_type, NULL, list)".
 */
extern int	alog_get_history(int channel, int min_severity,
			int max_entries, time_t max_age,
			const regex_t *preg, struct alog_history *list);

/*
 * Clear (i.e., forget) log history.
 */
extern int	alog_clear_history(int channel);

/*
 * Log to the currently active logging channel. Preserves errno.
 */
extern void	alog(int sev, const char *fmt, ...) __printflike(2, 3);
extern void	valog(int sev, const char *fmt,
			va_list args) __printflike(2, 0);

/*
 * Convert between numeric syslog facility and string.
 */
extern int	alog_facility(const char *name);
const char	*alog_facility_name(int facility);

/*
 * Convert between numeric syslog severity and string.
 */
extern int	alog_severity(const char *name);
const char	*alog_severity_name(int sev);

/*
 * Expand '%m' in a format string.
 *
 * Returns a pointer to a static buffer.
 */
extern void	alog_expand(const char *fmt,
			int errnum, char *buf, size_t bufsize);

/* Some useful alog "structs" types */
PD_IMPORT const struct structs_type	alog_facility_type;
PD_IMPORT const struct structs_type	alog_severity_type;
PD_IMPORT const struct structs_type	alog_config_type;
PD_IMPORT const struct structs_type	alog_history_type;

__END_DECLS

/* Handy macro for a common usage */

#if defined(PD_VA_MACRO_GNU)
#define alogf(sev, fmt, arg...) alog(sev, "%s: " fmt, __FUNCTION__ , ## arg)
#elif defined(PD_VA_MACRO_C99)
#define alogf(sev, fmt, ...)	\
	alog(sev, "%s: " fmt, __FUNCTION__ , ## __VA_ARGS__)
#elif defined(PD_VA_MACRO_MSVC)
#define alogf(sev, fmt, ...)	\
	alog(sev, "%s: " fmt, __FUNCTION__ , __VA_ARGS__)
#else /* workaround for compilers without variadic macros: last resort */
#define alogf(sev, fmt, args)	\
	alog(sev, "%s: " fmt, __FUNCTION__ , args)
#endif

#endif	/* _PDEL_SYS_ALOG_H_ */
