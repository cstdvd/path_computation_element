
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#ifndef WIN32
#include <sys/uio.h>
#else
#include <wtypes.h>
#include <winbase.h>
#include <io.h>
#endif
#include <sys/queue.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <syslog.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif
#include "pdel/pd_io.h"
#include "pdel/pd_regex.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/int.h"
#include "structs/type/ip4.h"
#include "structs/type/pointer.h"
#include "structs/type/string.h"
#include "structs/type/struct.h"
#include "structs/type/time.h"
#include "sys/alog.h"
#include "sys/logfile.h"
#include "util/typed_mem.h"

/* Memory allocation types */
#define MEM_TYPE		"alog"
#define MEM_TYPE_ENTRY		"alog_entry"
#define MEM_TYPE_HISTORY	"alog_history"
#define MEM_TYPE_CURCHAN	"alog_curchan"

/* Last message timeout functions */
#define LASTMSG_TIMEOUT_INITIAL	5
#define LASTMSG_TIMEOUT_1(t, d)	((t) + (d) + 1 / 2)
#define LASTMSG_TIMEOUT_2(t, d)	((t) + (d) + 1 / 2)
#define LASTMSG_TIMEOUT_3(t)	(t)

#define LASTMSG_REPEATED_FMT	"last message repeated %u times"

/* Misc defs */
#define SYSLOG_PORT		514	/* syslogd udp port */
#define MAX_NAME		64	/* max identifier length */
#define EST_ENTRY_SIZE		100	/* estimated avg. size per entry */

enum alog_type {
	ALOG_NONE = 0,			/* not initialized */
	ALOG_NULL,			/* log to nowhere */
	ALOG_STDERR,			/* log to stderr */
	ALOG_SYSLOG_LOCAL,		/* log to local machine syslog */
	ALOG_SYSLOG_REMOTE		/* log to remote machine syslog */
};

/* Current alog channel (thread-specific variable) */
struct alog_curchan {
	int		channel;
};

/* Stolen from syslog.h */
struct nameval {
	const char	*name;
	int		val;
};

/* 'Last message' state for a channel */
struct alog_lastmsg {
	char			*msg;		/* previously logged message */
	int			sev;		/* severity for 'msg' */
	time_t			when;		/* timestamp for 'msg' */
	u_int			repeat;		/* number of unlogged repeats */
	u_int			timeout;	/* timeout for logging repeat */
	time_t			timer_expiry;	/* timer running & expiry */
};

/* Structure describing one channel */
struct alog_channel {
	char			name[MAX_NAME];	/* local syslog(3) identifier */
	struct logfile		*logfile;	/* history logfile, or NULL */
	enum alog_type		type;		/* channel configuration type */
	int			sock;		/* remote syslog(3) socket */
	int			facility;	/* channel facility */
	int			min_severity;	/* min channel severity */
	u_char			debug;		/* debug enabled flag */
	pthread_mutex_t		mutex;		/* mutex, if not ALOG_NONE */
	struct alog_lastmsg	last;		/* 'last message' info */
};

/*
 * Internal variables
 */
static struct		alog_channel alog_channels[ALOG_MAX_CHANNELS];
static pthread_key_t	alog_current_channel;

/* Stolen from syslog.h */
static struct	nameval prioritynames[] = {
	{ "alert",	LOG_ALERT,	},
	{ "crit",	LOG_CRIT,	},
	{ "debug",	LOG_DEBUG,	},
	{ "emerg",	LOG_EMERG,	},
	{ "err",	LOG_ERR,	},
	{ "error",	LOG_ERR,	},	/* DEPRECATED */
	{ "info",	LOG_INFO,	},
	{ "notice",	LOG_NOTICE,	},
	{ "panic", 	LOG_EMERG,	},	/* DEPRECATED */
	{ "warn",	LOG_WARNING,	},	/* DEPRECATED */
	{ "warning",	LOG_WARNING,	},
	{ NULL,		-1,		}
};

/* Stolen from syslog.h */
static struct	nameval facilitynames[] = {
	{ "auth",	LOG_AUTH,	},
	{ "authpriv",	LOG_AUTHPRIV,	},
	{ "cron", 	LOG_CRON,	},
	{ "daemon",	LOG_DAEMON,	},
	{ "ftp",	LOG_FTP,	},
	{ "kern",	LOG_KERN,	},
	{ "lpr",	LOG_LPR,	},
	{ "mail",	LOG_MAIL,	},
	{ "news",	LOG_NEWS,	},
#ifdef LOG_NTP
	{ "ntp",	LOG_NTP,	},
#endif
#ifdef LOG_SECURITY
	{ "security",	LOG_SECURITY,	},
#endif
	{ "syslog",	LOG_SYSLOG,	},
	{ "user",	LOG_USER,	},
	{ "uucp",	LOG_UUCP,	},
	{ "local0",	LOG_LOCAL0,	},
	{ "local1",	LOG_LOCAL1,	},
	{ "local2",	LOG_LOCAL2,	},
	{ "local3",	LOG_LOCAL3,	},
	{ "local4",	LOG_LOCAL4,	},
	{ "local5",	LOG_LOCAL5,	},
	{ "local6",	LOG_LOCAL6,	},
	{ "local7",	LOG_LOCAL7,	},
	{ NULL,		-1,		}
};

/*
 * Internal functions
 */
static void	alog_write(struct alog_channel *ch,
			int sev, time_t when, const char *msg);
static void	alog_flush_lastmsg(struct alog_channel *ch, u_int timeout);
static void	alog_last_check(struct alog_channel *ch);
static int	alog_get_channel(void);
static struct	alog_curchan *alog_get_current_channel(int create);
static void	alog_init_current_channel(void);
static void	alog_curchan_destroy(void *arg);

/*
 * Initialize or reconfigure a logging channel.
 */
int
alog_configure(int channel, const struct alog_config *conf)
{
	struct alog_channel *const ch = &alog_channels[channel];
	struct sockaddr_in sin;
	int init_mutex = 0;
	int esave;
	int debug;

	/* Sanity check */
	if (channel < 0 || channel >= ALOG_MAX_CHANNELS) {
		errno = EINVAL;
		return (-1);
	}

	/* If already initialized, shut it down */
	if (ch->type != ALOG_NONE)
		alog_shutdown(channel);

	/* Initialize channel */
	debug = ch->debug;
	memset(ch, 0, sizeof(*ch));
	ch->sock = -1;
	ch->debug = debug;
	if (conf->name != NULL)
		strlcpy(ch->name, conf->name, sizeof(ch->name));
	ch->facility = (ch->debug || conf->facility == NULL) ?
	    -1 : alog_facility(conf->facility);
	ch->min_severity = conf->min_severity;

	/* Open logfile */
	if (conf->histlen > 0) {
		if ((ch->logfile = logfile_open(conf->path, 0,
		    conf->histlen, conf->histlen * EST_ENTRY_SIZE)) == NULL)
			goto fail;
	}

	/* Initialize mutex */
	if ((errno = pthread_mutex_init(&ch->mutex, NULL)) != 0)
		goto fail;
	init_mutex = 1;

	/* Handle stderr case */
	if (ch->facility == -1) {
		ch->type = ALOG_STDERR;
		return (0);
	}

	/* Handle NULL case */
	if (conf->name == NULL) {
		ch->type = ALOG_NULL;
		return (0);
	}

	/* Handle local syslog case */
	if (conf->remote_server.s_addr == 0) {
		ch->type = ALOG_SYSLOG_LOCAL;
		return (0);
	}

	/* Handle remote syslog case */
	if ((ch->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		goto fail;
#ifdef F_SETFD 
	(void)fcntl(ch->sock, F_SETFD, 1);
#endif
	memset(&sin, 0, sizeof(sin));
#ifdef HAVE_SIN_LEN
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_port = htons(SYSLOG_PORT);
	sin.sin_addr = conf->remote_server;
	if (connect(ch->sock, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		goto fail;
	if (shutdown(ch->sock, SHUT_RD) == -1)
		goto fail;
	ch->type = ALOG_SYSLOG_REMOTE;
	return (0);

fail:
	/* Clean up after failure */
	esave = errno;
	logfile_close(&ch->logfile);			/* ok if null */
	if (ch->sock != -1) {
		(void)pd_close(ch->sock);
		ch->sock = -1;
	}
	if (init_mutex)
		pthread_mutex_destroy(&ch->mutex);
	errno = esave;
	return (-1);
}

/*
 * Shutdown one logging channel.
 */
int
alog_shutdown(int channel)
{
	struct alog_channel *const ch = &alog_channels[channel];

	/* Sanity check */
	if (channel < 0 || channel >= ALOG_MAX_CHANNELS) {
		errno = EINVAL;
		return (-1);
	}

	/* Cancel timer */
	ch->last.timer_expiry = 0;

	/* Flush any repeated message */
	alog_flush_lastmsg(ch, 0);

	/* Shut down channel */
	switch (ch->type) {
	case ALOG_SYSLOG_REMOTE:
		(void)pd_close(ch->sock);
		ch->sock = -1;
		pthread_mutex_destroy(&ch->mutex);
		break;
	case ALOG_SYSLOG_LOCAL:
#ifndef WIN32
		closelog();
		pthread_mutex_destroy(&ch->mutex);
#endif
		break;
	case ALOG_NULL:
	case ALOG_STDERR:
		pthread_mutex_destroy(&ch->mutex);
		break;
	case ALOG_NONE:
		break;
	}
	ch->type = ALOG_NONE;

	/* Free saved last message */
	FREE(MEM_TYPE, ch->last.msg);
	ch->last.msg = NULL;

	/* Close logfile */
	logfile_close(&ch->logfile);			/* ok if null */

	/* Done */
	return (0);
}

/*
 * Get current channel.
 */
static int
alog_get_channel(void)
{
	struct alog_curchan *cc;

	if ((cc = alog_get_current_channel(0)) == NULL)
		return (0);
	return (cc->channel);
}

/*
 * Set active channel.
 */
int
alog_set_channel(int channel)
{
	struct alog_curchan *cc;

	/* Sanity check */
	if (channel < 0 || channel >= ALOG_MAX_CHANNELS) {
		errno = EINVAL;
		return (-1);
	}

	/* Compare with current channel */
	if ((cc = alog_get_current_channel(channel != 0)) == NULL) {
		if (channel != 0)
			return (-1);
		return (0);
	}
	if (channel == cc->channel)
		return (0);

	/* Set to new channel, even if it's uninitialized */
	cc->channel = channel;

	/* Done */
	return (0);
}

/*
 * Get current channel per-thread variable.
 */
static struct alog_curchan *
alog_get_current_channel(int create)
{
	static pthread_once_t init_channel_once = PTHREAD_ONCE_INIT;
	struct alog_curchan *cc;

	/* Initialize per-thread variable (once) */
	if ((errno = pthread_once(&init_channel_once,
	    alog_init_current_channel)) != 0) {
		fprintf(stderr, "%s: %s: %s\n",
		    "alog", "pthread_once", strerror(errno));
		return (NULL);
	}

	/* Get instance for this thread; create if necessary */
	if ((cc = pthread_getspecific(alog_current_channel)) == NULL) {
		if (!create)
			return (NULL);
		if ((cc = MALLOC(MEM_TYPE_CURCHAN, sizeof(*cc))) == NULL) {
			fprintf(stderr, "%s: %s: %s\n",
			    "alog", "malloc", strerror(errno));
			return (NULL);
		}
		cc->channel = 0;
		if ((errno = pthread_setspecific(alog_current_channel,
		    cc)) != 0) {
			fprintf(stderr, "%s: %s: %s\n",
			    "alog", "pthread_setspecific", strerror(errno));
			FREE(MEM_TYPE_CURCHAN, cc);
			return (NULL);
		}
	}

	/* Return current channel */
	return (cc);
}

/*
 * Initialize "current channel" per-thread variable.
 */
static void
alog_init_current_channel(void)
{
	int err;

	if ((err = pthread_key_create(&alog_current_channel,
	    alog_curchan_destroy)) != 0) {
		fprintf(stderr, "%s: %s: %s\n",
		    "alog", "pthread_key_create", strerror(err));
		assert(0);
	}
}

/*
 * Destroy an instance of the per-thread current channel variable.
 */
static void
alog_curchan_destroy(void *arg)
{
	struct alog_curchan *const cc = arg;

	FREE(MEM_TYPE_CURCHAN, cc);
}

/*
 * Enable debugging on a channel.
 */
void
alog_set_debug(int channel, int enabled)
{
	struct alog_channel *const ch = &alog_channels[channel];

	ch->debug = !!enabled;
}

/*
 * Log something to the currently selected channel. Preserves errno.
 */
void
alog(int sev, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	valog(sev, fmt, args);
	va_end(args);
}

/*
 * Log something to the currently selected channel. Preserves errno.
 */
void
valog(int sev, const char *fmt, va_list args)
{
	const int errno_save = errno;
	struct alog_channel *const ch = &alog_channels[alog_get_channel()];
	char fmtbuf[1024];
	time_t now;
	char *msg;
	int r;

	/* Lock channel */
	if (ch->type != ALOG_NONE) {
		r = pthread_mutex_lock(&ch->mutex);
		assert(r == 0);
	}

	/* Check last message timer */
	alog_last_check(ch);

	/* Filter out unwanted verbosity */
	if (ch->type != ALOG_NONE && sev > ch->min_severity)
		goto done;

	/* Allow for things like ``alog(LOG_DEBUG + 1, ...)'' */
	if (sev >= LOG_DEBUG)
		sev = LOG_DEBUG;

	/* Build the message */
	alog_expand(fmt, errno_save, fmtbuf, sizeof(fmtbuf));
	VASPRINTF(MEM_TYPE, &msg, fmtbuf, args);
	if (msg == NULL) {
		fprintf(stderr, "%s: %s: %s\n",
		    __FUNCTION__, "vasprintf", strerror(errno));
		goto done;
	}

	/* Get current time */
	now = time(NULL);

	/* If not configured, don't do the repeated messages thing */
	if (ch->type == ALOG_NONE) {
		alog_write(ch, sev, now, msg);
		FREE(MEM_TYPE, msg);
		goto done;
	}

	/* Handle repeated messages */
	if (ch->last.msg == NULL || strcmp(msg, ch->last.msg) != 0) {
		alog_flush_lastmsg(ch, LASTMSG_TIMEOUT_INITIAL);
		FREE(MEM_TYPE, ch->last.msg);
		ch->last.msg = msg;
		ch->last.sev = sev;
		ch->last.when = now;
		alog_write(ch, sev, now, msg);
	} else {
		time_t delay;

		delay = MAX(now - ch->last.when, 0);
		if (++ch->last.repeat == 1) {
			ch->last.timeout = LASTMSG_TIMEOUT_1(
			    ch->last.timeout, delay);
		} else {
			ch->last.timeout = LASTMSG_TIMEOUT_2(
			    ch->last.timeout, delay);
		}
		ch->last.when = now;
		ch->last.timer_expiry = now + ch->last.timeout;
		FREE(MEM_TYPE, msg);
	}

done:
	/* Unlock channel and restore errno */
	if (ch->type != ALOG_NONE) {
		r = pthread_mutex_unlock(&ch->mutex);
		assert(r == 0);
	}
	errno = errno_save;
}

/*
 * Check for expiration of 'last mesasge' timer.
 *
 * This assumes the channel is locked.
 */
static void
alog_last_check(struct alog_channel *ch)
{
	/* Is the timer "running"? */
	if (ch->last.timer_expiry == 0)
		return;

	/* Has the timer "expired"? */
	if (ch->last.timer_expiry > time(NULL))
		return;

	/* "Stop" timer */
	ch->last.timer_expiry = 0;

	/* Flush last message */
	alog_flush_lastmsg(ch, LASTMSG_TIMEOUT_3(ch->last.timeout));
}

/*
 * Log repeated message(s).
 *
 * This assumes the channel is locked.
 */
static void
alog_flush_lastmsg(struct alog_channel *ch, u_int timeout)
{
	switch (ch->last.repeat) {
	case 0:
		break;
	case 1:
		if (ch->last.msg != NULL) {
			alog_write(ch, ch->last.sev,
			    ch->last.when, ch->last.msg);
		}
		break;
	default:
	    {
		char buf[sizeof(LASTMSG_REPEATED_FMT) + 32];

		snprintf(buf, sizeof(buf),
		    LASTMSG_REPEATED_FMT, ch->last.repeat);
		alog_write(ch, ch->last.sev, ch->last.when, buf);
		break;
	    }
	}
	ch->last.repeat = 0;
	ch->last.timeout = timeout;
	ch->last.timer_expiry = 0;
}

/*
 * Write a message to the log.
 *
 * This assumes the channel is locked.
 */
static void
alog_write(struct alog_channel *ch, int sev, time_t when, const char *msg)
{
	/* Log it to wherever */
	switch (ch->type) {
	case ALOG_NONE:
	case ALOG_STDERR:
		fprintf(stderr, "%s: %s\n",
		    alog_severity_name(LOG_PRI(sev)), msg);
		break;
	case ALOG_NULL:
		break;
	case ALOG_SYSLOG_LOCAL:
#ifndef WIN32
		openlog(ch->name, 0, ch->facility);	/* XXX race condition */
		syslog(sev, "%s", msg);			/* XXX race condition */
#endif
		break;
	case ALOG_SYSLOG_REMOTE:
	    {
		char ascbuf[32];
		struct tm tm;
		char *netmsg;
		int len;

		if (ch->sock == -1)
			break;
		gmtime_r(&when, &tm);
		asctime_r(&tm, ascbuf);
		len = ASPRINTF(TYPED_MEM_TEMP, &netmsg, "<%d>%.15s %s",
		       LOG_MAKEPRI(ch->facility, sev) _ ascbuf + 4 _ msg);
		if (netmsg != NULL)
			pd_write(ch->sock, netmsg, len);
		FREE(TYPED_MEM_TEMP, netmsg);
		break;
	    }
	}

	/* Save message in history buffer */
	if (ch->logfile != NULL) {
		struct alog_entry *ent;
		int mlen;

		/* Setup entry */
		mlen = strlen(msg) + 1;
		if ((ent = MALLOC(MEM_TYPE, sizeof(*ent) + mlen)) == NULL) {
			fprintf(stderr, "%s: %s: %s\n",
			    __FUNCTION__, "malloc", strerror(errno));
			return;
		}
		ent->when = when;
		ent->sev = LOG_PRI(sev);
		memcpy(ent->msg, msg, mlen);

		/* Add to history file */
		if (logfile_put(ch->logfile, ent, sizeof(*ent) + mlen) == -1) {
			fprintf(stderr, "%s: %s: %s\n",
			    __FUNCTION__, "logfile_put", strerror(errno));
		}
		FREE(MEM_TYPE, ent);
	}
}

/*
 * Get channel history
 */
int
alog_get_history(int channel, int min_sev, int max_num, time_t max_age,
	const regex_t *preg, struct alog_history *list)
{
	struct alog_channel *const ch = &alog_channels[channel];
	struct alog_entry *copy;
	regmatch_t pmatch;
	u_int num_ent;
	int rtn = -1;
	time_t now;
	int i, j;
	u_int num;
	int r;

	/* Sanity check */
	if (channel < 0 || channel >= ALOG_MAX_CHANNELS) {
		errno = EINVAL;
		return (-1);
	}

	/* Lock channel */
	if (ch->type != ALOG_NONE) {
		r = pthread_mutex_lock(&ch->mutex);
		assert(r == 0);
	}

	/* Check last message timer */
	alog_last_check(ch);

	/* Initialize array */
	memset(list, 0, sizeof(*list));

	/* Nothing to return? */
	if (ch->logfile == NULL
	    || max_num == 0
	    || (num_ent = logfile_num_entries(ch->logfile)) == 0) {
		rtn = 0;
		goto done;
	}

	/* Get current time */
	now = time(NULL);

#define MATCH(sev, when, msg, msglen)					\
	(sev <= min_sev && (now - when) <= max_age && (preg == NULL	\
	  || (pmatch.rm_so = 0, pmatch.rm_eo = msglen,			\
	    pd_regexec(preg, msg, 0, &pmatch, REG_STARTEND) == 0)))

	/* Count how many entries we'll be returning */
	num = 0;
	if (ch->last.repeat > 0
	    && ch->last.msg != NULL
	    && MATCH(ch->last.sev, ch->last.when,
	      ch->last.msg, strlen(ch->last.msg)))
		num++;
	for (i = -1; num < max_num && i >= (- (int)num_ent); i--) {
		const struct alog_entry *ent;
		int len;

		/* Get entry, check severity and age */
		if ((ent = logfile_get(ch->logfile, i, &len)) != NULL
		    && len >= sizeof(*ent) + 1
		    && MATCH(ent->sev, ent->when,
		      ent->msg, len - sizeof(*ent) - 1))
			num++;
	}
	if (num == 0) {
		rtn = 0;
		goto done;
	}

	/* Allocate array of pointers */
	if ((list->elems = MALLOC(MEM_TYPE_HISTORY,
	    num * sizeof(*list->elems))) == NULL)
		goto done;

	/* Fill array, starting with the most recent first */
	j = num;
	if (ch->last.repeat > 0
	    && ch->last.msg != NULL
	    && MATCH(ch->last.sev, ch->last.when,
	      ch->last.msg, strlen(ch->last.msg))) {
		char buf[sizeof(LASTMSG_REPEATED_FMT) + 32];
		const char *s;

		if (ch->last.repeat > 1) {
			snprintf(buf, sizeof(buf),
			    LASTMSG_REPEATED_FMT, ch->last.repeat);
			s = buf;
		} else
			s = ch->last.msg;
		if ((copy = MALLOC(MEM_TYPE_ENTRY,
		    sizeof(*copy) + strlen(s) + 1)) != NULL) {
			copy->when = ch->last.when;
			copy->sev = ch->last.sev;
			strcpy(copy->msg, s);
			list->elems[--j] = copy;
		}
	}
	for (i = -1; j > 0 && i >= (-(int)num_ent); i--) {
		const struct alog_entry *ent;
		int len;

		/* Copy it if it matches */
		if ((ent = logfile_get(ch->logfile, i, &len)) != NULL
		    && len >= sizeof(*ent) + 1
		    && MATCH(ent->sev, ent->when,
		      ent->msg, len - sizeof(*ent) - 1)) {
			if ((copy = MALLOC(MEM_TYPE_ENTRY, len)) != NULL) {
				memcpy(copy, ent, len - 1);
				((char *)copy)[len - 1] = '\0';	/* safety */
				list->elems[--j] = copy;
			}
		}
	}

	/* Collapse entries dropped because of memory problems or whatever */
	if (j > 0) {
		num -= j;
		memcpy(list->elems, list->elems + j,
		    num * sizeof(*list->elems));
	}
	list->length = num;
	rtn = 0;

done:
	/* Unlock channel and return */
	if (ch->type != ALOG_NONE) {
		r = pthread_mutex_unlock(&ch->mutex);
		assert(r == 0);
	}
	return (rtn);
}

/*
 * Clear log history.
 */
int
alog_clear_history(int channel)
{
	struct alog_channel *const ch = &alog_channels[channel];
	int r;

	/* Sanity check */
	if (channel < 0 || channel >= ALOG_MAX_CHANNELS) {
		errno = EINVAL;
		return (-1);
	}

	/* Lock channel */
	if (ch->type != ALOG_NONE) {
		r = pthread_mutex_lock(&ch->mutex);
		assert(r == 0);
	}

	/* Reset last message state */
	ch->last.timer_expiry = 0;
	FREE(MEM_TYPE, ch->last.msg);
	memset(&ch->last, 0, sizeof(ch->last));

	/* Zero out logfile */
	if (ch->logfile != NULL)
		logfile_trim(ch->logfile, 0);

	/* Unlock channel */
	if (ch->type != ALOG_NONE) {
		r = pthread_mutex_unlock(&ch->mutex);
		assert(r == 0);
	}

	/* Done */
	return (0);
}

/*
 * Decode syslog facility name
 */
int
alog_facility(const char *name)
{
	int i;

	for (i = 0; facilitynames[i].name != NULL; i++) {
		if (strcmp(name, facilitynames[i].name) == 0)
			return (facilitynames[i].val);
	}
	return (-1);
}

/*
 * Decode syslog facility name
 */
const char *
alog_facility_name(int facility)
{
	int i;

	for (i = 0; facilitynames[i].name != NULL; i++) {
		if (facilitynames[i].val == facility)
			return (facilitynames[i].name);
	}
	return (NULL);
}

/*
 * Decode syslog severity name
 */
int
alog_severity(const char *name)
{
	char *eptr;
	int i;

	for (i = 0; prioritynames[i].name != NULL; i++) {
		if (strcmp(name, prioritynames[i].name) == 0)
			return (prioritynames[i].val);
	}
	i = (int)strtol(name, &eptr, 0);
	if (*name != '\0' && *eptr == '\0')
		return (i);
	return (-1);
}

/*
 * Decode syslog severity name
 */
const char *
alog_severity_name(int severity)
{
	int i;

	for (i = 0; prioritynames[i].name != NULL; i++) {
		if (prioritynames[i].val == severity)
			return (prioritynames[i].name);
	}
	return (NULL);
}

/*
 * Kludge to deal with %m format when not using syslog
 */
void
alog_expand(const char *fmt, int errnum, char *buf, size_t bufsize)
{
	const char *errstr;
	const char *s;
	int errlen;
	int i;

	/* Find "%m" which is usually at the end */
	for (s = fmt + strlen(fmt) - 2;
	    s >= fmt && !(s[0] == '%' && s[1] == 'm');
	    s--);

	/* Check if we should bother doing anything */
	if (s < fmt || (i = s - fmt) > bufsize - 2) {
		strlcpy(buf, fmt, bufsize);
		return;
	}

	/* Convert "%m" to error string */
	errstr = strerror(errnum);
	errlen = strlen(errstr);
	strlcpy(buf, fmt, i + 1);
	strlcpy(buf + i, errstr, bufsize - i);
	strlcpy(buf + i + errlen, fmt + i + 2, bufsize - i - errlen);
}

/*********************************************************************
			STRUCTS TYPE DEFINITIONS
*********************************************************************/

/* Type for "path" and "name" strings in struct alog_config */
static const struct structs_type alog_path_type
	= STRUCTS_STRING_TYPE("alog_config.path", 1);
static const struct structs_type alog_name_type
	= STRUCTS_STRING_TYPE("alog_config.name", 1);

/* Fields list for struct alog_config */
static const struct structs_field alog_config_fields[] = {
	STRUCTS_STRUCT_FIELD(alog_config, path, &alog_path_type),
	STRUCTS_STRUCT_FIELD(alog_config, name, &alog_name_type),
	STRUCTS_STRUCT_FIELD(alog_config, facility, &alog_facility_type),
	STRUCTS_STRUCT_FIELD(alog_config, remote_server, &structs_type_ip4),
	STRUCTS_STRUCT_FIELD(alog_config, min_severity, &alog_severity_type),
	STRUCTS_STRUCT_FIELD(alog_config, histlen, &structs_type_int),
	STRUCTS_STRUCT_FIELD_END
};

/* Type for struct alog_config */
const struct structs_type alog_config_type
	= STRUCTS_STRUCT_TYPE(alog_config, &alog_config_fields);

/* Override method for alog_facility_type */
static structs_init_t		alog_facility_type_init;
static structs_binify_t		alog_facility_type_binify;

/* Type for syslog facility, which we store as a string. */
const struct structs_type alog_facility_type = {
	sizeof(char *),
	"alog",
	STRUCTS_TYPE_PRIMITIVE,
	alog_facility_type_init,
	structs_ascii_copy,
	structs_string_equal,
	structs_string_ascify,
	alog_facility_type_binify,
	structs_string_encode,
	structs_string_decode,
	structs_string_free,
	{ { (void *)"alog_config.facility" }, { (void *)1 } }
};

/*
 * Initializer for alog_facility_type
 */
static int
alog_facility_type_init(const struct structs_type *type, void *data)
{
	return (structs_string_binify(type, "daemon", data, NULL, 0));
}

/*
 * Binifier for alog_facility_type
 */
static int
alog_facility_type_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	if (*ascii != '\0' && alog_facility(ascii) == -1) {
		strlcpy(ebuf, "invalid syslog facility", emax);
		errno = EINVAL;
		return (-1);
	}
	return (structs_string_binify(type, ascii, data, NULL, 0));
}

/* Override methods for alog_severity_type */
static structs_init_t		alog_severity_init;
static structs_ascify_t		alog_severity_ascify;
static structs_binify_t		alog_severity_binify;

/* Type for severity, which defaults to LOG_INFO. The input can be either
   a symbolic name or a numeric value. */
const struct structs_type alog_severity_type = {
	sizeof(int),
	"int",
	STRUCTS_TYPE_PRIMITIVE,
	alog_severity_init,
	structs_region_copy,
	structs_region_equal,
	alog_severity_ascify,
	alog_severity_binify,
	structs_region_encode_netorder,
	structs_region_decode_netorder,
	structs_nothing_free,
	{ { (void *)2 }, { (void *)1 } }
};

/*
 * Initializer for severity
 */
static int
alog_severity_init(const struct structs_type *type, void *data)
{
	*((int *)data) = LOG_INFO;
	return (0);
}

/*
 * Ascifier for severity
 */
static char *
alog_severity_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const int sev = *((int *)data);
	const char *s;
	char buf[32];

	if ((s = alog_severity_name(sev)) == NULL) {
		snprintf(buf, sizeof(buf), "%d", sev);
		s = buf;
	}
	return (STRDUP(mtype, s));
}

/*
 * Binifier for severity
 */
static int
alog_severity_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	int sev;

	if ((sev = alog_severity(ascii)) == -1
	    && structs_int_binify(type, ascii, data, NULL, 0) == -1) {
		strlcpy(ebuf, "invalid syslog severity", emax);
		return (-1);
	}
	*((int *)data) = sev;
	return (0);
}

/*
 * Ascifier for log entry message.
 */
static char *
alog_entry_msg_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	return (STRDUP(mtype, data));
}

/* Type for log entry message. This is a "read only" type. */
static const struct structs_type alog_entry_msg_type = {
	0,
	"alog_entry_msg_type",
	STRUCTS_TYPE_PRIMITIVE,
	structs_notsupp_init,
	structs_notsupp_copy,
	structs_notsupp_equal,
	alog_entry_msg_ascify,
	structs_notsupp_binify,
	structs_notsupp_encode,
	structs_notsupp_decode,
	structs_nothing_free,
};

/* Type for history list: an array of pointers to log entries */
static const struct structs_field alog_entry_fields[] = {
	STRUCTS_STRUCT_FIELD(alog_entry, when, &structs_type_time_abs),
	STRUCTS_STRUCT_FIELD(alog_entry, sev, &alog_severity_type),
	STRUCTS_STRUCT_FIELD(alog_entry, msg, &alog_entry_msg_type),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type alog_entry_type
	= STRUCTS_STRUCT_TYPE(alog_entry, &alog_entry_fields);
static const struct structs_type alog_history_ptr_type
	= STRUCTS_POINTER_TYPE(&alog_entry_type, MEM_TYPE_ENTRY);
const struct structs_type alog_history_type
	= STRUCTS_ARRAY_TYPE(&alog_history_ptr_type, MEM_TYPE_HISTORY, "entry");

