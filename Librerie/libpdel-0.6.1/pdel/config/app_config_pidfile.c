
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "pdel/pd_io.h"
#include "pdel/pd_sys.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "util/typed_mem.h"
#include "config/app_config.h"
#include "sys/alog.h"

/************************************************************************
			PIDFILE SUBSYSTEM
************************************************************************/

static app_ss_startup_t		app_pidfile_start;
static app_ss_shutdown_t	app_pidfile_stop;
static app_ss_willrun_t		app_pidfile_willrun;
static app_ss_changed_t		app_pidfile_changed;

static int			app_pidfile_fd = -1;
static int			app_pidfile_pid;

const struct app_subsystem	app_config_pidfile_subsystem = {
	"pidfile",
	NULL,
	app_pidfile_start,
	app_pidfile_stop,
	app_pidfile_willrun,
	app_pidfile_changed,
	NULL
};

/*
 * PID file startup
 */
static int
app_pidfile_start(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const char *const name = ss->arg;
	char *path;
	char buf[32];

	/* Get PID file name */
	if ((path = structs_get_string(app_config_get_type(ctx),
	    name, config, TYPED_MEM_TEMP)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_get_string");
		return (-1);
	}

	/* Create new PID file, or reset old one */
	if (app_pidfile_fd != -1) {
		alog(LOG_DEBUG, "rewriting pidfile \"%s\"", path);
		(void)pd_ftruncate(app_pidfile_fd, 0);
	} else {
		int flags = O_CREAT|O_WRONLY|O_EXLOCK|O_NONBLOCK;
#ifdef O_NOINHERIT
		/* Win32 has no F_SETFD, but we can set this at open() time */
		flags |= O_NOINHERIT;
#endif

		/* Open file */
		alog(LOG_DEBUG, "creating pidfile \"%s\"", path);
		if ((app_pidfile_fd = open(path, flags, 0644)) == -1)
		    	goto failed;
#ifdef O_EXLOCK
		goto got_lock;
#else
	    {
		struct flock f;

		memset(&f, 0, sizeof(f));
		f.l_pid = pd_getpid();
		f.l_type = F_WRLCK;
		f.l_whence = SEEK_SET;
		if (fcntl(app_pidfile_fd, F_SETLK, &f) != -1)
		    	goto got_lock;
	    }
#endif
failed:
		/* Failed to open and lock pidfile */
		switch (errno) {
#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
			alog(LOG_ERR, "%s: file locked: exiting", path);
			(void)kill(pd_getpid(), SIGTERM);
			FREE(TYPED_MEM_TEMP, path);
			return (-1);
#endif
		default:
			alog(LOG_ERR, "%s: %m", path);
			FREE(TYPED_MEM_TEMP, path);
			return (-1);
		}
got_lock:
#ifdef F_SETFD
		(void)fcntl(app_pidfile_fd, F_SETFD, 1);
#endif
		if (pd_ftruncate(app_pidfile_fd, 0) == -1) {
			alog(LOG_ERR, "%s: can't truncate: %m", path);
			FREE(TYPED_MEM_TEMP, path);
			return (-1);
		}
	}

	/* Write PID into file */
	snprintf(buf, sizeof(buf), "%ld\n", pd_getpid());
	if (write(app_pidfile_fd, buf, strlen(buf)) != strlen(buf)) {
		alog(LOG_ERR, "%s: write error: %m", path);
		(void)close(app_pidfile_fd);
		app_pidfile_fd = -1;
		FREE(TYPED_MEM_TEMP, path);
		return (-1);
	}

	/* Leave file open so it stays locked */
	FREE(TYPED_MEM_TEMP, path);
	app_pidfile_pid = pd_getpid();
	return (0);
}

/*
 * PID file shutdown
 */
static void
app_pidfile_stop(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const char *const name = ss->arg;
	char *path;

	/* Get PID file name */
	if ((path = structs_get_string(app_config_get_type(ctx),
	    name, config, TYPED_MEM_TEMP)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_get_string");
		return;
	}

	/* Remove old PID file */
	alog(LOG_DEBUG, "removing pidfile \"%s\"", path);
	(void)unlink(path);
	(void)close(app_pidfile_fd);
	app_pidfile_fd = -1;
	app_pidfile_pid = -1;
	FREE(TYPED_MEM_TEMP, path);
}

/*
 * PID file necessity check
 */
static int
app_pidfile_willrun(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config)
{
	const char *const name = ss->arg;
	char *path;
	int r;

	/* Get PID file name */
	assert(name != NULL);
	if ((path = structs_get_string(app_config_get_type(ctx),
	    name, config, TYPED_MEM_TEMP)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "structs_get_string");
		return (-1);
	}

	/* Check if not NULL */
	r = (*path != '\0');
	FREE(TYPED_MEM_TEMP, path);
	return (r);
}

/*
 * PID file configuration changed check
 */
static int
app_pidfile_changed(struct app_config_ctx *ctx,
	const struct app_subsystem *ss, const void *config1,
	const void *config2)
{
	const char *const name = ss->arg;
	int equal;

	/* Compare PID file names as well as PID itself */
	if ((equal = structs_equal(app_config_get_type(ctx),
	    name, config1, config2)) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_equal");
		return (-1);
	}
	return (!equal || pd_getpid() != app_pidfile_pid);
}

