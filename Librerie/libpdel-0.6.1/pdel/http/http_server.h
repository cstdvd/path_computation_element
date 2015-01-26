/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_HTTP_HTTP_SERVER_H_
#define _PDEL_HTTP_HTTP_SERVER_H_

#include <stdio.h>

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif

struct http_connection;
struct http_request;
struct http_response;
struct http_server;
struct http_servlet;
struct mime_multipart;
struct mime_part;
struct pevent_ctx;

typedef struct http_connection	*http_connection_h;
typedef struct http_request	*http_request_h;
typedef struct http_response	*http_response_h;
typedef struct http_server	*http_server_h;
typedef struct mime_multipart	*mime_multipart_h;
typedef struct mime_part	*mime_part_h;

/*
 * SSL info supplied by the application
 */
typedef struct http_server_ssl {
	const char	*cert_path;	/* path to certificate file */
	const char	*pkey_path;	/* path to private key file */
	const char	*pkey_password;	/* private key password, if needed */
} http_server_ssl;

typedef http_server_ssl	*http_server_ssl_h;

/*
 * Special "headers" from the first line
 * of an HTTP request or HTTP response.
 */
#define	HDR_REQUEST_METHOD	((const char *)1)
#define	HDR_REQUEST_URI		((const char *)2)
#define	HDR_REQUEST_VERSION	((const char *)3)
#define	HDR_REPLY_VERSION	((const char *)1)
#define	HDR_REPLY_STATUS	((const char *)2)
#define	HDR_REPLY_REASON	((const char *)3)

/*
 * User callback types
 */
typedef void	http_logger_t(int sev, const char *fmt, ...);
typedef void	http_proxy_t(void *arg, struct http_request *req,
                        struct http_response *resp);

/*
 * Should return 0 to proceed, or -1 and set errno to abort.
 * The handler should NOT close the stream and should NOT free "part".
 */
typedef int	http_mime_handler_t(void *arg,
			struct mime_part *part, FILE *fp);

__BEGIN_DECLS

/*
 * Server functions
 */
extern struct	http_server *http_server_start(struct pevent_ctx *ctx,
			struct in_addr ip, u_int16_t port,
			const struct http_server_ssl *ssl,
			const char *server_name, http_logger_t *logger);
extern void	http_server_stop(struct http_server **serverp);
extern int	http_server_register_servlet(struct http_server *serv,
			struct http_servlet *servlet, const char *vhost,
			const char *urlpat, int order);
extern void	http_server_destroy_servlet(struct http_servlet **servletp);
extern void	http_server_set_proxy_handler(struct http_server *serv,
			http_proxy_t *handler, void *arg);

/*
 * Client functions
 */
extern struct	http_client *http_client_create(struct pevent_ctx *ctx,
			const char *user_agent, u_int max_conn, u_int max_cache,
			u_int max_cache_idle, http_logger_t *logger);
extern int	http_client_destroy(struct http_client **clientp);

extern struct	http_client_connection *http_client_connect(
			struct http_client *client, struct in_addr ip,
			u_int16_t port, int https);
extern struct	in_addr http_client_get_local_ip(
			struct http_client_connection *cc);
extern u_int16_t http_client_get_local_port(struct http_client_connection *cc);
extern struct	http_request *http_client_get_request(
			struct http_client_connection *client);
extern struct	http_response *http_client_get_response(
			struct http_client_connection *client);
extern void	http_client_close(struct http_client_connection **cconp);
extern const	char *http_client_get_reason(
			struct http_client_connection *ccon);

/*
 * Request functions
 */
extern const	char *http_request_get_query_string(struct http_request *req);
extern const	char *http_request_get_method(struct http_request *req);
extern int	http_request_set_method(struct http_request *req,
			const char *method);
extern const	char *http_request_get_uri(struct http_request *req);
extern const	char *http_request_get_version(struct http_request *req);
extern const	char *http_request_get_path(struct http_request *req);
extern int	http_request_set_path(struct http_request *req,
			const char *path);
#ifdef SSL_ERROR_NONE
extern SSL_CTX	*http_request_get_ssl(struct http_request *req);
#endif
extern const	char *http_request_get_header(struct http_request *req,
			const char *name);
extern int	http_request_num_headers(struct http_request *req);
extern int	http_request_get_header_by_index(struct http_request *req,
			u_int index, const char **namep, const char **valuep);
extern void	http_request_set_proxy(struct http_request *req, int whether);
extern int	http_request_set_header(struct http_request *req,
			int append, const char *name, const char *valfmt, ...)
			__printflike(4, 5);
extern int	http_request_remove_header(struct http_request *req,
			const char *name);
extern int	http_request_send_headers(struct http_request *req);
extern FILE	*http_request_get_input(struct http_request *req);
extern FILE	*http_request_get_output(struct http_request *req, int buffer);
extern const	char *http_request_get_username(struct http_request *req);
extern const	char *http_request_get_password(struct http_request *req);
extern const	char *http_request_get_host(struct http_request *req);
extern struct	in_addr http_request_get_remote_ip(struct http_request *req);
extern u_int16_t http_request_get_remote_port(struct http_request *req);
extern const	char *http_request_get_value(struct http_request *req,
			const char *name, int instance);
extern int	http_request_set_value(struct http_request *req,
			const char *name, const char *value);
extern int	http_request_get_num_values(struct http_request *req);
extern int	http_request_get_value_by_index(struct http_request *req,
			int index, const char **name, const char **value);
extern int	http_request_set_query_from_values(struct http_request *req);
extern int	http_request_read_url_encoded_values(struct http_request *req);
extern int	http_request_write_url_encoded_values(struct http_request *req);

extern int	http_request_get_mime_multiparts(struct http_request *req,
			http_mime_handler_t *handler, void *arg);
extern struct	mime_multipart *http_request_read_mime_multipart(
			struct http_request *req);
extern int	http_request_file_upload(struct http_request *req,
			const char *field, FILE *fp, size_t max);

extern time_t	http_request_parse_time(const char *string);
extern void	http_request_url_decode(const char *s, char *t);
extern char	*http_request_url_encode(const char *mtype, const char *string);
extern char	*http_request_encode_basic_auth(const char *mtype,
			const char *username, const char *password);
extern int	http_request_get_raw_socket(struct http_request *req);

/*
 * mime_multipart methods
 */
extern u_int	http_mime_multipart_get_count(struct mime_multipart *mp);
extern struct	mime_part *http_mime_multipart_get_part(
			struct mime_multipart *mp, u_int index);
extern void	http_mime_multipart_free(struct mime_multipart **mpp);

/*
 * mime_part methods
 */
extern const	char *http_mime_part_get_header(struct mime_part *part,
			const char *name);
extern u_int	http_mime_part_get_length(struct mime_part *part);
extern u_char	*http_mime_part_get_data(struct mime_part *part);

/*
 * Response functions
 */
#ifdef SSL_ERROR_NONE
extern SSL_CTX	*http_response_get_ssl(struct http_response *resp);
#endif
extern const	char *http_response_get_header(struct http_response *resp,
			const char *name);
extern int	http_response_num_headers(struct http_response *req);
extern int	http_response_get_header_by_index(struct http_response *resp,
			u_int index, const char **namep, const char **valuep);
extern int	http_response_set_header(struct http_response *resp,
			int append, const char *name, const char *valfmt, ...)
			__printflike(4, 5);
extern int	http_response_remove_header(struct http_response *resp,
			const char *name);
extern int	http_response_send_headers(struct http_response *resp,
			int unbuffer);
extern int	http_response_get_code(struct http_response *resp);
extern FILE	*http_response_get_input(struct http_response *resp);
extern FILE	*http_response_get_output(struct http_response *resp, int bufr);
extern struct	in_addr http_response_get_remote_ip(struct http_response *resp);
extern u_int16_t http_response_get_remote_port(struct http_response *resp);
extern void	http_response_guess_mime(const char *path, const char **ctype,
			const char **cencs, int maxenc);
extern void	http_response_send_redirect(struct http_response *resp,
			const char *url);
extern void	http_response_send_basic_auth(struct http_response *resp,
			const char *realm);
extern void	http_response_send_error(struct http_response *resp,
			int code, const char *fmt, ...);
extern void	http_response_send_errno_error(struct http_response *resp);

extern int	http_response_no_body(int code);
extern const	char *http_response_status_msg(int code);
extern int	http_response_get_raw_socket(struct http_response *resp);

__END_DECLS

#endif	/* _PDEL_HTTP_HTTP_SERVER_H_ */

