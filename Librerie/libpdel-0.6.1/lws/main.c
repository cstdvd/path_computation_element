
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_config.h"

/*
 * Internal functions
 */

/* SSL typed_mem(3) wrappers */
static void	*ssl_malloc(size_t size);
static void	*ssl_realloc(void *mem, size_t size);
static void	ssl_free(void *mem);

static void	usage(void) __dead2;

#ifndef lws_signame
#define lws_signame(x) sys_signame[x]
#endif

#ifndef LWS_DIR
#ifndef WIN32
#define LWS_DIR "/etc/lws"
#else
#define LWS_DIR "C:\\WINDOWS\\LWS"
#endif
#endif

#ifndef PREFIX
#ifndef WIN32
#define PREFIX "/usr/local"
#else
#define PREFIX "C:"
#endif
#endif

/*
 * Local flags
 */
int	log_min_severity = 0;
int	log_max_entries = 10000;
time_t	log_max_age = 0;
int	do_log_dump = 0;

static const struct app_config_alog_info error_info = {
	"error_log", 0
};

static struct alog_config lws_log_config = {
  "lws.logfile",
  "",
  0,
#ifdef WIN32
  { { { 0, 0, 0, 0 } } },
#else
  { 0 },
#endif
  0,
  5000 
};

#ifdef WIN32
#define err(x, fmt, args) do { fprintf(stderr, fmt, args); exit(x); } while (0)
#endif

/*
 * Local funcs.
 */
static int
lws_dump_logfile(int min_severity, int max_entries, time_t max_age, 
		 const char *match)
{ 
	struct alog_history	lhx;
	regex_t			*preg = NULL;
	u_int			x;
	time_t			now;

	/* Start up alog channel */
	if (alog_configure(error_info.channel, &lws_log_config) == -1) {
		fprintf(stderr, "error configuring logging: %d-%s", 
			errno, strerror(errno));
		return (-1);
	}
	if (alog_get_history(0, min_severity, max_entries, max_age,
			     preg, &lhx) != 0) {
	  fprintf(stderr, "Unable to get log history");
		return(-1);
	}
	time(&now);
	printf("lws - Printing %d log entries:\n", lhx.length);
	for (x = 0; x < lhx.length; x++) {
		const struct alog_entry	*le = lhx.elems[x];
		
		printf("-%ld\t%d\t%s\n", (long) now - le->when, le->sev, le->msg);
	}
	return(0);
}

/*
 * Global variables
 */
pid_t		pid;
int		debug_level = 0;
struct		pevent_ctx *lws_event_ctx;

int
main(int argc, char **argv)
{
	const char *dir = PREFIX LWS_DIR;
	int no_fork = 0;
	void *config;
#ifndef WIN32
	sigset_t sigs;
	int sig;
#endif
	int ch;

	/* Parse command line */
	while ((ch = pd_getopt(argv[0], argc, argv, "Dd:E")) != -1) {
		switch (ch) {
		case 'D':
			no_fork = 1;
			debug_level++;
			break;
		case 'd':
			dir = pd_optarg;
			break;
		case 'E':
			do_log_dump = 1;
			no_fork = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= pd_optind;
	argv += pd_optind;
	switch (argc) {
	default:
		usage();
		break;
	case 0:
		break;
	}

	pd_init(0);

	/* Change directory */
	if (chdir(dir) == -1)
		err(1, "%s", dir);

	/* Dump logfile ? */
	if (do_log_dump) {
		lws_dump_logfile(log_min_severity, log_max_entries, 
				 log_max_age, NULL);
		exit(0);
	}

	/* Make OpenSSL use our malloc/free wrappers */
	CRYPTO_set_mem_functions(ssl_malloc, ssl_realloc, ssl_free);
	CRYPTO_set_locked_mem_functions(ssl_malloc, ssl_free);

	/* Enable typed memory */
	if (debug_level > 0 && typed_mem_enable() == -1)
		err(1, "typed_mem_enable%s", "");

#if !defined(__linux__) && !defined(__CYGWIN__) && !defined(WIN32)
	/* Seed random number generator */
	srandomdev();
#endif

#ifdef SIGPIPE
	/* Block SIGPIPE */
	(void)signal(SIGPIPE, SIG_IGN);
#endif

	/* Enable debug logging and malloc() if desired */
	if (debug_level > 0) {
		alog_set_debug(0, 1);
#ifndef WIN32
		setenv("MALLOC_OPTIONS", "AJ", 1);
#endif
	}

	/* Get a new event context */
	if ((lws_event_ctx = pevent_ctx_create("pevent_ctx", NULL)) == NULL)
		err(1, "pevent_ctx_create%s", "");

	/* Fork into the background, but stay in same directory */
#ifndef WIN32
	if (!no_fork && daemon(1, debug_level > 0) == -1)
		err(1, "daemon%s", "");
#endif
	pid = pd_getpid();

	/* Load in configuration */
	if (lws_config_init(lws_event_ctx, CONFIG_FILE) == -1)
		err(1, "failed to get configuration%s", "");

loop:
	/* Wait for interrupt */
#ifndef WIN32
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
#ifdef SIGHUP
	sigaddset(&sigs, SIGHUP);
#endif
#ifdef SIGUSR1
	sigaddset(&sigs, SIGUSR1);
#endif
	if (sigprocmask(SIG_BLOCK, &sigs, NULL) == -1)
		err(1, "sigprocmask%s", "");
	if (sigwait(&sigs, &sig) == -1)
		err(1, "sigwait%s", "");
#else
	Sleep(INFINITE);
#endif

#ifdef SIGHUP
	/* If SIGHUP, just reload config */
	if (sig == SIGHUP) {
		alog(LOG_NOTICE, "rec'd signal %s, %s",
		     lws_signame(sig), "reloading configuration");
		app_config_reload(lws_config_ctx);
		goto loop;
	}
	alog(LOG_NOTICE, "rec'd signal %s, %s", lws_signame(sig),
	    sig == SIGUSR1 ? "restarting" : "shutting down");
#endif
	/* Shut down */
	if (app_config_set(lws_config_ctx, NULL, 0, NULL, 0) == -1)
		err(1, "app_config_set%s", "");
	while ((config = app_config_get(lws_config_ctx, 0)) != NULL) {
		app_config_free(lws_config_ctx, &config);
		usleep(10000);
	}

#ifdef SIGHUP
	/* If restarting, start back up */
	if (sig == SIGUSR1) {
		app_config_reload(lws_config_ctx);
		goto loop;
	}
#endif
	/* Shut down app_config stuff */
	app_config_uninit(&lws_config_ctx);

	/* Destroy event context */
	pevent_ctx_destroy(&lws_event_ctx);
	usleep(10000);

	/* Show memory usage */
	if (debug_level > 0) {
		printf("\nUnfreed memory:\n\n");
		typed_mem_dump(stdout);
	}

	/* Done */
	return (0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: lws [-D] [-d directory] -E\n");
	exit(1);
}

/***********************************************************************
			SSL MEMORY WRAPPERS
***********************************************************************/

static void
*ssl_malloc(size_t size)
{
	return (MALLOC("OpenSSL", size));
}

static void
*ssl_realloc(void *mem, size_t size)
{
	return (REALLOC("OpenSSL", mem, size));
}

static void
ssl_free(void *mem)
{
	FREE("OpenSSL", mem);
	return;
}

