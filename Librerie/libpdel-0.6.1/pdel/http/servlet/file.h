
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_SERVLET_FILE_H_
#define _PDEL_HTTP_SERVLET_FILE_H_

#ifdef BUILDING_PDEL
#include "tmpl/tmpl.h"
#include "http/servlet/tmpl.h"
#else
#include "pdel/tmpl/tmpl.h"
#include "pdel/http/servlet/tmpl.h"
#endif

struct http_servlet_file_info;

/*
 * Optional 'screener' function that tells whether a file in
 * the hierarchy should be visible or not. If not supplied,
 * all normal (and symlinked to normal) files will be visible.
 *
 * The "path" is the filesystem pathname to the file.
 *
 * Returns non-zero to hide the file, else zero to publish.
 */
typedef int	http_servlet_file_hide_t(
			const struct http_servlet_file_info *info,
			struct http_request *req, struct http_response *resp,
			const char *path);

/* Info required for this servlet */
struct http_servlet_file_info {
	const char	*docroot;	/* document root, or NULL for cwd */
	u_char		allow_escape;	/* allow url to escape docroot */
	const char	*filename;	/* filename, or NULL to use url */
	const char	*prefix;	/* url prefix to strip, or NULL */
	const char	*mime_type;	/* mime type, or NULL to guess */
	const char	*mime_encoding;	/* mime encoding (only if mime_type) */
	http_logger_t	*logger;	/* http error logger */

	/* Set this to hide certain files */
	http_servlet_file_hide_t	*hide;

	/* Fill in this struct to enable processing of '*.tmpl' as templates */
	struct http_servlet_tmpl_tinfo	tinfo;
};

__BEGIN_DECLS

/*
 * Create a new file servlet.
 *
 * NOTE: the 'info' structure is not copied; it must remain
 * usable as long as the servlet exists.
 */
extern struct	http_servlet *http_servlet_file_create(
			const struct http_servlet_file_info *info);

/*
 * General purpose function for pushing out the contents of a file
 * into an HTTP response.
 */
extern void	http_servlet_file_serve(const char *path, http_logger_t *logger,
			struct http_request *req, struct http_response *resp);

__END_DECLS

#endif	/* _PDEL_HTTP_SERVLET_FILE_H_ */
