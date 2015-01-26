
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/stat.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <netdb.h>
#include <err.h>
#include <regex.h>
#include <pthread.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>

#include <pdel/structs/structs.h>
#include <pdel/structs/type/array.h>

#include <pdel/tmpl/tmpl.h>
#include <pdel/util/typed_mem.h>
#include <pdel/util/pevent.h>

#include <pdel/http/http_defs.h>
#include <pdel/http/http_server.h>
#include <pdel/http/http_servlet.h>
#include <pdel/http/servlet/tmpl.h>
#include <pdel/http/servlet/file.h>
#include <pdel/http/servlet/basicauth.h>
#include <pdel/http/servlet/redirect.h>
#include <pdel/http/servlet/cookieauth.h>

#define DEMO_MEM_TYPE		"demo"
#define DEMO_COOKIE_NAME	"demo_cookie"
#define TMPL_MEM_TYPE		"tmpl"
#define NUM_IN_CACHE		2
#define CACHE_TIMEOUT_SECS 	10
#define NUM_CACHE_LOOP		10
#define PATH_KERNEL		"/kernel"

#ifndef MIN
#define MIN(a, b)	((a) > (b) ? (a) : (b))
#endif

/*
 * Servlets
 */
static struct	http_servlet *demo_servlet_create(void);
static struct	http_servlet *cgi_servlet_create(void);
static struct	http_servlet *getkernel_servlet_create(void);

/*
 * Internal functions
 */

static http_logger_t		demo_logger;

static tmpl_handler_t		demo_tmpl_handler;
static tmpl_errfmtr_t		demo_tmpl_errfmtr;

static http_servlet_basicauth_t	demo_basicauth;

static http_servlet_cookieauth_reqd_t	demo_auth_reqd;

static void	demo_file_upload(struct http_request *req,
			struct http_response *resp, FILE *op);
static int	demo_client(struct pevent_ctx *ctx, int nurls,
			char **urls, int count);

/* SSL typed_mem(3) wrappers */
static void	*ssl_malloc(size_t size);
static void	*ssl_realloc(void *mem, size_t size);
static void	ssl_free(void *mem);

static void	usage(void);

/*
 * Internal variables
 */

/* SSL info */
static const struct http_server_ssl ssl_info = {
	"demo.crt",			/* SSL x509 certificate file */
	"demo.key",			/* SSL RSA private key file */
	NULL				/* no password for private key needed */
};

static const	char *redirect_url;
static const	struct in_addr zero_ip;
static pid_t	main_pid;

static const	u_char demo_id[] = { 'd', 'e', 'm', 'o' };

static const	char *vhost = NULL;
static const	struct http_server_ssl *ssl = NULL;
static int	port = 0;

static struct	http_servlet *cookieauth_servlet;

/* Info for "demo.tmpl" template servlet */
static struct http_servlet_tmpl_info tmpl_servlet_info = {
	"demo.tmpl",			/* pathname of template file */
	NULL,				/* guess the mime type for me */
	NULL,				/* guess the mime encoding for me */
	demo_logger,			/* error logging routine */
	{				/* info required by tmpl library */
		TMPL_SKIP_NL_WHITE,		/* flags for tmpl_execute() */
		TMPL_MEM_TYPE,			/* tmpl string typed mem type */
		demo_tmpl_handler,		/* handler for custom @funcs */
		demo_tmpl_errfmtr,		/* handler for errors */
		NULL,				/* opaque user cookie */
	}
};

/* Info for "logon.tmpl" template servlet */
static struct http_servlet_tmpl_info logon_servlet_info = {
	"logon.tmpl",			/* pathname of template file */
	NULL,				/* guess the mime type for me */
	NULL,				/* guess the mime encoding for me */
	demo_logger,			/* error logging routine */
	{				/* info required by tmpl library */
		TMPL_SKIP_NL_WHITE,		/* flags for tmpl_execute() */
		TMPL_MEM_TYPE,			/* tmpl string typed mem type */
		demo_tmpl_handler,		/* handler for custom @funcs */
		demo_tmpl_errfmtr,		/* handler for errors */
		NULL,				/* opaque user cookie */
	}
};

/* Info for "vhost.tmpl" template servlet */
static struct http_servlet_tmpl_info vhost_servlet_info = {
	"vhost.tmpl",			/* pathname of template file */
	NULL,				/* guess the mime type for me */
	NULL,				/* guess the mime encoding for me */
	demo_logger,			/* error logging routine */
	{				/* info required by tmpl library */
		TMPL_SKIP_NL_WHITE,		/* flags for tmpl_execute() */
		TMPL_MEM_TYPE,			/* tmpl string typed mem type */
		demo_tmpl_handler,		/* handler for custom @funcs */
		demo_tmpl_errfmtr,		/* handler for errors */
		NULL,				/* opaque user cookie */
	}
};

/* Info for BIND docs file servlet */
static struct http_servlet_file_info bind_servlet_info = {
	"/usr/share/doc/bind/html",	/* document root for this servlet */
	0,				/* don't allow escape from docroot */
	NULL,				/* derive filename from URL */
	"/file",			/* URL prefix to strip from pathname */
	NULL,				/* guess the mime type for me */
	NULL,				/* guess the mime encoding for me */
	demo_logger,			/* error logging routine */
	NULL,				/* don't hide any files */
	{ }				/* don't support templates */
};

/*
 * Start a HTTP server.
 */
int
main(int argc, char **argv)
{
	struct pevent_ctx *ctx;
	char hostname[255] = { '\0' };
	struct http_servlet *servlet;
	struct http_server *server;
	int do_client = 0;
	sigset_t sigs;
	int count = 1;
	int sig;
	int ret;
	int ch;

	/* Parse command line arguments */
	while ((ch = getopt(argc, argv, "sp:c:r:h:v:")) != -1) {
		switch (ch) {
		case 'c':
			count = atoi(optarg);
			break;
		case 'h':
			snprintf(hostname, sizeof(hostname), "%s", optarg);
			break;
		case 's':
			ssl = &ssl_info;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'r':
			redirect_url = optarg;
			break;
		case 'v':
			vhost = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	switch (argc) {
	default:
		do_client = 1;
		break;
	case 0:
		break;
	}
	if (port == 0)
		port = (ssl != NULL) ? 443 : 80;
	if (*hostname == '\0') {
		if (gethostname(hostname, sizeof(hostname)) == -1)
			err(1, "gethostname");
		hostname[sizeof(hostname) - 1] = '\0';
	}

	/* Make OpenSSL use our malloc/free wrappers */
	if (ssl != NULL) {
		ret = CRYPTO_set_mem_functions(ssl_malloc,
		    ssl_realloc, ssl_free);
		assert(ret);
		ret = CRYPTO_set_locked_mem_functions(ssl_malloc, ssl_free);
		assert(ret);
	}
	main_pid = getpid();

	/* Enable typed memory */
	if (typed_mem_enable() == -1)
		err(1, "typed_mem_enable");

	/* Block SIGPIPE */
	(void)signal(SIGPIPE, SIG_IGN);

	/* Get event context */
	if ((ctx = pevent_ctx_create("demo_server.ctx", NULL)) == NULL)
		err(1, "pevent_ctx_create");

	/* If client mode, do that */
	if (do_client) {
		demo_client(ctx, argc, argv, count);
		goto shutdown;
	}

	/* Start HTTP server */
	printf("demo_server: starting http server on port %d, SSL %sabled\n",
	    port, ssl ? "en" : "dis");
	if ((server = http_server_start(ctx, zero_ip,
	    port, ssl, "Demo/1.0", demo_logger)) == NULL)
		err(1, "http_server_start");

	/* Register default virtual host servlet if vhost defined */
	if (vhost != NULL) {
		/* Register "vhost.tmpl" servlet */
		if ((servlet = http_servlet_tmpl_create(
		    &vhost_servlet_info)) == NULL)
			err(1, "http_servlet_tmpl_create");
		if (http_server_register_servlet(server,
		    servlet, NULL, "^/", 0) == -1)
			err(1, "http_server_register_servlet");
	}

	/* Register redirect servlet */
	if (redirect_url != NULL) {
		if ((servlet = http_servlet_redirect_create(redirect_url,
		    HTTP_SERVLET_REDIRECT_NO_APPEND)) == NULL)
			err(1, "redirect_servlet_create");
		if (http_server_register_servlet(server,
		    servlet, vhost, "^/redirect", 0) == -1)
			err(1, "http_server_register_servlet");
	}

	/* Register cookie auth servlet */
	{
		char logurl[128];

		if ((ssl && port == 443) || (!ssl && port == 80)) {
			snprintf(logurl, sizeof(logurl),
			    "http%s://%s/logon", ssl ? "s" : "", hostname);
		} else {
			snprintf(logurl, sizeof(logurl),
			    "http%s://%s:%u/logon", ssl ? "s" : "",
			    hostname, port);
		}
		if ((cookieauth_servlet = http_servlet_cookieauth_create(
		    logurl, HTTP_SERVLET_REDIRECT_APPEND_URL,
		    demo_auth_reqd, NULL, NULL, "demo.key", demo_id,
		    sizeof(demo_id), DEMO_COOKIE_NAME)) == NULL)
			err(1, "http_servlet_cookieauth_create");
		if (http_server_register_servlet(server,
		    cookieauth_servlet, vhost, "^/.*", -20) == -1)
			err(1, "http_server_register_servlet");
	}

	/* Register logon page */
	if ((servlet = http_servlet_tmpl_create(&logon_servlet_info)) == NULL)
		err(1, "http_servlet_tmpl_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/logon$", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Register demo servlet */
	if ((servlet = demo_servlet_create()) == NULL)
		err(1, "demo_servlet_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/$", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Register "demo.tmpl" servlet */
	if ((servlet = http_servlet_tmpl_create(&tmpl_servlet_info)) == NULL)
		err(1, "http_servlet_tmpl_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/tmpl$", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Register authorization servlet */
	if ((servlet = http_servlet_basicauth_create(
	    demo_basicauth, NULL, NULL)) == NULL)
		err(1, "http_servlet_basicauth_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/[^l].*$", -10) == -1)
		err(1, "http_server_register_servlet");

	/* Register BIND docs servlet */
	if ((servlet = http_servlet_file_create(&bind_servlet_info)) == NULL)
		err(1, "http_servlet_file_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/file", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Register CGI servlet, at two different URLs */
	if ((servlet = cgi_servlet_create()) == NULL)
		err(1, "cgi_servlet_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/cgi/get$", 0) == -1)
		err(1, "http_server_register_servlet");
	if ((servlet = cgi_servlet_create()) == NULL)
		err(1, "cgi_servlet_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/cgi/post$", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Register kernel servlet */
	if ((servlet = getkernel_servlet_create()) == NULL)
		err(1, "getkernel_servlet_create");
	if (http_server_register_servlet(server,
	    servlet, vhost, "^/kernel", 0) == -1)
		err(1, "http_server_register_servlet");

	/* Wait for interrupt */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	if (sigprocmask(SIG_BLOCK, &sigs, NULL) == -1)
		err(1, "sigprocmask");
	if (sigwait(&sigs, &sig) == -1)
		err(1, "sigwait");

	/* Shut down server */
	printf("Rec'd signal %d, shutting down http server...\n", sig);
	http_server_stop(&server);

shutdown:
	/* Destroy event context */
	pevent_ctx_destroy(&ctx);
	usleep(100000);

	/* Done */
	printf("Done. Displaying unfreed memory...\n");

	/* Show memory usage */
	typed_mem_dump(stdout);
	return (0);
}

/***********************************************************************
			SUPPORT ROUTINES
***********************************************************************/

/*
 * Do client loop
 */
static int
demo_client(struct pevent_ctx *ctx, int nurls, char **urls, int count)
{
	struct http_client *client;
	struct hostent *hent;
	struct in_addr ip;
	char buf[256];
	regex_t reg;
	int i, j;
	int r;

	/* Compile URL pattern */
	if ((r = regcomp(&reg,
	    "^(http|https)://([-.[:alnum:]]+)(:[[:digit:]]+)?(/.*)?$",
	    REG_EXTENDED)) != 0) {
		regerror(r, &reg, buf, sizeof(buf));
		errx(1, "invalid URL pattern: %s", buf);
	}

	/* Create client */
	if ((client = http_client_create(ctx, "DemoClient/1.0",
	    5, 3, 7, demo_logger)) == NULL)
		err(1, "http_client_create");

	printf("Fetching %d URLs %d times...\n", nurls, count);
	for (i = 0; i < count; i++) {
	    for (j = 0; j < nurls; j++) {
		const char *const url = urls[j];
		struct http_client_connection *cc;
		struct http_request *req;
		struct http_response *resp;
		regmatch_t match[5];
		char host[256];
		char path[256];
		int https = 0;
		int port;

		/* Parse URL */
		if ((r = regexec(&reg, url, sizeof(match) / sizeof(*match),
		    match, 0)) != 0) {
			regerror(r, &reg, buf, sizeof(buf));
			errx(1, "invalid URL \"%s\": %s", url, buf);
		}
		if (match[1].rm_eo - match[1].rm_so == 4)
			port = 80;
		else {
			port = 443;
			https = 1;
		}
		if (match[3].rm_so != match[3].rm_eo) {
			strncpy(buf, url + match[3].rm_so, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			if ((port = strtoul(buf + 1, NULL, 10)) == 0)
				errx(1, "invalid port");
		}
		memset(&host, 0, sizeof(host));
		strncpy(host, url + match[2].rm_so,
		    MIN(sizeof(host) - 1, (match[2].rm_eo - match[2].rm_so)));
		if (match[4].rm_so != match[4].rm_eo) {
			memset(&path, 0, sizeof(path));
			strncpy(path, url + match[4].rm_so,
			    MIN(sizeof(path) - 1, (match[4].rm_eo
			      - match[4].rm_so)));
		} else
			strcpy(path, "/");
		if (!inet_aton(host, &ip)) {
			if ((hent = gethostbyname(host)) == NULL)
				err(1, "%s: %s", host, hstrerror(h_errno));
			ip = *((struct in_addr **)hent->h_addr_list)[0];
		}
		printf("\nFetching %s://%s:%d%s\n", https ? "https" : "http",
		    host, port, path);

		/* Create/get client connection */
		if ((cc = http_client_connect(client, ip, port, https)) == NULL)
			err(1, "http_client_connect");

		/* Set up request */
		req = http_client_get_request(cc);
		if (http_request_set_method(req, "GET") == -1)
			err(1, "http_request_set_method");
		if (http_request_set_path(req, path) == -1)
			err(1, "%s(%s)", "http_request_set_path", path);
		if (http_request_set_header(req, 0,
		    HTTP_HEADER_HOST, "%s:%u", host, port) == -1)
			err(1, "http_request_set_header");

		/* Send request */
		if ((resp = http_client_get_response(cc)) == NULL)
			printf("Error: %s\n", http_client_get_reason(cc));
		else {
			FILE *fp;
			int ch;
			int lnum;

			printf("********** RESPONSE (first 20 lines): %d %s\n",
			    http_response_get_code(resp),
			    http_response_get_header(resp, HDR_REPLY_REASON));
			if ((fp = http_response_get_input(resp)) == NULL)
				err(1, "http_response_get_input");
			for (lnum = 0; (ch = getc(fp)) != EOF; ) {
				putchar(ch);
				if (ch == '\n') {
					if (++lnum == 20)
						break;
				}
			}
			printf("*********************************\n\n\n");
		}

		/* Release client connection */
		http_client_close(&cc);

		/* Pause */
		sleep(1);
	    }
	}

	/* Release client */
	if (http_client_destroy(&client) == -1)
		err(1, "http_client_free");
	regfree(&reg);
	return (0);
}

/*
 * Demo CGI file upload handler
 */
static void
demo_file_upload(struct http_request *req, struct http_response *resp, FILE *fp)
{
	static const char *hr = "-----------------------------------------\n";
	struct mime_multipart *mp;
	u_int count;
	int i;

	/* Read multipart MIME */
	if ((mp = http_request_read_mime_multipart(req)) == NULL) {
		http_response_send_error(resp,
		    HTTP_STATUS_INTERNAL_SERVER_ERROR,
		    "Can't read multi-part MIME input: %s", strerror(errno));
		return;
	}

	/* Display parts */
	count = http_mime_multipart_get_count(mp);
	fprintf(fp, "Read %d parts:\n\n", count);
	fprintf(fp, "%s", hr);
	for (i = 0; i < count; i++) {
		struct mime_part *const part
		    = http_mime_multipart_get_part(mp, i);
		u_char *const data = http_mime_part_get_data(part);
		const u_int dlen = http_mime_part_get_length(part);
		const char *const cd = http_mime_part_get_header(part,
		    HTTP_HEADER_CONTENT_DISPOSITION);

		fprintf(fp, "PART #%u: %u bytes\n%s: %s\n%s",
		    i + 1, dlen, HTTP_HEADER_CONTENT_DISPOSITION,
		    cd == NULL ? "<< NONE >>" : cd, hr);
		fwrite(data, 1, dlen, fp);
		fprintf(fp, "\n%s", hr);
	}

	/* Free multipart data */
	http_mime_multipart_free(&mp);
}

/*
 * Exit after printing usage string
 */
static void
usage(void)
{
	(void)fprintf(stderr,
	   "Usage: demo_server [-s] [-p port] [-c count]\n"
	   "\t[-r redirect url] [-h hostname] [-v virtual-host] [url ...]\n");
	exit(1);
}

/***********************************************************************
			HTTP CALLBACKS
***********************************************************************/

/*
 * Logger for the HTTP server.
 */
static void
demo_logger(int sev, const char *fmt, ...)
{
	static char buf[512];
	va_list args;

	snprintf(buf, sizeof(buf), "http_server: %s", fmt);
	va_start(args, fmt);
	vfprintf(stderr, buf, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/***********************************************************************
			AUTHORIZOR SERVLETS
***********************************************************************/

static int
demo_auth_reqd(void *arg, struct http_request *req)
{
	const char *path = http_request_get_path(req);

	return (strcmp(path, "/logon") != 0);
}

static const char *
demo_basicauth(void *arg, struct http_request *req,
	const char *username, const char *password)
{
	if (strcmp(username, "demo") != 0 || strcmp(password, "demo") != 0)
		return ("HTTP Server Demo");
	return (NULL);
}

/***********************************************************************
			DEMO SERVLET
***********************************************************************/

static http_servlet_run_t	demo_servlet_run;
static http_servlet_destroy_t	demo_servlet_destroy;

/*
 * Demo servlet
 */
static struct http_servlet *
demo_servlet_create(void)
{
	struct http_servlet *servlet;

	if ((servlet = MALLOC(DEMO_MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = demo_servlet_run;
	servlet->destroy = demo_servlet_destroy;
	return (servlet);
}

static int
demo_servlet_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	const time_t now = time(NULL);
	char buf[32];
	FILE *fp;
	char *u;

	/* Get output stream */
	if ((fp = http_response_get_output(resp, 1)) == NULL) {
		http_response_send_error(resp,
		    HTTP_STATUS_INTERNAL_SERVER_ERROR,
		    "Can't get output stream");
		return (1);
	}

	/* Set content type */
	http_response_set_header(resp, 0,
	    HTTP_HEADER_CONTENT_TYPE, "text/html");

	/* Tell browser not to cache me */
        http_response_set_header(resp, 1, HTTP_HEADER_PRAGMA, "no-cache");
	http_response_set_header(resp, 0,
	    HTTP_HEADER_CACHE_CONTROL, "no-cache");

	/* Send some output */
	fprintf(fp, "<html><head><title>Demo Servlet</title></head>\n");
	fprintf(fp, "<body bgcolor=\"#ffffff\">\n");
	fprintf(fp, "<h3>Demo Servlet in C</h3>\n");
	fprintf(fp, "This servlet is implemented directly in C code.\n");
	fprintf(fp, "See http_servlet(3) for details.\n<br>\n");
	fprintf(fp, "<p>The time is <code>%s</code>\n", ctime_r(&now, buf));
	if ((u = http_servlet_cookieauth_user("demo.key", demo_id,
	    sizeof(demo_id), DEMO_COOKIE_NAME, req, TYPED_MEM_TEMP)) == NULL) {
		if (errno == EACCES)
			u = STRDUP(TYPED_MEM_TEMP, "(nobody)");
	}
	if (u != NULL) {
		fprintf(fp, "<p>You are logged on as <b>%s</b>.", u);
		FREE(TYPED_MEM_TEMP, u);
	} else
		fprintf(fp, "<p><b>Error with user: %s</b>.", strerror(errno));
	fprintf(fp, "<p>Click <a href=\"tmpl\">HERE</a>"
	    " for tmpl servlet demo (login as <b>demo</b>"
	    " password <b>demo</b>)\n");
	fprintf(fp, "</body></html>\n");
	fclose(fp);
	return (1);
}

static void
demo_servlet_destroy(struct http_servlet *servlet)
{
	FREE(DEMO_MEM_TYPE, servlet);
}

/***********************************************************************
			KERNEL SERVLET
***********************************************************************/

static http_servlet_run_t	getkernel_servlet_run;
static http_servlet_destroy_t	getkernel_servlet_destroy;

/*
 * Kernel servlet
 */
static struct http_servlet *
getkernel_servlet_create(void)
{
	struct http_servlet *servlet;

	if ((servlet = MALLOC(DEMO_MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = getkernel_servlet_run;
	servlet->destroy = getkernel_servlet_destroy;
	return (servlet);
}

static int
getkernel_servlet_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct stat sb;
	time_t timestamp;
	char date[64];
	struct tm tm;

	/* Make this "web page" not cachable */
	http_response_set_header(resp, 1, HTTP_HEADER_PRAGMA, "no-cache");
	http_response_set_header(resp, 0,
	    HTTP_HEADER_CACHE_CONTROL, "no-cache");

	/* Get timestamp of kernel file */
	if (stat(PATH_KERNEL, &sb) != -1)
		timestamp = sb.st_mtime;
	else
		timestamp = time(NULL); /* ok because servlet will fail too */

	/* Set MIME type */
	http_response_set_header(resp, 0, HTTP_HEADER_CONTENT_TYPE,
	    "application/octet-stream");

	/* Set filename (via Content-Disposition) based on date */
	strftime(date, sizeof(date), "%Y-%m-%d", localtime_r(&timestamp, &tm));
	http_response_set_header(resp, 0, HTTP_HEADER_CONTENT_DISPOSITION,
	    "application/octet-stream; filename=\"kernel-%s\"", date);

	/* Piggy-back off of file servlet routine */
	http_servlet_file_serve(PATH_KERNEL, demo_logger, req, resp);
	return (1);
}

static void
getkernel_servlet_destroy(struct http_servlet *servlet)
{
	FREE(DEMO_MEM_TYPE, servlet);
}

/***********************************************************************
			    CGI SERVLET
***********************************************************************/

static http_servlet_run_t	cgi_servlet_run;
static http_servlet_destroy_t	cgi_servlet_destroy;

/*
 * Demo CGI
 */
static struct http_servlet *
cgi_servlet_create(void)
{
	struct http_servlet *servlet;

	if ((servlet = MALLOC(DEMO_MEM_TYPE, sizeof(*servlet))) == NULL)
		return (NULL);
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = cgi_servlet_run;
	servlet->destroy = cgi_servlet_destroy;
	return (servlet);
}

static int
cgi_servlet_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	FILE *fp;
	int i;

	/* Set no cache */
	http_response_set_header(resp, 1, "Pragma", "no-cache");
	http_response_set_header(resp, 0, "Cache-Control", "no-cache");

	/* Set MIME type */
	http_response_set_header(resp, 0,
	    HTTP_HEADER_CONTENT_TYPE, "text/plain; charset=iso-8859-1");

	/* Get output stream */
	if ((fp = http_response_get_output(resp, 0)) == NULL) {
		http_response_send_error(resp,
		    HTTP_STATUS_INTERNAL_SERVER_ERROR,
		    "Can't get output stream");
		return (1);
	}

	/* For POST, read and decode input */
	if (strcmp(http_request_get_method(req), "POST") == 0) {
		const char *hval;
		const char *s;
		int nval;

		/* Check type of encoding */
		if ((hval = http_request_get_header(req,
		    HTTP_HEADER_CONTENT_TYPE)) == NULL) {
			http_response_send_error(resp,
			    HTTP_STATUS_INTERNAL_SERVER_ERROR,
			    "Missing %s header", HTTP_HEADER_CONTENT_TYPE);
			return (1);
		}

		/* Special case form file upload */
		if ((s = strchr(hval, ';')) != NULL
		    && strncasecmp(hval,
		      HTTP_CTYPE_MULTIPART_FORMDATA, s - hval) == 0) {
			demo_file_upload(req, resp, fp);
			return (1);
		}

		/* Must be POST form data */
		if (strcasecmp(hval, HTTP_CTYPE_FORM_URLENCODED) != 0) {
			http_response_send_error(resp,
			    HTTP_STATUS_INTERNAL_SERVER_ERROR,
			    "Weird content-type \"%s\"", hval);
			return (1);
		}

		/* Read POST form data */
		if ((nval = http_request_read_url_encoded_values(req)) == -1) {
			http_response_send_error(resp,
			    HTTP_STATUS_INTERNAL_SERVER_ERROR,
			    "Can't read POST input");
			return (1);
		}
		fprintf(fp, "Read %d values from input\n", nval);
	}

	/* Get fields and display them */
	for (i = 1; 1; i++) {
		const char *value;
		char fname[32];

		snprintf(fname, sizeof(fname), "field%d", i);
		fprintf(fp, "FIELD \"%s\": ", fname);
		if ((value = http_request_get_value(req, fname, 0)) == NULL) {
			fprintf(fp, "<< NOT FOUND >>\n");
			break;
		} else
			fprintf(fp, "value=\"%s\"\n", value);
	}
	return (1);
}

static void
cgi_servlet_destroy(struct http_servlet *servlet)
{
	FREE(DEMO_MEM_TYPE, servlet);
}

/***********************************************************************
			TMPL USER FUNCTIONS
***********************************************************************/

static tmpl_handler_t	authname_handler;
static tmpl_handler_t	authorize_handler;
static tmpl_handler_t	date_handler;
static tmpl_handler_t	get_header_handler;
static tmpl_handler_t	get_port_handler;
static tmpl_handler_t	get_ssl_handler;
static tmpl_handler_t	query_handler;
static tmpl_handler_t	query_string_handler;
static tmpl_handler_t	redirect_handler;
static tmpl_handler_t	remote_ip_handler;
static tmpl_handler_t	remote_port_handler;
static tmpl_handler_t	shutdown_handler;
static tmpl_handler_t	sleep_handler;
static tmpl_handler_t	vhost_handler;

static const struct tmpl_func demo_tmpl_funcs[] = {
	{ "authname",		0, 0,	authname_handler	},
	{ "authorize",		0, 2,	authorize_handler	},
	{ "date",		0, 0,	date_handler		},
	{ "get_header",		1, 1,	get_header_handler	},
	{ "get_port",		0, 0,	get_port_handler	},
	{ "get_ssl",		0, 0,	get_ssl_handler		},
	{ "query",		1, 1,	query_handler		},
	{ "query_string",	0, 0,	query_string_handler	},
	{ "redirect",		0, 0,	redirect_handler	},
	{ "remote_ip",		0, 0,	remote_ip_handler	},
	{ "remote_port",	0, 0,	remote_port_handler	},
	{ "shutdown",		0, 0,	shutdown_handler	},
	{ "sleep",		1, 1,	sleep_handler		},
	{ "vhost",		0, 0,	vhost_handler		},
};

static char *
demo_tmpl_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	return (tmpl_list_handler(ctx, demo_tmpl_funcs,
	    sizeof(demo_tmpl_funcs) / sizeof(*demo_tmpl_funcs),
	    errmsgp, ac, av));
}

static char *
remote_ip_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype,
	    inet_ntoa(http_request_get_remote_ip(targ->req))));
}

static char *
remote_port_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[32];

	snprintf(buf, sizeof(buf), "%u",
	    http_request_get_remote_port(targ->req));
	return (STRDUP(mtype, buf));
}

static char *
redirect_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype, redirect_url ? redirect_url : ""));
}

static char *
date_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const time_t now = time(NULL);
	char buf[32];

	return (STRDUP(mtype, ctime_r(&now, buf)));
}

static char *
sleep_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	http_response_send_headers(targ->resp, 1);
	sleep(atoi(av[1]));
	return (STRDUP(mtype, ""));
}

static char *
authorize_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	/* Login */
	if (ac > 2 && *av[2] != '\0') {
		const u_long expiry = strtoul(av[2], NULL, 10);

		if (http_servlet_cookieauth_login(targ->resp,
		    "demo.key", av[1], 3600, expiry ? time(NULL) + expiry : 0,
		    expiry == 0, demo_id, sizeof(demo_id), DEMO_COOKIE_NAME,
		    "/", NULL, 0) == -1)
			return (NULL);
		return (STRDUP(mtype, ""));
	}

	/* Logout */
	if (http_servlet_cookieauth_logout(DEMO_COOKIE_NAME,
	    "/", NULL, targ->resp) == -1)
		return (NULL);
	return (STRDUP(mtype, ""));
}

static char *
authname_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char *name;

	if ((name = http_servlet_cookieauth_user("demo.key", demo_id,
	    sizeof(demo_id), DEMO_COOKIE_NAME, targ->req, mtype)) == NULL) {
		if (errno == EACCES)
			name = STRDUP(mtype, "");
	}
	if (name == NULL)
		return (NULL);
	return (name);
}

static char *
query_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *value;

	if ((value = http_request_get_value(targ->req, av[1], 0)) == NULL)
		value = "";
	return (STRDUP(mtype, value));
}

static char *
query_string_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *eqs = http_request_get_query_string(targ->req);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char *dqs;

	/* URL-decode query string */
	if ((dqs = MALLOC(mtype, strlen(eqs) + 1)) == NULL)
		return (NULL);
	http_request_url_decode(eqs, dqs);

	/* Return it */
	return (dqs);
}

static char *
get_header_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	struct http_servlet_tmpl_arg *const targ = tmpl_ctx_get_arg(ctx);
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	const char *value;

	if ((value = http_request_get_header(targ->req, av[1])) == NULL)
		value = "";
	return (STRDUP(mtype, value));
}

static char *
get_port_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char buf[32];

	snprintf(buf, sizeof(buf), "%u", port);
	return (STRDUP(mtype, buf));
}

static char *
get_ssl_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype, ssl != NULL ? "1" : "0"));
}

static char *
vhost_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	return (STRDUP(mtype, vhost == NULL ? "" : vhost));
}

static char *
shutdown_handler(struct tmpl_ctx *ctx, char **errmsgp, int ac, char **av)
{
	const char *const mtype = tmpl_ctx_get_mtype(ctx);

	kill(main_pid, SIGTERM);
	return (STRDUP(mtype, ""));
}

/*
 * Error formatter for templates.
 */
static char *
demo_tmpl_errfmtr(struct tmpl_ctx *ctx, const char *errmsg)
{
	static const char fmt[] =
	    "<font color=\"#ff0000\"><strong>Error: %s</strong></font>";
	const char *const mtype = tmpl_ctx_get_mtype(ctx);
	char *string;
	int slen;

	slen = sizeof(fmt) + strlen(errmsg);
	if ((string = MALLOC(mtype, slen)) == NULL)
		return (NULL);
	snprintf(string, slen, fmt, errmsg);
	return (string);
}

/***********************************************************************
			SSL MEMORY WRAPPERS
***********************************************************************/

/*
 * OpenSSL malloc() wrappers
 */
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
	return (FREE("OpenSSL", mem));
}

