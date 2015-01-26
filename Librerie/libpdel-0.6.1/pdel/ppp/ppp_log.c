
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"

#define LOG_MTYPE	"ppp_log"

struct ppp_log {
	void			*arg;
	u_int			refs;
	ppp_log_vput_t		*vput;
	ppp_log_close_t		*close;
};

/*
 * Create a new log.
 */
struct ppp_log *
ppp_log_create(void *arg, ppp_log_vput_t *vput, ppp_log_close_t *close)
{
	struct ppp_log *log;

	if ((log = MALLOC(LOG_MTYPE, sizeof(*log))) == NULL)
		return (NULL);
	memset(log, 0, sizeof(*log));
	log->arg = arg;
	log->vput = vput;
	log->close = close;
	log->refs = 1;
	return (log);
}

struct ppp_log *
ppp_log_dup(struct ppp_log *log)
{
	if (log == NULL)
		return (NULL);
	log->refs++;
	return (log);
}

void
ppp_log_close(struct ppp_log **logp)
{
	struct ppp_log *const log = *logp;

	if (log == NULL)
		return;
	*logp = NULL;
	assert(log->refs > 0);
	if (--log->refs == 0) {
		if (log->close != NULL)
			(*log->close)(log->arg);
		FREE(LOG_MTYPE, log);
	}
}

void
ppp_log_put(struct ppp_log *log, int sev, const char *fmt, ...)
{
	va_list args;

	if (log == NULL)
		return;
	va_start(args, fmt);
	ppp_log_vput(log, sev, fmt, args);
	va_end(args);
}

void
ppp_log_vput(struct ppp_log *log, int sev, const char *fmt, va_list args)
{
	int esave;

	if (log == NULL)
		return;
	esave = errno;
	(*log->vput)(log->arg, sev, fmt, args);
	errno = esave;
}

void
ppp_log_dump(struct ppp_log *log, int sev, const void *data, size_t len)
{
	static const int num = 16;		/* # bytes per line */
	const u_char *bytes = data;
	char buf[128];
	int i, j;

	/* Dump data */
	for (i = 0; i < ((len + num - 1) / num) * num; i += num) {
		snprintf(buf, sizeof(buf), "0x%04x  ", i);
		for (j = i; j < i + num; j++) {
			if (j < len) {
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf),
				    "%02x", bytes[j]);
			} else {
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf), "  ");
			}
			if ((j % 2) == 1) {
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf), " ");
			}
		}
		snprintf(buf + strlen(buf),
		    sizeof(buf) - strlen(buf), "       ");
		for (j = i; j < i + num; j++) {
			if (j < len) {
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf), "%c",
				    isprint(bytes[j]) ?  bytes[j] : '.');
			}
		}
		ppp_log_put(log, sev, "%s", buf);
	}
}

/***********************************************************************
			PREFIX LOG METHODS
***********************************************************************/

struct ppp_log_prefix {
	struct ppp_log	*log;
	char		*prefix;
};

/* Internal functions */
static ppp_log_vput_t	ppp_log_prefix_vput;
static ppp_log_close_t	ppp_log_prefix_close;

/*
 * Create a new log that prefixes everything before sending
 * it to another underlying log.
 *
 * The underlying log is NOT closed when the returned log is closed.
 */
struct ppp_log *
ppp_log_prefix(struct ppp_log *log, const char *fmt, ...)
{
	struct ppp_log_prefix *priv;
	va_list args;

	/* If no log or prefix, just duplicate log */
	if (log == NULL)
		return (NULL);
	if (fmt == NULL || *fmt == '\0')
		return (ppp_log_dup(log));

	/* Create prefix log */
	if ((priv = MALLOC(LOG_MTYPE, sizeof(*priv))) == NULL)
		return (NULL);
	va_start(args, fmt);
	VASPRINTF(LOG_MTYPE, &priv->prefix, fmt, args);
	va_end(args);
	if (priv->prefix == NULL) {
		FREE(LOG_MTYPE, priv);
		return (NULL);
	}

	/* Sanity check prefix: we can't handle any formats in it */
	if (strchr(priv->prefix, '%') != NULL) {
		errno = EINVAL;
		return (NULL);
	}

	/* Create new log object using prefix log methods */
	priv->log = ppp_log_dup(log);			/* can't fail */
	if ((log = ppp_log_create(priv,
	    ppp_log_prefix_vput, ppp_log_prefix_close)) == NULL) {
		ppp_log_close(&priv->log);
		FREE(LOG_MTYPE, priv->prefix);
		FREE(LOG_MTYPE, priv);
		return (NULL);
	}

	/* Done */
	return (log);
}

static void
ppp_log_prefix_vput(void *arg, int sev, const char *fmt, va_list args)
{
	struct ppp_log_prefix *const priv = arg;

	const size_t plen = strlen(priv->prefix);
	const size_t flen = strlen(fmt);
	char *pfmt;

	/* Create new format string with prefix prefixed */
	if ((pfmt = MALLOC(TYPED_MEM_TEMP, plen + flen + 1)) == NULL) {
		ppp_log_vput(priv->log, sev, fmt, args);	/* salvage */
		return;
	}
	memcpy(pfmt, priv->prefix, plen);
	memcpy(pfmt + plen, fmt, flen + 1);

	/* Log same arguments with new format string */
	ppp_log_vput(priv->log, sev, pfmt, args);
	FREE(TYPED_MEM_TEMP, pfmt);
}

static void
ppp_log_prefix_close(void *arg)
{
	struct ppp_log_prefix *const priv = arg;

	ppp_log_close(&priv->log);
	FREE(LOG_MTYPE, priv->prefix);
	FREE(LOG_MTYPE, priv);
}

