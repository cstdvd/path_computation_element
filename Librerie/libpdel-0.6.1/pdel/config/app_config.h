
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_CONFIG_APP_CONFIG_H_
#define _PDEL_CONFIG_APP_CONFIG_H_

/************************************************************************
				DEFINITIONS
************************************************************************/

struct app_subsystem;
struct app_config_ctx;
struct pevent_ctx;

/* XML document tag for app_config configurations */
#define APP_CONFIG_XML_TAG	"config"

/* Methods exported by a subsystem */
typedef int	app_ss_startup_t(struct app_config_ctx *ctx,
			const struct app_subsystem *ss, const void *config);
typedef void	app_ss_shutdown_t(struct app_config_ctx *ctx,
			const struct app_subsystem *ss, const void *config);
typedef int	app_ss_willrun_t(struct app_config_ctx *ctx,
			const struct app_subsystem *ss, const void *config);
typedef int	app_ss_changed_t(struct app_config_ctx *ctx,
			const struct app_subsystem *ss, const void *config1,
			const void *config2);

/* Descriptor for a subsystem */
struct app_subsystem {
	const char		*name;		/* name, null to end list */
	void			*arg;		/* opaque subsystem argument */
	app_ss_startup_t	*start;		/* start subsystem */
	app_ss_shutdown_t	*stop;		/* stop subsystem */
	app_ss_willrun_t	*willrun;	/* will subsystem run? */
	app_ss_changed_t	*changed;	/* subsystem config changed? */
	const char		**deplist;	/* config items dependent on */
};

/*
 * Methods exported by an application:
 *
 * init		Initialize a config structure. The structure will already
 *		be initialized with the values provided by the structs type.
 *		This method is so any other default values can be applied.
 *
 * getnew	This is used when no existing configuration is found for
 *		the system (i.e., after a factory reset). The argument
 *		provided is a configuration structure already initialized
 *		using the "init" method. The "getnew" method should apply
 *		any further initialization needed for this specific system.
 *
 * checker	This method is the gatekeeper for applying new configurations.
 *		If this method returns zero, the configuration is rejected.
 *		An appropriate error message may be put in "errbuf". Make
 *		sure your application does not automatically generate any
 *		configurations that fail to pass this method, otherwise
 *		you get into a stuck state.
 *
 * normalize	Configurations may contain redundant and/or derived info.
 *		This method (if defined) can be used to normalize a config
 *		struture. This method is called before "checker".
 *
 * upgrade	When the configuration structure changes, and you still
 *		want to be able to read in XML created from the older
 *		versions of the configuration structure, defining this
 *		method gives you a way to specify how to upgrade information
 *		the old format to the new one.
 */
typedef int	app_config_init_t(struct app_config_ctx *ctx, void *config);
typedef int	app_config_getnew_t(struct app_config_ctx *ctx, void *config);
typedef int	app_config_checker_t(struct app_config_ctx *ctx,
			const void *config, char *errbuf, size_t ebufsize);
typedef void	app_config_normalize_t(struct app_config_ctx *ctx,
			void *config);
typedef int	app_config_upgrade_t(struct app_config_ctx *ctx,
			const void *old_conf, u_int old_version,
			void *new_conf);

/*
 * Descriptor for an application's configuration information.
 *
 * Automatic configuration upgrade feature:
 *
 * types[version] is a structs type for the applications configuration
 * information structure. All previous pointers in the types[] array point
 * to types for older versions of the configuration structure. When an
 * older version is encountered in an XML file, it is read in using the
 * corresponding old structs type and converted to the current version
 * using the "upgrade" method. So versions are numbered 0, 1, .... Each
 * time the configuration structure is changed, "version" should be
 * bumped and the new type added to the "types" array. Version numbers
 * are not allowed to go backwards.
 */
struct app_config {
	u_int					version;/* current config version # */
	const struct structs_type	**types;/* types for all versions */
	const struct app_subsystem	**slist;/* list of subsystems */
	app_config_init_t		*init;	/* initialize config defaults */
	app_config_getnew_t		*getnew;/* generate new config */
	app_config_checker_t		*checker;/* validate a config struct */
	app_config_normalize_t		*normalize; /* normalize a config */
	app_config_upgrade_t		*upgrade;/* upgrade a config */
};

/************************************************************************
			    BUILT-IN SUBSYSTEMS
************************************************************************/

__BEGIN_DECLS

/*
 * PID file subsystem template.
 *
 * Copy this structure and set arg to be a const char * pointing to the
 * name of the item in the config structure containing the pathname to
 * the PID file. If this pathname is NULL/empty, no PID file is created.
 */
PD_IMPORT const struct app_subsystem	app_config_pidfile_subsystem;

/*
 * Directory subsystem template.
 *
 * Copy this structure and set arg to be a const char * pointing to the
 * name of the item in the config structure containing the directory to
 * chdir(2) into. If this pathname is NULL/empty, no change is made.
 */
PD_IMPORT const struct app_subsystem	app_config_directory_subsystem;

/*
 * "Curconf" subsystem template.
 *
 * Copy this structure and set arg to be a 'struct my_config **' pointer
 * pointing to the application's 'curconf' variable, which at all times
 * points to a (read-only) copy of the currently active configuration object.
 * This subsystem should usually be first in the subsystem list.
 */
PD_IMPORT const struct app_subsystem	app_config_curconf_subsystem;

__END_DECLS

/*
 * Alog logging subsystem template.
 *
 * Copy this structure and set arg to be a pointer to an instance
 * of the structure below.
 */
struct app_config_alog_info {
	const char	*name;		/* name of alog_channel_config_type */
	int		channel;	/* alog channel */
};

__BEGIN_DECLS
PD_IMPORT const struct app_subsystem	app_config_alog_subsystem;
__END_DECLS

/************************************************************************
				FUNCTIONS
************************************************************************/

__BEGIN_DECLS

/*
 * Initialize the application's configuration type and list of subsystems.
 * Also sets the XML tag for reading and writing the configuration and
 * the user application cookie.
 *
 * This must be called first.
 */
extern struct	app_config_ctx *app_config_init(struct pevent_ctx *ctx,
			const struct app_config *info, void *cookie);

/*
 * Reverse the effects of app_config_init().
 *
 * All subsystems must be shutdown and no new configurations
 * may be pending.
 */
extern int	app_config_uninit(struct app_config_ctx **ctxp);

/*
 * Get application cookie.
 */
extern void	*app_config_get_cookie(struct app_config_ctx *ctx);

/*
 * Initialize application configuration from an XML file.
 *
 * This reads in the configuration and then calls app_config_set()
 * with a delay of one millisecond.
 *
 * If "writeback" is zero, no writebacks will ever occur.
 */
extern int	app_config_load(struct app_config_ctx *ctx,
			const char *path, int allow_writeback);

/*
 * Re-read the XML file passed to app_config_load() and reconfigure
 * as appropriate.
 *
 * This reads in the configuration and then calls app_config_set()
 * with a delay of one millisecond.
 */
extern int	app_config_reload(struct app_config_ctx *c);

/*
 * Function to create a configuration structure. This structure will
 * have the application "default" values.
 */
extern void	*app_config_new(struct app_config_ctx *c);

/*
 * Change the application's current configuration to be a copy of "config"
 * after a delay of at most "delay" milliseconds.
 *
 * A NULL configuration shuts everything down. Any configs passed to
 * app_config_set() subsequent to passing a NULL config, but before
 * the shutdown operation has completed, are ignored. This guarantees
 * that a shutdown really does shut everything down.
 *
 * If an app_config_load() was previously called with "allow_writeback" true,
 * then a non-NULL configuration will be written back out to the XML file.
 *
 * If the configuration is invalid, -1 is returned with errno == EINVAL
 * and the buffer (if not NULL) is filled in with the reason.
 */
extern int	app_config_set(struct app_config_ctx *ctx,
			const void *config, u_long delay, char *ebuf, int emax);

/*
 * Get a copy of the application's current or pending configuration.
 */
extern void	*app_config_get(struct app_config_ctx *ctx, int pending);

/*
 * Get the configuration object type.
 */
extern const	struct structs_type *app_config_get_type(
			struct app_config_ctx *ctx);

/*
 * Get a copy of a configuration.
 */
extern void	*app_config_copy(struct app_config_ctx *ctx,
			const void *config);

/*
 * Free a configuration.
 */
extern void	app_config_free(struct app_config_ctx *ctx, void **configp);

__END_DECLS

#endif	/* _PDEL_CONFIG_APP_CONFIG_H_ */

