# $Id: Makefile.inc 462 2002-08-07 00:29:07Z archie $

# Base HTTP code

http_PATH+=	${PDELTOP}/http
http_SRCS+=	http_client.c \
		http_connection.c \
		http_connection_cache.c \
		http_head.c \
		http_message.c \
		http_mime.c \
		http_request.c \
		http_response.c \
		http_server.c \
		http_ssl.c \
		http_status.c \
		http_xml.c

http_INCS+=	http_defs.h \
		http_server.h \
		http_servlet.h \
		xml.h

http_MAN+=	http_server.3
http_MLINKS+=	http_server.3 http_server_start.3
http_MLINKS+=	http_server.3 http_server_stop.3
http_MLINKS+=	http_server.3 http_server_register_servlet.3
http_MLINKS+=	http_server.3 http_server_destroy_servlet.3
http_MLINKS+=	http_server.3 http_server_set_proxy_handler.3

http_MAN+=	http_client.3
http_MLINKS+=	http_client.3 http_client_close.3
http_MLINKS+=	http_client.3 http_client_connect.3
http_MLINKS+=	http_client.3 http_client_create.3
http_MLINKS+=	http_client.3 http_client_destroy.3
http_MLINKS+=	http_client.3 http_client_get_local_ip.3
http_MLINKS+=	http_client.3 http_client_get_local_port.3
http_MLINKS+=	http_client.3 http_client_get_reason.3
http_MLINKS+=	http_client.3 http_client_get_request.3
http_MLINKS+=	http_client.3 http_client_get_response.3

http_MAN+=	http_request.3
http_MLINKS+=	http_request.3 http_request_encode_basic_auth.3
http_MLINKS+=	http_request.3 http_request_file_upload.3
http_MLINKS+=	http_request.3 http_request_get_header.3
http_MLINKS+=	http_request.3 http_request_get_header_by_index.3
http_MLINKS+=	http_request.3 http_request_get_host.3
http_MLINKS+=	http_request.3 http_request_get_input.3
http_MLINKS+=	http_request.3 http_request_get_method.3
http_MLINKS+=	http_request.3 http_request_get_mime_multiparts.3
http_MLINKS+=	http_request.3 http_request_get_output.3
http_MLINKS+=	http_request.3 http_request_get_password.3
http_MLINKS+=	http_request.3 http_request_get_path.3
http_MLINKS+=	http_request.3 http_request_get_query_string.3
http_MLINKS+=	http_request.3 http_request_get_raw_socket.3
http_MLINKS+=	http_request.3 http_request_get_remote_ip.3
http_MLINKS+=	http_request.3 http_request_get_remote_port.3
http_MLINKS+=	http_request.3 http_request_get_ssl.3
http_MLINKS+=	http_request.3 http_request_get_uri.3
http_MLINKS+=	http_request.3 http_request_get_username.3
http_MLINKS+=	http_request.3 http_request_get_value.3
http_MLINKS+=	http_request.3 http_request_get_version.3
http_MLINKS+=	http_request.3 http_request_parse_time.3
http_MLINKS+=	http_request.3 http_request_read_mime_multipart.3
http_MLINKS+=	http_request.3 http_request_read_url_encoded_values.3
http_MLINKS+=	http_request.3 http_request_remove_header.3
http_MLINKS+=	http_request.3 http_request_send_headers.3
http_MLINKS+=	http_request.3 http_request_set_header.3
http_MLINKS+=	http_request.3 http_request_set_method.3
http_MLINKS+=	http_request.3 http_request_set_path.3
http_MLINKS+=	http_request.3 http_request_set_proxy.3
http_MLINKS+=	http_request.3 http_request_set_query_from_values.3
http_MLINKS+=	http_request.3 http_request_set_value.3
http_MLINKS+=	http_request.3 http_request_url_decode.3
http_MLINKS+=	http_request.3 http_request_url_encode.3
http_MLINKS+=	http_request.3 http_request_write_url_encoded_values.3

http_MAN+=	http_mime.3
http_MLINKS+=	http_mime.3 http_mime_multipart_free.3
http_MLINKS+=	http_mime.3 http_mime_multipart_get_count.3
http_MLINKS+=	http_mime.3 http_mime_multipart_get_part.3
http_MLINKS+=	http_mime.3 http_mime_part_get_data.3
http_MLINKS+=	http_mime.3 http_mime_part_get_header.3
http_MLINKS+=	http_mime.3 http_mime_part_get_length.3

http_MAN+=	http_response.3
http_MLINKS+=	http_response.3 http_response_get_code.3
http_MLINKS+=	http_response.3 http_response_get_header.3
http_MLINKS+=	http_response.3 http_response_get_header_by_index.3
http_MLINKS+=	http_response.3 http_response_get_input.3
http_MLINKS+=	http_response.3 http_response_get_output.3
http_MLINKS+=	http_response.3 http_response_get_raw_socket.3
http_MLINKS+=	http_response.3 http_response_get_remote_ip.3
http_MLINKS+=	http_response.3 http_response_get_remote_port.3
http_MLINKS+=	http_response.3 http_response_get_ssl.3
http_MLINKS+=	http_response.3 http_response_guess_mime.3
http_MLINKS+=	http_response.3 http_response_no_body.3
http_MLINKS+=	http_response.3 http_response_num_headers.3
http_MLINKS+=	http_response.3 http_response_remove_header.3
http_MLINKS+=	http_response.3 http_response_send_basic_auth.3
http_MLINKS+=	http_response.3 http_response_send_error.3
http_MLINKS+=	http_response.3 http_response_send_headers.3
http_MLINKS+=	http_response.3 http_response_send_redirect.3
http_MLINKS+=	http_response.3 http_response_set_header.3
http_MLINKS+=	http_response.3 http_response_status_msg.3

http_MAN+=	http_servlet.3

http_MAN+=	http_xml.3
http_MLINKS+=	http_xml.3 http_xml_send.3
http_MLINKS+=	http_xml.3 http_xml_send_xmlrpc.3

# Predefined servlets

http_PATH+=	${PDELTOP}/http/servlet

http_SRCS+=	http_servlet_basicauth.c \
		http_servlet_cookieauth.c \
		http_servlet_file.c \
		http_servlet_redirect.c \
		http_servlet_tmpl.c \
		http_servlet_xml.c \
		http_servlet_xmlrpc.c

http_INCS+=	servlet/basicauth.h \
		servlet/cookieauth.h \
		servlet/file.h \
		servlet/redirect.h \
		servlet/tmpl.h \
		servlet/xml.h \
		servlet/xmlrpc.h

http_MAN+=	servlet/http_servlet_basicauth.3
http_MLINKS+=	http_servlet_basicauth.3 http_servlet_basicauth_create.3

http_MAN+=	servlet/http_servlet_cookieauth.3
http_MLINKS+=	http_servlet_cookieauth.3 http_servlet_cookieauth_create.3
http_MLINKS+=	http_servlet_cookieauth.3 http_servlet_cookieauth_login.3
http_MLINKS+=	http_servlet_cookieauth.3 http_servlet_cookieauth_logout.3
http_MLINKS+=	http_servlet_cookieauth.3 http_servlet_cookieauth_user.3

http_MAN+=	servlet/http_servlet_file.3
http_MLINKS+=	http_servlet_file.3 http_servlet_file_create.3
http_MLINKS+=	http_servlet_file.3 http_servlet_file_serve.3

http_MAN+=	servlet/http_servlet_redirect.3
http_MLINKS+=	http_servlet_redirect.3 http_servlet_redirect_create.3

http_MAN+=	servlet/http_servlet_tmpl.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_create.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_query.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_query_exists.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_query_string.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_get_header.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_set_header.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_remove_header.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_redirect.3
http_MLINKS+=	http_servlet_tmpl.3 http_servlet_tmpl_func_unbuffer.3

http_MAN+=	servlet/http_servlet_xml.3
http_MLINKS+=	http_servlet_xml.3 http_servlet_xml_create.3

http_MAN+=	servlet/http_servlet_xmlrpc.3
http_MLINKS+=	http_servlet_xmlrpc.3 http_servlet_xmlrpc_create.3

