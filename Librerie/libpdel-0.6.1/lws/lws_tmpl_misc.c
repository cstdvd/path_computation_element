
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#ifdef __CYGWIN__
#include <crypt.h>
#endif

#include "lws_global.h"
#include "lws_config.h"
#include "lws_tmpl.h"
#include "lws_tmpl_misc.h"

#ifndef WIN32
#define HAVE_MKSTEMP
#else

#include <openssl/md4.h>

#endif

/***********************************************************************
			MISC TMPL FUNCTIONS
***********************************************************************/

/* Our user-defined template functions */
static tmpl_handler_t	lws_tf_debug_level;
static tmpl_handler_t	lws_tf_hostname;
static tmpl_handler_t	lws_tf_time;
static tmpl_handler_t	lws_tf_readfile;
static tmpl_handler_t	lws_tf_version;
static tmpl_handler_t	lws_tf_signal;
static tmpl_handler_t	lws_tf_crypt_hash;
static tmpl_handler_t	lws_tf_system;

/* User-defined template function descriptor list */
const struct lws_tmpl_func lws_tmpl_misc_functions[] = {
    LWS_TMPL_FUNC(debug_level, 0, 0, "",
	"Returns the number of \"-D\" flags specified on the command line."),
    LWS_TMPL_FUNC(hostname, 0, 0, "",
	"Returns the UNIX hostname."),
    LWS_TMPL_FUNC(time, 0, 0, "",
	"Returns the system time in seconds since 1/1/1970 0:00 GMT."),
    LWS_TMPL_FUNC(readfile, 1, 1, "relpath",
	"Returns the contents of the file found at pathname $1, taken relative"
"\n"	"to the document root for this server. <b>Warning</b>: it's possible"
"\n"	"to read files outside of the server's docroot hierarchy."),
    LWS_TMPL_FUNC(version, 0, 0, "",
	"Return the version number of this LWS software."),
    LWS_TMPL_FUNC(signal, 2, 2, "signal:delay",
	"Sends the web server signal $1 after a delay of $2 seconds."),
    LWS_TMPL_FUNC(crypt_hash, 1, 2, "secret:salt",
	"Works like the UNIX <code>crypt(3)</code> function. If $2 is omitted,"
"\n"	"a new, random salt is created and used."),
    LWS_TMPL_FUNC(system, 1, 2, "command:input",
	"Works like the UNIX <code>system(3)</code> function. The $1 is"
"\n"	"executed by <code>/bin/sh</code> in a sub-shell. If $2 is given,"
"\n"	"it is provided as the new process's standard input. The command's"
"\n"	"standard output is returned. Note: if $2 is given, it will be"
"\n"	"written in its entirety before the output of $1 is read. This"
"\n"	"may lead to deadlock if $1 tries to send a lot of output before"
"\n"	"reading any input."),
    { { NULL } }
};

#ifdef WIN32
/*
 * NT HASH = md4(str2unicode(pw))
 */

#define MD4_SIZE 16
#define crypt(pw, s) crypt_nthash((pw), (s))
/* ARGSUSED */
static char *
crypt_nthash(const char *pw, const char *salt __unused)
{
        size_t unipwLen;
        int i, j;
        static char hexconvtab[] = "0123456789abcdef";
        static const char *magic = "$3$";
        static char passwd[120];
        u_int16_t unipw[128];
        char final[MD4_SIZE*2 + 1];
        u_char hash[MD4_SIZE];
        const char *s;
        MD4_CTX ctx;
  
        memset(unipw, 0, sizeof(unipw)); 
        /* convert to unicode (thanx Archie) */
        unipwLen = 0;
        for (s = pw; unipwLen < sizeof(unipw) / 2 && *s; s++)
                unipw[unipwLen++] = htons(*s << 8);
        
        /* Compute MD4 of Unicode password */
        MD4_Init(&ctx);
        MD4_Update(&ctx, (u_char *)unipw, unipwLen*sizeof(u_int16_t));
        MD4_Final(hash, &ctx);  
        
        for (i = j = 0; i < MD4_SIZE; i++) {
                final[j++] = hexconvtab[hash[i] >> 4];
                final[j++] = hexconvtab[hash[i] & 15];
        }
        final[j] = '\0';

        strcpy(passwd, magic);
        strcat(passwd, "$");
        strncat(passwd, final, MD4_SIZE*2);

        /* Don't leave anything around in vm they could use. */
        memset(final, 0, sizeof(final));

        return (passwd);
}
#endif

static char *
lws_tf_readfile(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const struct lws_tmpl_info *const tfi = targ->arg;
	const struct lws_file_info *const finfo = tfi->fileinfo;
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char *rtn = NULL;
	char buf[1024];
	int esave;
	FILE *fp;
	FILE *sb;
	size_t r;

	/* Generate pathname starting at docroot */
	if (*av[1] != '/') {
		strlcpy(buf, finfo->docroot != NULL ?
		    finfo->docroot : ".", sizeof(buf));
		strlcat(buf, "/", sizeof(buf));
		strlcat(buf, av[1], sizeof(buf));
	} else
		strlcpy(buf, av[1], sizeof(buf));

	/* Read file into a string buffer */
	if ((fp = fopen(buf, "r")) == NULL)
		return (NULL);
	if ((sb = string_buf_output(mtype)) == NULL) {
		fclose(fp);
		return (NULL);
	}
	while ((r = fread(buf, 1, sizeof(buf), fp)) != 0) {
		if (fwrite(buf, 1, r, sb) != r)
			goto fail;
	}
	if (ferror(fp))
		goto fail;

	/* Done */
	rtn = string_buf_content(sb, 1);

	/* Clean up */
fail:	esave = errno;
	fclose(fp);
	fclose(sb);
	errno = esave;
	return (rtn);
}

static char *
lws_tf_debug_level(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[16];

	snprintf(buf, sizeof(buf), "%d", debug_level);
	return (STRDUP(mtype, buf));
}

static char *
lws_tf_hostname(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char hostname[MAXHOSTNAMELEN];

	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';
	return (STRDUP(mtype, hostname));
}

static char *
lws_tf_time(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[32];

	snprintf(buf, sizeof(buf), "%lu", (u_long)time(NULL));
	return (STRDUP(mtype, buf));
}

static char *
lws_tf_version(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype, LWS_SERVER_VERSION));
}

static void	lws_signal(void *arg);
static struct	pevent *lws_signal_event;

/* Linux doesn't have sys_sysname[] */
struct signame {
	int		sig;
	const char	*name;
};
#define SIGNAME(x)	{ SIG ## x, #x }
static const struct signame sigs[] = {
	SIGNAME(INT),
	SIGNAME(TERM),
#ifndef WIN32
	SIGNAME(HUP),
	SIGNAME(USR1),
#endif
	{ 0, NULL }
};

static char *
lws_tf_signal(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const int delay = atoi(av[2]) * 1000;
	int sig;

	if ((sig = atoi(av[1])) == 0) {
		int i;

		for (i = 0; sigs[i].name != NULL
		    && strcasecmp(av[1], sigs[i].name) != 0; i++);
		if (sigs[i].name == NULL) {
			errno = EINVAL;
			return (NULL);
		}
		sig = sigs[i].sig;
	}
	if (pevent_register(lws_event_ctx, &lws_signal_event, 0,
	      NULL, lws_signal, (void *)sig, PEVENT_TIME, delay) == -1
	    && errno != EBUSY)
		return (NULL);
	return (STRDUP(mtype, ""));
}

static void
lws_signal(void *arg)
{
#ifdef WIN32
	raise((int)arg);
#else
	kill(pid, (int)arg);
#endif
}

static char *
lws_tf_crypt_hash(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	static pthread_mutex_t crypt_mutex = PTHREAD_MUTEX_INITIALIZER;
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char newsalt[3 + 8 + 1];
	const char *cryptpw;
	const char *salt;
	char *rtn;

	/* Use supplied salt or create new salt */
	if (ac == 2) {
		u_char rdata[8 + 1];
		int fd;
		int i;

		if ((fd = open("/dev/urandom", O_RDONLY)) == -1) {
			ASPRINTF(mtype, errmsgp, "%s: %s",
			    "/dev/random" _ strerror(errno));
			return (NULL);
		}
		if (read(fd, rdata, sizeof(rdata) - 1) != sizeof(rdata) - 1) {
			close(fd);
			return (NULL);
		}
		close(fd);
		for (i = 0; i < sizeof(rdata) - 1; i++)
			rdata[i] = b64_rfc2045_charset[rdata[i] & 0x3f];
		rdata[i] = '\0';
		snprintf(newsalt, sizeof(newsalt), "$1$%s", rdata);
		salt = newsalt;
	} else
		salt = av[2];

	/* Only one crypt() at a time due to crypt()'s static buffer */
	if ((errno = pthread_mutex_lock(&crypt_mutex)) != 0)
		return (NULL);
	cryptpw = crypt(av[1], salt);
	rtn = (cryptpw != NULL) ? STRDUP(mtype, cryptpw) : NULL;
	pthread_mutex_unlock(&crypt_mutex);

	/* Done */
	return (rtn);
}

#define SYSTEM_MEM_TYPE			"lws_tf_system"

struct system_info {
	FILE	*input;			/* temporary input file stream */
	FILE	*output;		/* saved output stream */
	FILE	*cmdstream;		/* command stream */
	char	infile[56];		/* temporary input file */
	char	*cmdstring;		/* command string */
};

static void	lws_tf_system_cleanup(void *arg);

static char *
lws_tf_system(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	struct system_info info;
	u_char buf[128];
	char *s = NULL;
	size_t r;

	/* Initialize context and push cleanup hook */
	memset(&info, 0, sizeof(info));
	pthread_cleanup_push(lws_tf_system_cleanup, &info);

	/* Write optional input into temporary file and create command string */
	if (ac > 2) {
		const size_t slen = strlen(av[2]);
#ifndef HAVE_MKSTEMP
		const char *tdir;
		tdir = getenv("TEMP");
		snprintf(info.infile, sizeof(info.infile), 
			 "%s\\lws.XXXXXXXXX", tdir);
		if ((mktemp(info.infile)) == info.infile) {
			*info.infile = '\0';
			goto fail;
		}
		if ((info.input = fopen(info.infile, "w")) == NULL) {
			*info.infile = '\0';
			goto fail;
		}
		chmod(info.infile, 0600);
#else
		int fd;

		strlcpy(info.infile, "/tmp/lws.XXXXXXXX", sizeof(info.infile));
		if ((fd = mkstemp(info.infile)) == -1) {
			*info.infile = '\0';
			goto fail;
		}
		if ((info.input = fdopen(fd, "w")) == NULL) {
			close(fd);
			goto fail;
		}
#endif
		if (fwrite(av[2], 1, slen, info.input) != slen)
			goto fail;
		fclose(info.input);
		info.input = NULL;
	}

	/* Create command string */
	ASPRINTF(SYSTEM_MEM_TYPE, &info.cmdstring, "(%s) < %s",
	    av[1] _ *info.infile != '\0' ? info.infile : "/dev/null");
	if (info.cmdstring == NULL)
		return (NULL);

	/* Create stream for saving command output */
	if ((info.output = string_buf_output(mtype)) == NULL)
		goto fail;

	/* Create command stream */
	if ((info.cmdstream = popen(av[1], "r")) == NULL)
		return (NULL);

	/* Read output */
	while ((r = fread(buf, 1, sizeof(buf), info.cmdstream)) != 0) {
		if (fwrite(buf, 1, r, info.output) != r)
			goto fail;
	}
	if (ferror(info.cmdstream))
		goto fail;

	/* Get result */
	s = string_buf_content(info.output, 1);

	/* Clean up and return */
 fail:
	{
		/* Shut up busted GCC warning on linux about label at the
		   end of a compound statement */
		int dummy;
		dummy = r;
	}
	pthread_cleanup_pop(1);
	return (s);
}

static void
lws_tf_system_cleanup(void *arg)
{
	struct system_info *const info = arg;
	int esave;

	esave = errno;
	if (info->cmdstream != NULL)
		pclose(info->cmdstream);
	if (info->input != NULL)
		fclose(info->input);
	if (*info->infile != '\0')
		unlink(info->infile);
	if (info->output != NULL)
		fclose(info->output);
	if (info->cmdstring != NULL)
		FREE(SYSTEM_MEM_TYPE, info->cmdstring);
	errno = esave;
}

