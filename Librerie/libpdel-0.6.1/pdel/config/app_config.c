
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include <unistd.h>

#include <pthread.h>

#include "pdel/pd_string.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/xml.h"
#include "sys/alog.h"
#include "util/pevent.h"
#include "util/typed_mem.h"
#include "config/app_config.h"

#ifdef WIN32
#include <wtypes.h>
#include <io.h>
#endif

/************************************************************************
				DEFINITIONS
************************************************************************/

#define MEM_TYPE		"app_config"

#define ATTR_VERSION		"version"

/* Info passed to restart thread */
struct app_restart {
	u_int		delay;
	u_char		have_mutex;
	u_char		writeback;
	void		*config;
};

#define SS_NEED_STOP	0x01
#define SS_NEED_START	0x02
#define SS_RUNNING	0x04

/* Configuration state flags */
#define CONFIG_PENDING		0x0001		/* c->pending is valid */
#define CONFIG_APPLYING		0x0002		/* c->applying is valid */
#define CONFIG_RESTARTING	0x0004		/* a thread is doing restarts */

/* Application configuration state */
struct app_config_ctx {
	struct pevent_ctx	*ctx;		/* event context */
	struct app_config	info;		/* application info */
	void			*cookie;	/* application cookie */
	int			flags;		/* state flags */
	const struct structs_type *type;	/* config structs type */
	int			num_ss;		/* length of info.slist */
	void			*current;	/* current configuration */
	void			*pending;	/* pending new config */
	void			*applying;	/* config being applied */
	u_char			*ss_flags;	/* subsystem state flags */
	char			*xml_path;	/* xml file pathname */
	int			xml_writeback;	/* allow xml writeback */
	void			*xml_cache;	/* cached xml file contents */
	pthread_mutex_t		mutex;		/* mutex for config state */
	struct pevent		*timer;		/* pending restart timer */
};

/************************************************************************
				FUNCTIONS
************************************************************************/

/*
 * Internal functions
 */
static const	void *app_config_get_last(struct app_config_ctx *c);
static int	app_config_equal(struct app_config_ctx *c,
			const void *config1, const void *config2);
static void	app_config_restart(struct app_config_ctx *c);
static void	app_config_store(struct app_config_ctx *c);

static pevent_handler_t		app_config_apply;
static structs_xmllog_t		app_config_xml_logger;

/*
 * Initialize the application configuration framework.
 */
struct app_config_ctx *
app_config_init(struct pevent_ctx *ctx,
	const struct app_config *info, void *cookie)
{
	struct app_config_ctx *c;
	int i, j;
	u_int u;

	/* Sanity check */
	for (u = 0; u <= info->version; u++) {
		if (info->types[u] == NULL) {
			errno = EINVAL;
			return (NULL);
		}
	}

	/* Check each subsystem has a unique name */
	for (i = 0; info->slist[i] != NULL; i++) {
		for (j = i + 1; info->slist[j] != NULL; j++) {
			if (strcmp(info->slist[i]->name,
			    info->slist[j]->name) == 0) {
				errno = EINVAL;
				return (NULL);
			}
		}
	}

	/* Create new state object */
	if ((c = MALLOC(MEM_TYPE, sizeof(*c))) == NULL) {
		alogf(LOG_ERR, "%s: %m" , "malloc");
		return (NULL);
	}
	memset(c, 0, sizeof(*c));
	c->ctx = ctx;
	c->cookie = cookie;

	/* Copy application config */
	c->info = *info;
	c->type = c->info.types[c->info.version];
	c->info.slist = NULL;

	/* Copy subsystem list */
	for (c->num_ss = 0; info->slist[c->num_ss] != NULL; c->num_ss++);
	if ((c->info.slist = MALLOC(MEM_TYPE,
	    c->num_ss * sizeof(*c->info.slist))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		goto fail;
	}
	memcpy((struct app_subsystem	**) c->info.slist,
	    info->slist, c->num_ss * sizeof(*c->info.slist));

	/* Allocate subsystem flags list */
	if ((c->ss_flags = MALLOC(MEM_TYPE,
	    c->num_ss * sizeof(*c->ss_flags))) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		goto fail;
	}
	memset(c->ss_flags, 0, c->num_ss * sizeof(*c->ss_flags));

	/* Initialize mutex */
	if ((errno = pthread_mutex_init(&c->mutex, NULL)) != 0) {
		alogf(LOG_ERR, "%s: %m", "pthread_mutex_init");
		goto fail;
	}

	/* Done */
	return (c);

fail:
	/* Clean up after failure */
	FREE(MEM_TYPE, c->ss_flags);
	FREE(MEM_TYPE, (struct app_subsystem	**) c->info.slist);
	FREE(MEM_TYPE, c);
	return (NULL);
}

/*
 * Shutdown and free application configuration stuff.
 */
int
app_config_uninit(struct app_config_ctx **cp)
{
	struct app_config_ctx *const c = *cp;
	int r;

	/* Sanity check */
	if (c == NULL)
		return (0);

	/* Acquire mutex */
	r = pthread_mutex_lock(&c->mutex);
	assert(r == 0);

	/* Check that nothing is happening and everything is shut down */
	if (c->current != NULL
	    || (c->flags & (CONFIG_PENDING|CONFIG_RESTARTING)) != 0) {
		r = pthread_mutex_unlock(&c->mutex);
		assert(r == 0);
		errno = EBUSY;
		return (-1);
	}

	/* Nuke caller's reference, release mutex */
	*cp = NULL;
	r = pthread_mutex_unlock(&c->mutex);
	assert(r == 0);

	/* Destroy config state */
	pevent_unregister(&c->timer);		/* not really necessary */
	app_config_free(c, &c->xml_cache);
	FREE(MEM_TYPE, c->xml_path);
	pthread_mutex_destroy(&c->mutex);
	FREE(MEM_TYPE, c->ss_flags);
	FREE(MEM_TYPE, (struct app_subsystem	**) c->info.slist);
	FREE(MEM_TYPE, c);

	/* Done */
	return (0);
}

/*
 * Get the application cookie.
 */
void *
app_config_get_cookie(struct app_config_ctx *c)
{
	return (c->cookie);
}

/*
 * Get the configuration object type.
 */
const struct structs_type *
app_config_get_type(struct app_config_ctx *c)
{
	return (c->type);
}

/*
 * Load in application configuration from an XML file and apply it.
 * The named file becomes the implicit file for app_config_reload().
 *
 * If the file does not exist, one is automatically created after
 * calling the applications "getnew" routine.
 */
int
app_config_load(struct app_config_ctx *c, const char *path, int writeback)
{
	void *old_config = NULL;
	char *attrs = NULL;
	u_int old_version;
	FILE *fp = NULL;
	struct stat sb;
	int old_cstate;
	char ebuf[128];
	void *config;
	int rtn = -1;
	char *s;
	int r;

	/* Block thread from being canceled */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_cstate);

	/* Create a new configuration with the default values */
	if ((config = app_config_new(c)) == NULL)
		goto fail;

	/* If configuration file is there but empty, remove it */
	if (stat(path, &sb) == 0 && sb.st_size == 0) {
		char resolved_path[MAXPATHLEN];

		if (pd_realpath(path, resolved_path) == NULL)
			goto fail;
		(void)unlink(resolved_path);
	}

	/* If configuration file doesn't exist, create one */
	if (stat(path, &sb) == -1) {

		/* Bail on any wierd errors */
		if (errno != ENOENT)
			goto fail;

		/* Create new configuration */
		if (c->info.getnew != NULL
		    && (*c->info.getnew)(c, config) == -1)
			goto fail;

		/* Normalize it */
		if (c->info.normalize != NULL)
			(*c->info.normalize)(c, config);
		goto ready;
	}

	/* Open XML config file */
	if ((fp = fopen(path, "r")) == NULL)
		goto fail;

	/* Read top level XML attributes only */
	r = structs_xml_input(NULL, APP_CONFIG_XML_TAG, &attrs,
	    TYPED_MEM_TEMP, fp, NULL, STRUCTS_XML_SCAN, app_config_xml_logger);
	if (r == -1) {
		alog(LOG_ERR, "error reading configuration from \"%s\"", path);
		goto fail;
	}

	/* Find the version number attribute and parse version */
	for (s = attrs, old_version = 0; *s != '\0'; s += strlen(s) + 1) {
		u_long vers;
		char *eptr;
		int match;

		match = (strcmp(s, ATTR_VERSION) == 0);
		s += strlen(s) + 1;
		if (!match)
			continue;
		vers = strtoul(s, &eptr, 10);
		if (*s == '\0' || *eptr != '\0') {
			alog(LOG_ERR, "\"%s\" contains invalid version \"%s\"",
			    path, s);
			FREE(TYPED_MEM_TEMP, attrs);
			errno = EPROGMISMATCH;
			goto fail;
		}
		old_version = vers;
		break;
	}
	FREE(TYPED_MEM_TEMP, attrs);

	/* Sanity check version and rewind file */
	if (old_version > c->info.version) {
		alog(LOG_ERR, "\"%s\" contains newer version %d > %u",
		    path, old_version, c->info.version);
		errno = EPROGMISMATCH;
		goto fail;
	}
	if (fseek(fp, 0, SEEK_SET) == -1)
		goto fail;

	/* Upgrade older format if necessary */
	if (old_version < c->info.version) {
		const struct structs_type *const old_type
		    = c->info.types[old_version];

		/* Do we have an upgrade method? */
		if (c->info.upgrade == NULL) {
			alog(LOG_ERR,
			    "\"%s\" contains obsolete version %d < %u",
			    path, old_version, c->info.version);
			errno = EPROGMISMATCH;
			goto fail;
		}

		/* Log it */
		alog(LOG_INFO, "\"%s\" contains old version %d < %u, upgrading",
		    path, old_version, c->info.version);

		/* Create an (uninitialized) old config structure */
		if ((old_config = MALLOC(MEM_TYPE, old_type->size)) == NULL)
			goto fail;

		/* Read in the old version XML (and initialize) */
		r = structs_xml_input(old_type, APP_CONFIG_XML_TAG, NULL,
		    TYPED_MEM_TEMP, fp, old_config,
		    STRUCTS_XML_UNINIT | STRUCTS_XML_LOOSE,
		    app_config_xml_logger);
		fclose(fp);
		fp = NULL;
		if (r == -1) {
			alog(LOG_ERR,
			    "error reading configuration from \"%s\"", path);
			FREE(MEM_TYPE, old_config);
			goto fail;
		}

		/* Upgrade from the old version to the new version */
		r = (*c->info.upgrade)(c, old_config, old_version, config);
		structs_free(old_type, NULL, old_config);
		FREE(MEM_TYPE, old_config);
		if (r == -1) {
			alog(LOG_ERR,
			    "error upgrading configuration \"%s\" version"
			    " from %d -> %u", path,
			    old_version, c->info.version);
			goto fail;
		}
	} else {

		/* Version is correct, read in configuration */
		r = structs_xml_input(c->type, APP_CONFIG_XML_TAG, NULL,
		    TYPED_MEM_TEMP, fp, config, STRUCTS_XML_LOOSE,
		    app_config_xml_logger);
		fclose(fp);
		fp = NULL;
		if (r == -1) {
			alog(LOG_ERR,
			    "error reading configuration from \"%s\"", path);
			goto fail;
		}
	}

ready:
	/* Remember path and writeback settings */
	r = pthread_mutex_lock(&c->mutex);
	assert(r == 0);
	FREE(MEM_TYPE, c->xml_path);
	if ((c->xml_path = STRDUP(MEM_TYPE, path)) == NULL)
		alogf(LOG_ERR, "%s: %m", "strdup");
	app_config_free(c, &c->xml_cache);
	c->xml_writeback = writeback;
	r = pthread_mutex_unlock(&c->mutex);
	assert(r == 0);

	/* Apply configuration read from file */
	if ((rtn = app_config_set(c, config, 0, ebuf, sizeof(ebuf))) == -1) {
		alog(LOG_ERR, "error applying configuration from \"%s\": %s",
		    path, ebuf);
	}

fail:
	/* Done */
	app_config_free(c, &config);
	if (fp != NULL)
		fclose(fp);
	pthread_setcancelstate(old_cstate, NULL);
	return (rtn);
}

/*
 * Re-read the XML file passed to app_config_load() and reconfigure
 * as appropriate.
 */
int
app_config_reload(struct app_config_ctx *c)
{
	int writeback;
	char *path;
	int rtn;
	int r;

	/* Get the relevant info */
	r = pthread_mutex_lock(&c->mutex);
	assert(r == 0);
	if (c->xml_path == NULL) {
		r = pthread_mutex_unlock(&c->mutex);
		assert(r == 0);
		errno = ENXIO;
		return (-1);
	}
	if ((path = STRDUP(TYPED_MEM_TEMP, c->xml_path)) == NULL) {
		r = pthread_mutex_unlock(&c->mutex);
		assert(r == 0);
		return (-1);
	}
	writeback = c->xml_writeback;
	r = pthread_mutex_unlock(&c->mutex);
	assert(r == 0);

	/* Apply it */
	rtn = app_config_load(c, path, writeback);

	/* Done */
	FREE(TYPED_MEM_TEMP, path);
	return (rtn);
}

/*
 * Function to create a configuration structure. This structure will
 * have the application's default values.
 */
void *
app_config_new(struct app_config_ctx *c)
{
	static int did_check;
	void *config;

	/* Create initialized structure */
	if ((config = MALLOC(MEM_TYPE, c->type->size)) == NULL)
		return (NULL);
	if (structs_init(c->type, NULL, config) == -1) {
		FREE(MEM_TYPE, config);
		return (NULL);
	}

	/* Apply application defaults */
	if (c->info.init != NULL && (*c->info.init)(c, config) == -1) {
		structs_free(c->type, NULL, config);
		FREE(MEM_TYPE, config);
		return (NULL);
	}

	/* Sanity check that default configuration is really valid */
	if (!did_check) {
		char buf[512];

		snprintf(buf, sizeof(buf), "generic problem");
		if (c->info.checker != NULL
		    && !(*c->info.checker)(c, config, buf, sizeof(buf))) {
			alog(LOG_CRIT,
			    "default configuration is invalid: %s", buf);
		}
		did_check = 1;
	}
	return (config);
}

/*
 * Get the application's most recently applied configuration.
 *
 * This assumes the mutex is held.
 */
static const void *
app_config_get_last(struct app_config_ctx *c)
{
	return (((c->flags & CONFIG_PENDING) != 0) ? c->pending
	    : ((c->flags & CONFIG_APPLYING) != 0) ? c->applying
	    : c->current);
}

/*
 * Compare two configurations, either of which may be NULL.
 */
static int
app_config_equal(struct app_config_ctx *c,
	const void *config1, const void *config2)
{
	if (config1 == NULL && config2 == NULL)
		return (1);
	if (config1 == NULL || config2 == NULL)
		return (0);
	return (structs_equal(c->type, NULL, config1, config2));
}

/*
 * Get a copy of the application's current or pending configuration.
 */
void *
app_config_get(struct app_config_ctx *c, int pending)
{
	void *copy;
	int r;

	/* Get copy of config */
	r = pthread_mutex_lock(&c->mutex);
	assert(r == 0);
	copy = app_config_copy(c, pending ?
	    app_config_get_last(c) : c->current);
	r = pthread_mutex_unlock(&c->mutex);
	assert(r == 0);

	/* Done */
	return (copy);
}

/*
 * Get a copy of a configuration.
 */
void *
app_config_copy(struct app_config_ctx *c, const void *config)
{
	void *copy;

	if (config == NULL)
		return (NULL);
	if ((copy = MALLOC(MEM_TYPE, c->type->size)) == NULL) {
		alogf(LOG_ERR, "%s: %m", "malloc");
		return (NULL);
	}
	if (structs_get(c->type, NULL, config, copy) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_get");
		FREE(MEM_TYPE, copy);
		return (NULL);
	}
	return (copy);
}

/*
 * Free a configuration.
 */
void
app_config_free(struct app_config_ctx *c, void **configp)
{
	void *const config = *configp;

	if (config == NULL)
		return;
	*configp = NULL;
	structs_free(c->type, NULL, config);
	FREE(MEM_TYPE, config);
}

/*
 * Change the application's current configuration.
 */
int
app_config_set(struct app_config_ctx *c,
	const void *config0, u_long delay, char *ebuf, int emax)
{
	struct pevent_info info;
	char buf[512];
	void *config;
	int rtn = 0;
	int r;

	/* Allow NULL error buffer to be passed; initialize buffer */
	if (ebuf == NULL) {
		ebuf = buf;
		emax = sizeof(buf);
	}
	snprintf(ebuf, emax, "unknown problem");

	/* Copy config */
	if (config0 == NULL)
		config = NULL;
	else if ((config = app_config_copy(c, config0)) == NULL)
		return (-1);

	/* Normalize it */
	if (config != NULL && c->info.normalize != NULL)
		(*c->info.normalize)(c, config);

	/* Validate the new configuration */
	if (config != NULL
	    && c->info.checker != NULL
	    && !(*c->info.checker)(c, config, ebuf, emax)) {
		app_config_free(c, &config);
		errno = EINVAL;
		return (-1);
	}

	/* Acquire mutex */
	r = pthread_mutex_lock(&c->mutex);
	assert(r == 0);

	/* Ignore new configs while a shutdown is pending */
	if ((c->timer != NULL && c->pending == NULL)
	    || ((c->flags & CONFIG_RESTARTING) != 0 && c->applying == NULL)) {
		app_config_free(c, &config);
		goto done;
	}

	/* See if new config is really new */
	if (app_config_equal(c, config, app_config_get_last(c))) {
		app_config_free(c, &config);	/* same as what we got */
		goto done;
	}

	/* Update pending configuration */
	app_config_free(c, &c->pending);
	c->pending = config;
	c->flags |= CONFIG_PENDING;

	/* If a previous restart thread is still running, we're done */
	if ((c->flags & CONFIG_RESTARTING) != 0)
		goto done;

	/* If the restart timer is not running, need to start it */
	if (c->timer == NULL)
		goto start_timer;

	/* Get time remaining on the restart timer */
	if (pevent_get_info(c->timer, &info) == -1) {
		alogf(LOG_ERR, "%s: %m", "pevent_get_info");
		goto start_timer;
	}

	/* See if we need a shorter delay */
	if (delay > INT_MAX || delay >= (u_int)info.u.millis)
		goto done;		/* existing timeout is sufficient */

start_timer:
	/* Cancel previous timer */
	pevent_unregister(&c->timer);

	/* Start restart timer */
	if (pevent_register(c->ctx, &c->timer, PEVENT_OWN_THREAD,
	    &c->mutex, app_config_apply, c, PEVENT_TIME, delay) == -1) {
		alogf(LOG_ERR, "%s: %m", "pevent_register");
		rtn = -1;
		goto done;
	}

done:
	/* Done */
	r = pthread_mutex_unlock(&c->mutex);
	assert(r == 0);
	return (rtn);
}

/*
 * Write the current configuration back out to XML config file.
 *
 * This assumes c->current != NULL and the mutex is locked.
 */
static void
app_config_store(struct app_config_ctx *c)
{
	char attrs[sizeof(ATTR_VERSION) + 32];
	char *tpath = NULL;
	FILE *fp = NULL;
	int old_cstate;

	/* Get configuration and XML pathname */
	assert(c->xml_path != NULL);

	/* Block thread from being canceled */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_cstate);

	/* Check if configuration is different from cached file contents */
	if (c->xml_cache != NULL
	    && structs_equal(c->type, NULL, c->current, c->xml_cache))
		goto done;

	/* Set version attribute */
	snprintf(attrs, sizeof(attrs), "%s%c%u%c",
	    ATTR_VERSION, '\0', c->info.version, '\0');

	/* Write out temporary new file */
	ASPRINTF(TYPED_MEM_TEMP, &tpath, "%s.new", c->xml_path);
	if (tpath == NULL) {
		alogf(LOG_ERR, "%s: %m", "asprintf");
		goto done;
	}
	if ((fp = fopen(tpath, "w")) == NULL) {
		alogf(LOG_ERR, "%s: %m", tpath);
		goto done;
	}
	if (structs_xml_output(c->type, APP_CONFIG_XML_TAG,
	    attrs, c->current, fp, NULL, STRUCTS_XML_FULL) == -1) {
		alogf(LOG_ERR, "%s: %m", "structs_xml_output");
		(void)unlink(tpath);
		goto done;
	}
	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);
	fp = NULL;

#ifdef WIN32
	/* Win32 rename will not overwrite an existing file... */
	if (remove(c->xml_path) != 0) {
		alogf(LOG_ERR, "%s(%s): %m", "remove" _ c->xml_path);
		(void)unlink(tpath);
	}
#else
	/* Atomically overwrite exsting file with new file using rename(2) */
#endif
	if (rename(tpath, c->xml_path) != 0) {
		alogf(LOG_ERR, "%s(%s, %s): %m", "rename" _ 
		      tpath _ c->xml_path);
		(void)unlink(tpath);
		goto done;
	}

	/* Cache file contents */
	app_config_free(c, &c->xml_cache);
	if ((c->xml_cache = app_config_copy(c, c->current)) == NULL)
		alogf(LOG_ERR, "%s: %m", "app_config_copy");

done:
	/* Clean up */
	if (fp != NULL)
		fclose(fp);
	if (tpath != NULL)
		FREE(TYPED_MEM_TEMP, tpath);
	pthread_setcancelstate(old_cstate, NULL);
}

/*
 * Entry point for restarter thread.
 *
 * The mutex will be acquired when we enter and released when we return.
 */
static void
app_config_apply(void *arg)
{
	struct app_config_ctx *const c = arg;

	/* Sanity checks */
	assert((c->flags & CONFIG_APPLYING) == 0);
	assert((c->flags & CONFIG_RESTARTING) == 0);
	assert(c->applying == NULL);

	/* Set flag indicating this thread is applying new configs */
	c->flags |= CONFIG_RESTARTING;

	/* Keep applying configurations until there are no more new ones */
	while ((c->flags & CONFIG_PENDING) != 0) {
		int r;

		/* Make sure latest new configuration is different */
		if (app_config_equal(c, c->pending, c->current)) {
			app_config_free(c, &c->pending);
			c->flags &= ~CONFIG_PENDING;
			goto done;
		}

		/* Grab pending config and apply it */
		c->applying = c->pending;
		c->pending = NULL;
		c->flags &= ~CONFIG_PENDING;
		c->flags |= CONFIG_APPLYING;

		/* Restart subsystems */
		r = pthread_mutex_unlock(&c->mutex);
		assert(r == 0);
		app_config_restart(c);
		r = pthread_mutex_lock(&c->mutex);
		assert(r == 0);
	}

	/* Save new configuration to disk */
	if (c->xml_writeback && c->current != NULL)
		app_config_store(c);

done:
	/* Done */
	c->flags &= ~CONFIG_RESTARTING;
}

/*
 * Restart subsystems to effect the new configuration 'config'.
 *
 * This assumes the mutex is NOT locked.
 */
static void
app_config_restart(struct app_config_ctx *c)
{
	int i;
	int r;

	/* Initialize actions */
	for (i = 0; i < c->num_ss; i++)
		c->ss_flags[i] &= ~(SS_NEED_STOP|SS_NEED_START);

	/* Determine what needs to be shutdown and started up */
	for (i = 0; i < c->num_ss; i++) {
		const struct app_subsystem *const ss = c->info.slist[i];

		/* Do simple analysis */
		if (c->current != NULL
		    && (c->ss_flags[i] & SS_RUNNING) != 0)
			c->ss_flags[i] |= SS_NEED_STOP;
		if (c->applying != NULL
		    && (ss->willrun == NULL
		      || (*ss->willrun)(c, ss, c->applying) != 0))
			c->ss_flags[i] |= SS_NEED_START;

		/* Optimize away unnecessary restarts */
		if ((c->ss_flags[i] & (SS_NEED_STOP|SS_NEED_START))
		    == (SS_NEED_STOP|SS_NEED_START)) {

			/* If either is NULL, the other isn't, so must do it */
			if (c->current == NULL || c->applying == NULL)
				goto no_optimize;

			/* Check dependency list */
			if (ss->deplist != NULL) {
				int j;

				for (j = 0; ss->deplist[j] != NULL; j++) {
					if ((r = structs_equal(c->type,
					    ss->deplist[j], c->current,
					    c->applying)) != 1) {
						if (r != -1)
							goto no_optimize;
						alogf(LOG_ERR, "%s(%s): %m",
						    "structs_equal" _
						    ss->deplist[j]);
					}
				}
			}

			/* Check using subsystem's method */
			if (ss->changed != NULL
			    && (*ss->changed)(c, ss, c->current, c->applying))
				goto no_optimize;

			/* Optimize away restart because it's not needed */
			c->ss_flags[i] &= ~(SS_NEED_STOP|SS_NEED_START);
no_optimize:;
		}
	}

	/* Shut stuff down */
	for (i = c->num_ss - 1; i >= 0; i--) {
		const struct app_subsystem *const ss = c->info.slist[i];

		if ((c->ss_flags[i] & SS_NEED_STOP) != 0) {
			alog(LOG_DEBUG, "stopping subsystem \"%s\"", ss->name);
			if (ss->stop != NULL)
				(*ss->stop)(c, ss, c->current);
			c->ss_flags[i] &= ~SS_RUNNING;
		}
	}

	/* Update current configuration */
	r = pthread_mutex_lock(&c->mutex);
	assert(r == 0);
	assert((c->flags & CONFIG_APPLYING) != 0);
	assert((c->flags & CONFIG_RESTARTING) != 0);
	app_config_free(c, &c->current);
	c->current = c->applying;
	c->applying = NULL;
	c->flags &= ~CONFIG_APPLYING;
	r = pthread_mutex_unlock(&c->mutex);
	assert(r == 0);

	/* If new config is NULL, we've just successfully shut down */
	if (c->current == NULL)
		return;

	/* Start stuff up */
	for (i = 0; i < c->num_ss; i++) {
		const struct app_subsystem *const ss = c->info.slist[i];

		if ((c->ss_flags[i] & SS_NEED_START) != 0) {
			alog(LOG_DEBUG, "starting subsystem \"%s\"", ss->name);
			if (ss->start != NULL
			    && (*ss->start)(c, ss, c->current) == -1) {
				alog(LOG_ERR, "subsystem \"%s\" startup failed",
				    ss->name);
				continue;
			}
			c->ss_flags[i] |= SS_RUNNING;
		}
	}
}

/*
 * Logger for XML parsing
 */
static void
app_config_xml_logger(int sev, const char *fmt, ...)
{
	char buf[512];
	va_list args;

	snprintf(buf, sizeof(buf), "XML error: ");
	va_start(args, fmt);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt, args);
	va_end(args);
	alog(sev, "%s", buf);
}

