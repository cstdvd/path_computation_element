
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "sls_global.h"
#include "sls_config.h"

/*
 * Internal functions
 */
static ppp_log_vput_t		sls_log_vput;
static void			usage(void) __dead2;

/*
 * Global variables
 */
pid_t			pid;
int			debug_level = 0;
struct pevent_ctx	*sls_event_ctx;
struct ppp_engine	*engine;

int
main(int argc, char **argv)
{
	const char *config_file = PREFIX "/etc/sl2tps/" CONFIG_FILE;
	struct ppp_log *log;
	int no_fork = 0;
	sigset_t sigs;
	void *config;
	int sig = 0;
	int ch;

	/* Parse command line */
	while ((ch = getopt(argc, argv, "Df:")) != -1) {
		switch (ch) {
		case 'D':
			no_fork = 1;
			debug_level++;
			break;
		case 'f':
			config_file = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	switch (argc) {
	default:
		usage();
		break;
	case 0:
		break;
	}

	/* Enable typed memory */
	if (debug_level > 0 && typed_mem_enable() == -1)
		err(1, "typed_mem_enable");

	/* Seed random number generator */
	srandomdev();

	/* Block SIGPIPE */
	(void)signal(SIGPIPE, SIG_IGN);

	/* Enable debug logging and malloc() if desired */
	if (debug_level > 0) {
		alog_set_debug(0, 1);
		setenv("MALLOC_OPTIONS", "AJ", 1);
		//NgSetDebug(debug_level);
	}

	/* Fork into the background */
	if (!no_fork && daemon(1, debug_level > 0) == -1)
		err(1, "daemon");
	pid = getpid();

	/* Get a new event context */
	if ((sls_event_ctx = pevent_ctx_create("sls_event_ctx", NULL)) == NULL)
		err(1, "pevent_ctx_create");

	/* Load in configuration */
	if (sls_config_init(sls_event_ctx, config_file) == -1)
		err(1, "failed to get configuration");

	/* Create PPP log */
	if ((log = ppp_log_create(NULL, sls_log_vput, NULL)) == NULL) {
		alog(LOG_ERR, "%s: %m", "ppp_log_create");
		goto shutdown;
	}

	/* Create new PPP engine */
	if ((engine = ppp_engine_create(&sls_manager, NULL, log)) == NULL) {
		alog(LOG_ERR, "%s: %m", "ppp_enging_create");
		ppp_log_close(&log);
		goto shutdown;
	}

loop:
	/* Wait for signal */
	alog(LOG_INFO, "waiting for connections...");
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGHUP);
	sigaddset(&sigs, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &sigs, NULL) == -1) {
		warn("sigprocmask");
		goto shutdown;
	}
	if (sigwait(&sigs, &sig) == -1) {
		warn("sigwait");
		goto shutdown;
	}

	/* If SIGHUP, just reload config */
	if (sig == SIGHUP) {
		alog(LOG_NOTICE, "rec'd signal %s, %s",
		    sys_signame[sig], "reloading configuration");
		app_config_reload(sls_config_ctx);
		goto loop;
	}
	alog(LOG_NOTICE, "rec'd signal %s, %s", sys_signame[sig],
	    sig == SIGUSR1 ? "restarting" : "shutting down");

shutdown:
	/* Shut down */
	if (app_config_set(sls_config_ctx, NULL, 0, NULL, 0) == -1)
		err(1, "app_config_set");
	while ((config = app_config_get(sls_config_ctx, 0)) != NULL) {
		app_config_free(sls_config_ctx, &config);
		usleep(10000);
	}

	/* If restarting, start back up */
	if (sig == SIGUSR1) {
		app_config_reload(sls_config_ctx);
		goto loop;
	}

	/* Shut down app_config stuff */
	app_config_uninit(&sls_config_ctx);

	/* Destroy PPP engine */
	ppp_engine_destroy(&engine, 1);

	/* Destroy event context */
	pevent_ctx_destroy(&sls_event_ctx);
	usleep(10000);

	/* Show memory leaks */
	if (debug_level > 0) {
		printf("\nUnfreed memory:\n\n");
		typed_mem_dump(stdout);
	}

	/* Done */
	return 0;
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: sl2tps [-D] [-f %s]\n", CONFIG_FILE);
	exit(1);
}

static void
sls_log_vput(void *arg, int sev, const char *fmt, va_list args)
{
	valog(sev, fmt, args);
}


