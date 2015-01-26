
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/uio.h>
#endif
#ifdef __linux__
#include <sys/sendfile.h>
#endif

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

#include <openssl/ssl.h>

#include "pdel/pd_string.h"

#include "structs/structs.h"
#include "structs/type/array.h"

#include "sys/alog.h"
#include "io/string_fp.h"
#include "tmpl/tmpl.h"
#include "util/ghash.h"
#include "util/typed_mem.h"

#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/tmpl.h"
#include "http/servlet/file.h"

#define	MEM_TYPE	"http_servlet_file"
#define	TMPL_SUFFIX	".tmpl"

#ifdef WIN32
#define DIRSEPC  '\\'
#define DIRSEPS  "\\"
#else
#define DIRSEPC '/'
#define DIRSEPS "/"
#endif

/* Cleanup state for http_servlet_file_serve() */
struct http_servlet_file_serve_state {
	int	fd;
};

/* One cached template */
struct tmpl_cache {
	char			*path;		/* file pathname */
	struct http_servlet	*servlet;	/* tmpl servlet */
};

/* Servlet state */
struct file_private {
	struct http_servlet_file_info	*info;
	struct ghash			*tmpls;	/* cached tmpl servlets */
	http_servlet_tmpl_free_t	*freer;	/* freer for tmpl arg */
};

/* Directory -> file redirects */
static const	char *http_servlet_file_dirindex[][2] = {
	{ "index.tmpl",	"index" },
	{ "index.html",	"index.html" },
	{ "index.htm",	"index.htm" },
	{ NULL, NULL }
};

/*
 * Internal functions
 */
static char	*http_servlet_file_gen_filename(
			struct http_servlet_file_info *finfo, const char *url,
			const char *mtype);
static void	http_servlet_file_tmpl(struct http_servlet *servlet,
			const char *path, struct http_request *req,
			struct http_response *resp);
static void	http_servlet_file_serve_cleanup(void *arg);

static http_servlet_run_t	http_servlet_file_run;
static http_servlet_destroy_t	http_servlet_file_destroy;

static ghash_hash_t	tmpl_cache_hash;
static ghash_equal_t	tmpl_cache_equal;
static ghash_del_t	tmpl_cache_del;

/*
 * Create a new file servlet.
 */
struct http_servlet *
http_servlet_file_create(const struct http_servlet_file_info *info)
{
	struct http_servlet *servlet = NULL;
	struct file_private *priv = NULL;

	/* Create servlet */
	if ((servlet = MALLOC(MEM_TYPE, sizeof(*servlet))) == NULL)
		goto fail;
	memset(servlet, 0, sizeof(*servlet));
	servlet->run = http_servlet_file_run;
	servlet->destroy = http_servlet_file_destroy;

	/* Initialize private info */
	if ((priv = MALLOC(MEM_TYPE, sizeof(*priv))) == NULL)
		goto fail;
	memset(priv, 0, sizeof(*priv));

	/* Copy "info" */
	if ((priv->info = MALLOC(MEM_TYPE, sizeof(*priv->info))) == NULL)
		goto fail;
	memset(priv->info, 0, sizeof(*priv->info));
	if (info->docroot != NULL
	    && (priv->info->docroot = STRDUP(MEM_TYPE, info->docroot)) == NULL)
		goto fail;
	priv->info->allow_escape = info->allow_escape;
	if (info->filename != NULL
	    && (priv->info->filename
	      = STRDUP(MEM_TYPE, info->filename)) == NULL)
		goto fail;
	if (info->prefix != NULL
	    && (priv->info->prefix = STRDUP(MEM_TYPE, info->prefix)) == NULL)
		goto fail;
	if (info->mime_type != NULL
	    && (priv->info->mime_type
	      = STRDUP(MEM_TYPE, info->mime_type)) == NULL)
		goto fail;
	if (info->mime_encoding != NULL
	    && (priv->info->mime_encoding
	      = STRDUP(MEM_TYPE, info->mime_encoding)) == NULL)
		goto fail;
	priv->info->logger = info->logger;
	priv->info->hide = info->hide;
	if (_http_servlet_tmpl_copy_tinfo(&priv->info->tinfo,
	    &info->tinfo) == -1)
		goto fail;

	/* Only free the template argument once */
	priv->freer = priv->info->tinfo.freer;
	priv->info->tinfo.freer = NULL;

	/* Create template hash table */
	if ((priv->tmpls = ghash_create(priv, 0, 200, MEM_TYPE,
	    tmpl_cache_hash, tmpl_cache_equal, NULL, tmpl_cache_del)) == NULL)
		goto fail;
	servlet->arg = priv;

	/* OK */
	return (servlet);

fail:
	/* Clean up after failure */
	if (priv != NULL) {
		if (priv->info != NULL) {
			FREE(MEM_TYPE, (char *)priv->info->filename);
			FREE(MEM_TYPE, (char *)priv->info->docroot);
			FREE(MEM_TYPE, (char *)priv->info->prefix);
			FREE(MEM_TYPE, (char *)priv->info->mime_type);
			FREE(MEM_TYPE, (char *)priv->info->mime_encoding);
			_http_servlet_tmpl_free_tinfo(&priv->info->tinfo);
			FREE(MEM_TYPE, priv->info);
		}
		ghash_destroy(&priv->tmpls);
		FREE(MEM_TYPE, priv);
	}
	if (servlet != NULL)
		FREE(MEM_TYPE, servlet);
	return (NULL);
}

/*
 * Destroy a file servlet.
 */
static void
http_servlet_file_destroy(struct http_servlet *servlet)
{
	struct file_private *const priv = servlet->arg;

	/* Free template argument */
	if (priv->freer != NULL)
		(*priv->freer)(priv->info->tinfo.arg);

	/* Free private info */
	FREE(MEM_TYPE, (char *)priv->info->filename);
	FREE(MEM_TYPE, (char *)priv->info->docroot);
	FREE(MEM_TYPE, (char *)priv->info->prefix);
	FREE(MEM_TYPE, (char *)priv->info->mime_type);
	FREE(MEM_TYPE, (char *)priv->info->mime_encoding);
	_http_servlet_tmpl_free_tinfo(&priv->info->tinfo);
	FREE(MEM_TYPE, priv->info);
	ghash_destroy(&priv->tmpls);
	FREE(MEM_TYPE, priv);

	/* Free servlet */
	FREE(MEM_TYPE, servlet);
}

/*
 * Execute file servlet.
 */
static int
http_servlet_file_run(struct http_servlet *servlet,
	struct http_request *req, struct http_response *resp)
{
	struct file_private *const priv = servlet->arg;
	struct http_servlet_file_info *const info = priv->info;
	const char *const urlpath = http_request_get_path(req);
	char *path = NULL;
	int got_tmpl = 0;
	size_t len;

	/* Generate file name from URL; first try to find a template file */
	if (info->tinfo.handler != NULL
	    && (len = strlen(urlpath)) < MAXPATHLEN - sizeof(TMPL_SUFFIX)) {
		char tp[MAXPATHLEN];
		struct stat sb;

		memcpy(tp, urlpath, len);
		memcpy(tp + len, TMPL_SUFFIX, sizeof(TMPL_SUFFIX));
		path = http_servlet_file_gen_filename(info, tp, TYPED_MEM_TEMP);
		if (path != NULL) {
			if (stat(path, &sb) == 0 
			    && (sb.st_mode & S_IFMT) == S_IFREG)
				got_tmpl = 1;
			else {
				FREE(TYPED_MEM_TEMP, path);
				path = NULL;
			}
		}
	}

	/* Generate file name from URL; now try a normal file */
	if (!got_tmpl
	    && (path = http_servlet_file_gen_filename(info,
	      urlpath, TYPED_MEM_TEMP)) == NULL) {
		http_response_send_errno_error(resp);
		return (1);
	}

	/* Handle templates */
	if (got_tmpl) {
		http_servlet_file_tmpl(servlet, path, req, resp);
		goto done;
	}

	/* Check whether to hide this file */
	if (info->hide != NULL && (*info->hide)(info, req, resp, path)) {
		FREE(TYPED_MEM_TEMP, path);
		return (0);			/* continue with next servlet */
	}

	/* Use supplied MIME info, if any */
	if (info->mime_type != NULL) {
		http_response_set_header(resp, 0,
		    HTTP_HEADER_CONTENT_TYPE, "%s", info->mime_type);
		if (info->mime_encoding != NULL) {
			http_response_set_header(resp, 0,
			    HTTP_HEADER_CONTENT_ENCODING,
			    "%s", info->mime_encoding);
		}
	}

	/* Serve up file */
	http_servlet_file_serve(path, info->logger, req, resp);

done:
	/* Done */
	FREE(TYPED_MEM_TEMP, path);
	return (1);
}

#define MAX_ENCODINGS	10

/*
 * Serve up a file.
 *
 * This is a public function usable by other servlets.
 */
void
http_servlet_file_serve(const char *path, http_logger_t *logger,
	struct http_request *req, struct http_response *resp)
{
	const char *hval;
	struct stat sb;
	char buf[1024];
	FILE *output;
	struct tm tm;
	time_t when;
#if !defined(__CYGWIN__) && !defined(WIN32)
	int sock;
#endif

	/* Stat file */
	if (stat(path, &sb) == -1) {
fail_errno:	http_response_send_errno_error(resp);
		return;
	}

	/* If file is a directory, redirect to default file if it exists */
	if ((sb.st_mode & S_IFMT) == S_IFDIR) {
		int i;

		for (i = 0; http_servlet_file_dirindex[i][0] != NULL; i++) {
			const char *qs = http_request_get_query_string(req);
			char *urlpath;
			char *dfile;

			if (qs == NULL)
				qs = "";
			ASPRINTF(TYPED_MEM_TEMP, &dfile, "%s/%s", path _
			    http_servlet_file_dirindex[i][0]);
			if (dfile == NULL)
				goto fail_errno;
			if (stat(dfile, &sb) == -1) {
				FREE(TYPED_MEM_TEMP, dfile);
				continue;
			}
			if ((urlpath = http_request_url_encode(TYPED_MEM_TEMP,
			    http_request_get_path(req))) == NULL) {
				FREE(TYPED_MEM_TEMP, dfile);
				goto fail_errno;
			}
			if (http_response_set_header(resp, 0,
			    HTTP_HEADER_LOCATION, "%s%s%s%s%s", urlpath,
			    "/" + (urlpath[strlen(urlpath) - 1] == '/'),
			    http_servlet_file_dirindex[i][1],
			    (*qs != '\0') ? "?" : "", qs) == -1) {
				FREE(TYPED_MEM_TEMP, urlpath);
				FREE(TYPED_MEM_TEMP, dfile);
				goto fail_errno;
			}
			FREE(TYPED_MEM_TEMP, urlpath);
			FREE(TYPED_MEM_TEMP, dfile);
			http_response_send_error(resp,
			    HTTP_STATUS_FOUND, NULL);
			return;
		}
	}

	/* File must be regular */
	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		errno = ENOENT;			/* hide non-regular files */
		goto fail_errno;
	}

	/* Set timestamp from stat(2) info */
	strftime(buf, sizeof(buf), HTTP_TIME_FMT_RFC1123,
	    gmtime_r(&sb.st_mtime, &tm));
	http_response_set_header(resp, 0, HTTP_HEADER_DATE, "%s", buf);

	/* Check for If-Modified-Since: header */
	if ((hval = http_request_get_header(req,
	    HTTP_HEADER_IF_MODIFIED_SINCE)) != NULL) {
		if ((when = http_request_parse_time(hval)) != (time_t)-1
		    && sb.st_mtime <= when) {
			http_response_send_error(resp,
			    HTTP_STATUS_NOT_MODIFIED, NULL);
			return;
		}
	}

	/* Set MIME type if not set already */
	if (http_response_get_header(resp, HTTP_HEADER_CONTENT_TYPE) == NULL) {
		const char *cencs[MAX_ENCODINGS];
		const char *ctype;
		int i;

		http_response_guess_mime(path, &ctype, cencs, MAX_ENCODINGS);
		http_response_set_header(resp, 0,
		    HTTP_HEADER_CONTENT_TYPE, "%s", ctype);
		for (i = 0; i < MAX_ENCODINGS && cencs[i] != NULL; i++) {
			http_response_set_header(resp, i > 0,
			    HTTP_HEADER_CONTENT_ENCODING, "%s", cencs[i]);
		}
	}

	/* Set content length */
	http_response_set_header(resp, 0,
	    HTTP_HEADER_CONTENT_LENGTH, "%lu", (u_long)sb.st_size);

	/* Get servlet output stream (unbuffered) */
	if ((output = http_response_get_output(resp, 0)) == NULL) {
		(*logger)(LOG_ERR, "can't get response output: %s",
		    strerror(errno));
		return;
	}

	/* Send file contents, using sendfile(2) if possible */
#if !defined(__CYGWIN__) && !defined(WIN32)
	if ((sock = http_response_get_raw_socket(resp)) != -1) {
		struct http_servlet_file_serve_state state;

		/* Open file */
		if ((state.fd = open(path, O_RDONLY)) == -1)
			goto fail_errno;

		/* Set cleanup hook in case thread is canceled */
		pthread_cleanup_push(http_servlet_file_serve_cleanup, &state);

		/* Make sure headers are sent first */
		http_response_send_headers(resp, 1);
		fflush(output);

		/* Send file directly using sendfile(2) */
#ifndef __linux__
		sendfile(state.fd, sock, 0, sb.st_size, NULL, NULL, 0);
#else
		sendfile(sock, state.fd, NULL, sb.st_size);
#endif

		/* Close file */
		pthread_cleanup_pop(1);
	} else 
#endif
	{

		FILE *fp;
		int ret;

		/* Open file */
		if ((fp = fopen(path, "r")) == NULL)
			goto fail_errno;

		/* Set cleanup hook in case thread is canceled */
		pthread_cleanup_push((void (*)(void *))fclose, fp);

		/* Tranfer file contents */
		while (1) {
			if ((ret = fread(buf, 1, sizeof(buf), fp)) != 0) {
				if (fwrite(buf, 1, ret, output) < ret)
					break;
			}
			if (ret < sizeof(buf))
				break;
		}

		/* Close file */
		pthread_cleanup_pop(1);
	}
}

/*
 * Do a template file.
 */
static void
http_servlet_file_tmpl(struct http_servlet *servlet, const char *path,
	struct http_request *req, struct http_response *resp)
{
	struct file_private *const priv = servlet->arg;
	struct http_servlet_file_info *const info = priv->info;
	struct http_servlet_tmpl_info ti;
	char mimepath[MAXPATHLEN + 1];
	struct tmpl_cache *t = NULL;
	struct tmpl_cache key;
	const char *s;

	/* See if template already cached */
	key.path = (char *)path;
	if ((t = ghash_get(priv->tmpls, &key)) != NULL)
		goto found;

	/* Create new cached entry */
	if ((t = MALLOC(MEM_TYPE, sizeof(*t))) == NULL)
		goto fail;
	memset(t, 0, sizeof(*t));
	if ((t->path = STRDUP(MEM_TYPE, path)) == NULL)
		goto fail;

	/* Set info required by the template servlet */
	memset(&ti, 0, sizeof(ti));
	ti.path = t->path;
	ti.tinfo = info->tinfo;
	ti.logger = info->logger;

	/* Figure out templates's output MIME type */
	strlcpy(mimepath, path, sizeof(mimepath));
	mimepath[strlen(mimepath) - strlen(TMPL_SUFFIX)] = '\0';
	if ((s = strrchr(mimepath, '.')) == NULL
	    || strchr(s, '/') != NULL)		/* no suffix? assume html */
		strlcat(mimepath, "x.html", sizeof(mimepath));
	http_response_guess_mime(mimepath, &ti.mime_type, NULL, 0);

	/* Create template servlet */
	if ((t->servlet = http_servlet_tmpl_create(&ti)) == NULL)
		goto fail;

	/* Add it to hash table */
	if (ghash_put(priv->tmpls, t) == -1) {
		(*info->logger)(LOG_ERR,
		    "%s: %s", "ghash_put", strerror(errno));
fail:		FREE(MEM_TYPE, t);
		http_response_send_errno_error(resp);
		return;
	}

found:
	/* Invoke servlet */
	(*t->servlet->run)(t->servlet, req, resp);
}

static void
http_servlet_file_serve_cleanup(void *arg)
{
	const struct http_servlet_file_serve_state *const state = arg;

	close(state->fd);
}

static u_int32_t
tmpl_cache_hash(struct ghash *g, const void *item)
{
	const struct tmpl_cache *const t = item;
	u_int32_t hash;
	const char *s;

	for (hash = 0, s = t->path; *s != '\0'; s++)
		hash = (31 * hash) + (u_char)*s;
	return (hash);
}

static int
tmpl_cache_equal(struct ghash *g, const void *item1, const void *item2)
{
	const struct tmpl_cache *const t1 = item1;
	const struct tmpl_cache *const t2 = item2;

	return (strcmp(t1->path, t2->path) == 0);
}

static void
tmpl_cache_del(struct ghash *g, void *item)
{
	struct tmpl_cache *const t = item;

	http_server_destroy_servlet(&t->servlet);
	FREE(MEM_TYPE, t->path);
	FREE(MEM_TYPE, t);
}

/*
 * Compute a filename from supplied info and URL.
 *
 * Caller must free returned string, which is in a buffer of size MAXPATHLEN.
 */
static char *
http_servlet_file_gen_filename(struct http_servlet_file_info *info,
	const char *urlpath, const char *mtype)
{
	char path[MAXPATHLEN];
	char *rpath;
	char *tok;
	char *s;

	/* Sanity check */
	assert(*urlpath == '/');

	/* Disallow all ".", "..", and empty components within urlpath */
	strlcpy(path, urlpath, sizeof(path));
	for (s = path + 1; (tok = strsep(&s, "/")) != NULL; ) {
		if ((*tok == '\0' && s != NULL)
		     || strcmp(tok, ".") == 0 || strcmp(tok, "..") == 0) {
			errno = ENOENT;
			return (NULL);
		}
	}

	/* Prepend root directory, if any */
	if (info->docroot != NULL) {
		strlcpy(path, info->docroot, sizeof(path) - 1);
		if (path[strlen(path) - 1] != DIRSEPC)
			strlcat(path, DIRSEPS, sizeof(path));
	} else
		*path = '\0';

	/* Add fixed filename, if any */
	if (info->filename != NULL) {
		if (*info->filename == DIRSEPC)
			*path = '\0';
		strlcat(path, info->filename, sizeof(path));
		goto normalize;
	}

	/* Strip URL prefix, if it matches */
	if (info->prefix != NULL
	    && strncmp(urlpath, info->prefix, strlen(info->prefix)) == 0)
		urlpath += strlen(info->prefix);

	/* Derive remainder of pathname from URL */
	strlcat(path, urlpath + (*urlpath == '/'), sizeof(path));

#ifdef WIN32
	/* Convert any / from urlpath to \ */
	tok = path;
	while ((tok = strchr(path, '/')) != NULL) {
		*tok = '\\';
		tok++;
	}
#endif

normalize:

	/* Normalize path */
	if ((rpath = MALLOC(mtype, MAXPATHLEN)) == NULL)
		return (NULL);
	if (realpath(path, rpath) == NULL) {
		FREE(mtype, rpath);
		return (NULL);
	}
	rpath[MAXPATHLEN - 1] = '\0';

	/* Verify that file is within the document root directory hierarchy */
	if (!info->allow_escape) {
		const char *docroot;
		char *dpath;
		size_t rlen;
		int within;

		/* Use current working directory if info->docroot is NULL */
		if (info->docroot == NULL) {
			getcwd(path, sizeof(path));
			path[sizeof(path) - 1] = '\0';
			docroot = path;
		} else
			docroot = info->docroot;

		/* Normalize docroot path */
		if ((dpath = MALLOC(mtype, MAXPATHLEN)) == NULL) {
			FREE(mtype, rpath);
			return (NULL);
		}
		if (realpath(docroot, dpath) == NULL) {
			FREE(mtype, dpath);
			FREE(mtype, rpath);
			return (NULL);
		}
		dpath[MAXPATHLEN - 1] = '\0';

		/* Verify that path is within the root */
		rlen = strlen(dpath);
		within = strncmp(rpath, dpath, rlen) == 0
		    && (rpath[rlen] == '\0' || rpath[rlen] == DIRSEPC);
		FREE(mtype, dpath);
		if (!within) {
			FREE(mtype, rpath);
			errno = ENOENT;
			return (NULL);
		}
	}

	/* Done */
	return (rpath);
}

