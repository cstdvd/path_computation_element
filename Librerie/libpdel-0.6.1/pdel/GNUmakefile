# Note: generated file

#
# Make sure default target is all
#
all:	realall

DOTDOT=
include GNUmakefile.inc

hack_h = $(shell if [ -f $(srcdir)/ports/$(OS)/hacks.h ]; then echo 1 ; else echo 0 ; fi )

ifeq (0, $(hack_h))
hack_h_arg= 
else
hack_h_arg= -include $(srcdir)/ports/$(OS)/hacks.h
endif

# User switches

DESTDIR =

libbase=	pdel
slibrary=	lib$(libbase).a 

# Port specific make bits

ifeq ($(OS),CYGWIN_NT-5.1)
WIN32_SHLIB=1
WIN32_GCC_SHLIB=1
DLLROOT=cyg
endif

ifeq ($(OS),MINGW32_NT-5.1)

WIN32_SHLIB=1
WIN32_GCC_SHLIB=1
DLLROOT=
EXTRALIBDEPS=	expat_implibs
EXTRALIBDEPS=	openssl_implibs

expat_implibs:	$(expat_implibs)

$(expat_implib): $(expat_libdir)/libexpat.dll 
	pexports $(expat_libdir)/libexpat.dll > $(objdir)/expat.def
	dlltool -d $(objdir)/expat.def -l $(objdir)/libexpat.a

$(expat_implibw): $(expat_libdir)/libexpatw.dll 
	pexports $(expat_libdir)/libexpatw.dll > $(objdir)/expatw.def
	dlltool -d $(objdir)/expatw.def -l $(objdir)/libexpatw.a

openssl_implibs:	$(openssl_implibs)

$(openssl_implib): $(openssl_libdir)/$(ssl_libbase).a
	cp $+ $@

$(crypt_implib): $(openssl_libdir)/$(crypto_libbase).a
	cp $+ $@

endif

ifeq ($(OS),MSVC-WIN32)

WIN32_SHLIB=1
WIN32_MSVC_SHLIB=1
DLLROOT=
#EXTRALIBDEPS=	expat_implibs

endif

ifdef WIN32_GCC_SHLIB

dependency_libargs= -L$(objdir) $(OTHERLIBDIRS) -L$(libdir) -l$(ssl_libbase) -l$(crypto_libbase)  -l$(expat_libbase) $(otherlibs)
implib=		lib${libbase}$(shlib_major)$(shlib_minor).dll.a

sharedobjargs= -shared
sharedlibargs= -shared \
    -Wl,--out-implib=$(objdir)/$(implib) \
    -Wl,--export-all-symbols \
    -Wl,--enable-auto-import \
    -Wl,--whole-archive  $(sobjs)  \
    -Wl,--no-whole-archive ${dependency_libargs}
endif

ifdef WIN32_MSVC_SHLIB

compsfx=VC

endif

ifdef WIN32_SHLIB
DO_SHARED=	1

picargs=	-DPDEL_SHARED=1
shlib_ext=	dll
dlibrary=	$(DLLROOT)$(libbase)$(compsfx)$(shlib_major)$(shlib_minor).$(shlib_ext)

else

DO_SHARED=	1

shlib_ext=	so.$(shlib_major).$(shlib_minor)
dlibrary=	lib$(libbase).$(shlib_ext)
sharedobjargs= -shared
sharedlibargs= -shared $(sobjs)

ifeq ($(arch),amd64)
picargs=	-fPIC
else
picargs=	-fpic
endif

endif

EXTRAINCS=	-I$(srcdir)/ports/$(OS) 
ifdef WIN32_PORT
EXTRAINCS+=	-I$(srcdir)/ports/WIN32
endif
EXTRAINCS+=	-I$(prefix)/ssl/include -I/usr/kerberos/include  ${OTHERINCDIRS}

#
# Below here should not need to be edited
#

SRCS=		app_config.c app_config_alog.c app_config_curconf.c app_config_directory.c app_config_pidfile.c http_client.c http_connection.c http_connection_cache.c http_head.c http_message.c http_mime.c http_request.c http_response.c http_server.c http_ssl.c http_status.c http_xml.c http_servlet_basicauth.c http_servlet_cookieauth.c http_servlet_file.c http_servlet_redirect.c http_servlet_tmpl.c http_servlet_xml.c http_servlet_xmlrpc.c base64.c boundary_fp.c count_fp.c filter.c misc_io.c poll.c socketpair.c ssl_fp.c string_fp.c timeout_fp.c string_misc.c sys_misc.c time_misc.c domain_server.c pd_inet_ntop.c pd_inet_pton.c tcp_server.c structs.c structs_generic.c structs_xml.c structs_xmlrpc.c structs_type_array.c structs_type_boolean.c structs_type_data.c structs_type_dnsname.c structs_type_ether.c structs_type_float.c structs_type_id.c structs_type_int.c structs_type_ip4.c structs_type_ip6.c structs_type_null.c structs_type_pointer.c structs_type_regex.c structs_type_string.c structs_type_struct.c structs_type_time.c structs_type_union.c alog.c logfile.c pd_mmap.c thread.c tmpl.c tmpl_ctx.c tmpl_exec.c tmpl_mem.c tmpl_parse.c tmpl_vars.c ghash.c gtree.c mesg_port.c paction.c pevent.c rsa_util.c string_quote.c tinfo.c typed_mem.c
SRCDIRS=	$(srcdir)/config $(srcdir)/http $(srcdir)/http/servlet $(srcdir)/io $(srcdir)/misc $(srcdir)/net $(srcdir)/pdel $(srcdir)/ppp $(srcdir)/structs $(srcdir)/structs/type $(srcdir)/sys $(srcdir)/thread $(srcdir)/tmpl $(srcdir)/util
INCS=		version.h config/app_config.h http/http_defs.h http/http_server.h http/http_servlet.h http/xml.h http/servlet/basicauth.h http/servlet/cookieauth.h http/servlet/file.h http/servlet/redirect.h http/servlet/tmpl.h http/servlet/xml.h http/servlet/xmlrpc.h io/base64.h io/boundary_fp.h io/count_fp.h io/filter.h io/ssl_fp.h io/string_fp.h io/timeout_fp.h net/domain_server.h net/tcp_server.h structs/structs.h structs/types.h structs/xml.h structs/xmlrpc.h structs/type/array.h structs/type/array_define.h structs/type/boolean.h structs/type/data.h structs/type/dnsname.h structs/type/ether.h structs/type/float.h structs/type/id.h structs/type/int.h structs/type/ip4.h structs/type/ip6.h structs/type/null.h structs/type/pointer.h structs/type/regex.h structs/type/string.h structs/type/struct.h structs/type/time.h structs/type/union.h sys/alog.h sys/logfile.h tmpl/tmpl.h util/ghash.h util/gtree.h util/mesg_port.h util/paction.h util/pevent.h util/rsa_util.h util/string_quote.h util/tinfo.h util/typed_mem.h
BASEINCS=	pd_base.h pd_io.h pd_mem.h pd_inet.h pd_poll.h pd_port.h pd_regex.h pd_string.h pd_sys.h pd_syslog.h pd_thread.h pd_time.h
MAN=		libpdel.3 config/app_config.3 http/http_server.3 http/http_client.3 http/http_request.3 http/http_mime.3 http/http_response.3 http/http_servlet.3 http/http_xml.3 http/servlet/http_servlet_basicauth.3 http/servlet/http_servlet_cookieauth.3 http/servlet/http_servlet_file.3 http/servlet/http_servlet_redirect.3 http/servlet/http_servlet_tmpl.3 http/servlet/http_servlet_xml.3 http/servlet/http_servlet_xmlrpc.3 io/base64.3 io/boundary_fopen.3 io/count_fopen.3 io/timeout_fp.3 io/filter.3 io/string_fp.3 io/ssl_fp.3 net/domain_server.3 net/tcp_server.3 structs/structs.3 structs/structs_type.3 structs/structs_xml_input.3 structs/structs_xmlrpc.3 structs/type/structs_type_array.3 structs/type/structs_type_boolean.3 structs/type/structs_type_data.3 structs/type/structs_type_dnsname.3 structs/type/structs_type_ether.3 structs/type/structs_type_float.3 structs/type/structs_type_id.3 structs/type/structs_type_int.3 structs/type/structs_type_ip4.3 structs/type/structs_type_ip6.3 structs/type/structs_type_null.3 structs/type/structs_type_pointer.3 structs/type/structs_type_regex.3 structs/type/structs_type_string.3 structs/type/structs_type_struct.3 structs/type/structs_type_time.3 structs/type/structs_type_union.3 sys/alog.3 sys/logfile.3 tmpl/tmpl.3 util/ghash.3 util/gtree.3 util/mesg_port.3 util/paction.3 util/pevent.3 util/rsa_util_sign.3 util/string_enquote.3 util/tinfo.3 util/typed_mem.3
MLINKS=		app_config.3 app_config_alog_subsystem.3 app_config.3 app_config_copy.3 app_config.3 app_config_curconf_subsystem.3 app_config.3 app_config_directory_subsystem.3 app_config.3 app_config_free.3 app_config.3 app_config_get.3 app_config.3 app_config_get_type.3 app_config.3 app_config_init.3 app_config.3 app_config_load.3 app_config.3 app_config_new.3 app_config.3 app_config_pidfile_subsystem.3 app_config.3 app_config_reload.3 app_config.3 app_config_set.3 app_config.3 app_config_shutting_down.3 app_config.3 app_config_uninit.3 http_server.3 http_server_start.3 http_server.3 http_server_stop.3 http_server.3 http_server_register_servlet.3 http_server.3 http_server_destroy_servlet.3 http_server.3 http_server_set_proxy_handler.3 http_client.3 http_client_close.3 http_client.3 http_client_connect.3 http_client.3 http_client_create.3 http_client.3 http_client_destroy.3 http_client.3 http_client_get_local_ip.3 http_client.3 http_client_get_local_port.3 http_client.3 http_client_get_reason.3 http_client.3 http_client_get_request.3 http_client.3 http_client_get_response.3 http_request.3 http_request_encode_basic_auth.3 http_request.3 http_request_file_upload.3 http_request.3 http_request_get_header.3 http_request.3 http_request_get_header_by_index.3 http_request.3 http_request_get_host.3 http_request.3 http_request_get_input.3 http_request.3 http_request_get_method.3 http_request.3 http_request_get_mime_multiparts.3 http_request.3 http_request_get_output.3 http_request.3 http_request_get_password.3 http_request.3 http_request_get_path.3 http_request.3 http_request_get_query_string.3 http_request.3 http_request_get_raw_socket.3 http_request.3 http_request_get_remote_ip.3 http_request.3 http_request_get_remote_port.3 http_request.3 http_request_get_ssl.3 http_request.3 http_request_get_uri.3 http_request.3 http_request_get_username.3 http_request.3 http_request_get_value.3 http_request.3 http_request_get_version.3 http_request.3 http_request_parse_time.3 http_request.3 http_request_read_mime_multipart.3 http_request.3 http_request_read_url_encoded_values.3 http_request.3 http_request_remove_header.3 http_request.3 http_request_send_headers.3 http_request.3 http_request_set_header.3 http_request.3 http_request_set_method.3 http_request.3 http_request_set_path.3 http_request.3 http_request_set_proxy.3 http_request.3 http_request_set_query_from_values.3 http_request.3 http_request_set_value.3 http_request.3 http_request_url_decode.3 http_request.3 http_request_url_encode.3 http_request.3 http_request_write_url_encoded_values.3 http_mime.3 http_mime_multipart_free.3 http_mime.3 http_mime_multipart_get_count.3 http_mime.3 http_mime_multipart_get_part.3 http_mime.3 http_mime_part_get_data.3 http_mime.3 http_mime_part_get_header.3 http_mime.3 http_mime_part_get_length.3 http_response.3 http_response_get_code.3 http_response.3 http_response_get_header.3 http_response.3 http_response_get_header_by_index.3 http_response.3 http_response_get_input.3 http_response.3 http_response_get_output.3 http_response.3 http_response_get_raw_socket.3 http_response.3 http_response_get_remote_ip.3 http_response.3 http_response_get_remote_port.3 http_response.3 http_response_get_ssl.3 http_response.3 http_response_guess_mime.3 http_response.3 http_response_no_body.3 http_response.3 http_response_num_headers.3 http_response.3 http_response_remove_header.3 http_response.3 http_response_send_basic_auth.3 http_response.3 http_response_send_error.3 http_response.3 http_response_send_headers.3 http_response.3 http_response_send_redirect.3 http_response.3 http_response_set_header.3 http_response.3 http_response_status_msg.3 http_xml.3 http_xml_send.3 http_xml.3 http_xml_send_xmlrpc.3 http_servlet_basicauth.3 http_servlet_basicauth_create.3 http_servlet_cookieauth.3 http_servlet_cookieauth_create.3 http_servlet_cookieauth.3 http_servlet_cookieauth_login.3 http_servlet_cookieauth.3 http_servlet_cookieauth_logout.3 http_servlet_cookieauth.3 http_servlet_cookieauth_user.3 http_servlet_file.3 http_servlet_file_create.3 http_servlet_file.3 http_servlet_file_serve.3 http_servlet_redirect.3 http_servlet_redirect_create.3 http_servlet_tmpl.3 http_servlet_tmpl_create.3 http_servlet_tmpl.3 http_servlet_tmpl_func_query.3 http_servlet_tmpl.3 http_servlet_tmpl_func_query_exists.3 http_servlet_tmpl.3 http_servlet_tmpl_func_query_string.3 http_servlet_tmpl.3 http_servlet_tmpl_func_get_header.3 http_servlet_tmpl.3 http_servlet_tmpl_func_set_header.3 http_servlet_tmpl.3 http_servlet_tmpl_func_remove_header.3 http_servlet_tmpl.3 http_servlet_tmpl_func_redirect.3 http_servlet_tmpl.3 http_servlet_tmpl_func_unbuffer.3 http_servlet_xml.3 http_servlet_xml_create.3 http_servlet_xmlrpc.3 http_servlet_xmlrpc_create.3 base64.3 b64_encoder_create.3 base64.3 b64_decoder_create.3 base64.3 b64_rfc2045_charset.3 timeout_fp.3 timeout_fdopen.3 filter.3 filter_read.3 filter.3 filter_write.3 filter.3 filter_end.3 filter.3 filter_convert.3 filter.3 filter_destroy.3 filter.3 filter_fopen.3 filter.3 filter_process.3 string_fp.3 string_buf_input.3 string_fp.3 string_buf_output.3 string_fp.3 string_buf_content.3 string_fp.3 string_buf_length.3 ssl_fp.3 ssl_fdopen.3 ssl_fp.3 ssl_log.3 domain_server.3 domain_connection_get_cookie.3 domain_server.3 domain_connection_get_fd.3 domain_server.3 domain_connection_get_fp.3 domain_server.3 domain_connection_get_peer.3 domain_server.3 domain_connection_get_server.3 domain_server.3 domain_server_get_cookie.3 domain_server.3 domain_server_start.3 domain_server.3 domain_server_stop.3 tcp_server.3 tcp_connection_get_cookie.3 tcp_server.3 tcp_connection_get_fd.3 tcp_server.3 tcp_connection_get_fp.3 tcp_server.3 tcp_connection_get_peer.3 tcp_server.3 tcp_connection_get_server.3 tcp_server.3 tcp_server_get_cookie.3 tcp_server.3 tcp_server_start.3 tcp_server.3 tcp_server_stop.3 structs.3 structs_equal.3 structs.3 structs_find.3 structs.3 structs_free.3 structs.3 structs_get.3 structs.3 structs_get_binary.3 structs.3 structs_get_string.3 structs.3 structs_init.3 structs.3 structs_reset.3 structs.3 structs_set.3 structs.3 structs_set_binary.3 structs.3 structs_set_string.3 structs.3 structs_traverse.3 structs_type.3 structs_ascii_copy.3 structs_type.3 structs_nothing_free.3 structs_type.3 structs_notsupp_ascify.3 structs_type.3 structs_notsupp_binify.3 structs_type.3 structs_notsupp_copy.3 structs_type.3 structs_notsupp_decode.3 structs_type.3 structs_notsupp_encode.3 structs_type.3 structs_notsupp_equal.3 structs_type.3 structs_notsupp_init.3 structs_type.3 structs_region_copy.3 structs_type.3 structs_region_decode.3 structs_type.3 structs_region_decode_netorder.3 structs_type.3 structs_region_encode.3 structs_type.3 structs_region_encode_netorder.3 structs_type.3 structs_region_equal.3 structs_type.3 structs_region_init.3 structs_xml_input.3 structs_xml_output.3 structs_xmlrpc.3 structs_struct2xmlrpc.3 structs_xmlrpc.3 structs_type_xmlrpc_array.3 structs_xmlrpc.3 structs_type_xmlrpc_compact_fault.3 structs_xmlrpc.3 structs_type_xmlrpc_fault.3 structs_xmlrpc.3 structs_type_xmlrpc_member.3 structs_xmlrpc.3 structs_type_xmlrpc_request.3 structs_xmlrpc.3 structs_type_xmlrpc_response.3 structs_xmlrpc.3 structs_type_xmlrpc_struct.3 structs_xmlrpc.3 structs_type_xmlrpc_value.3 structs_xmlrpc.3 structs_xmlrpc2struct.3 structs_xmlrpc.3 structs_xmlrpc_build_fault_response.3 structs_xmlrpc.3 structs_xmlrpc_build_request.3 structs_xmlrpc.3 structs_xmlrpc_build_response.3 structs_type_array.3 DEFINE_STRUCTS_ARRAY.3 structs_type_array.3 STRUCTS_ARRAY_TYPE.3 structs_type_array.3 STRUCTS_FIXEDARRAY_TYPE.3 structs_type_array.3 structs_array_delete.3 structs_type_array.3 structs_array_insert.3 structs_type_array.3 structs_array_length.3 structs_type_array.3 structs_array_prep.3 structs_type_array.3 structs_array_reset.3 structs_type_array.3 structs_type_fixedarray.3 structs_type_boolean.3 structs_type_boolean_char.3 structs_type_boolean.3 structs_type_boolean_int.3 structs_type_boolean.3 structs_type_boolean_char_01.3 structs_type_boolean.3 structs_type_boolean_int_01.3 structs_type_data.3 STRUCTS_DATA_TYPE.3 structs_type_data.3 STRUCTS_FIXEDDATA_TYPE.3 structs_type_data.3 structs_type_hexdata.3 structs_type_ether.3 structs_type_ether_nocolon.3 structs_type_float.3 structs_type_double.3 structs_type_id.3 STRUCTS_ID_TYPE.3 structs_type_int.3 structs_type_char.3 structs_type_int.3 structs_type_uchar.3 structs_type_int.3 structs_type_hchar.3 structs_type_int.3 structs_type_short.3 structs_type_int.3 structs_type_ushort.3 structs_type_int.3 structs_type_hshort.3 structs_type_int.3 structs_type_uint.3 structs_type_int.3 structs_type_hint.3 structs_type_int.3 structs_type_long.3 structs_type_int.3 structs_type_ulong.3 structs_type_int.3 structs_type_hlong.3 structs_type_int.3 structs_type_int8.3 structs_type_int.3 structs_type_uint8.3 structs_type_int.3 structs_type_hint8.3 structs_type_int.3 structs_type_int16.3 structs_type_int.3 structs_type_uint16.3 structs_type_int.3 structs_type_hint16.3 structs_type_int.3 structs_type_int32.3 structs_type_int.3 structs_type_uint32.3 structs_type_int.3 structs_type_hint32.3 structs_type_int.3 structs_type_int64.3 structs_type_int.3 structs_type_uint64.3 structs_type_int.3 structs_type_hint64.3 structs_type_pointer.3 STRUCTS_POINTER_TYPE.3 structs_type_regex.3 STRUCTS_REGEX_TYPE.3 structs_type_regex.3 structs_type_regex_icase.3 structs_type_string.3 STRUCTS_FIXEDSTRING_TYPE.3 structs_type_string.3 STRUCTS_STRING_TYPE.3 structs_type_string.3 structs_type_string_null.3 structs_type_struct.3 STRUCTS_STRUCT_FIELD.3 structs_type_struct.3 STRUCTS_STRUCT_FIELD2.3 structs_type_struct.3 STRUCTS_STRUCT_TYPE.3 structs_type_time.3 structs_type_time_abs.3 structs_type_time.3 structs_type_time_gmt.3 structs_type_time.3 structs_type_time_iso8601.3 structs_type_time.3 structs_type_time_local.3 structs_type_time.3 structs_type_time_rel.3 structs_type_union.3 DEFINE_STRUCTS_UNION.3 structs_type_union.3 STRUCTS_UNION_FIELD.3 structs_type_union.3 STRUCTS_UNION_TYPE.3 structs_type_union.3 structs_union_set.3 alog.3 alog_clear_history.3 alog.3 alog_configure.3 alog.3 alog_expand.3 alog.3 alog_facility.3 alog.3 alog_facility_name.3 alog.3 alog_get_history.3 alog.3 alog_set_channel.3 alog.3 alog_set_debug.3 alog.3 alog_severity.3 alog.3 alog_severity_name.3 alog.3 alog_shutdown.3 alog.3 valog.3 logfile.3 logfile_close.3 logfile.3 logfile_get.3 logfile.3 logfile_num_entries.3 logfile.3 logfile_open.3 logfile.3 logfile_put.3 logfile.3 logfile_sync.3 logfile.3 logfile_trim.3 base64.3 b64_encoder_create.3 tmpl.3 tmpl_create.3 tmpl.3 tmpl_ctx_create.3 tmpl.3 tmpl_ctx_destroy.3 tmpl.3 tmpl_ctx_reset.3 tmpl.3 tmpl_destroy.3 tmpl.3 tmpl_execute.3 tmpl.3 tmpl_execute_func.3 ghash.3 ghash_arg.3 ghash.3 ghash_create.3 ghash.3 ghash_destroy.3 ghash.3 ghash_dump.3 ghash.3 ghash_get.3 ghash.3 ghash_iter_create.3 ghash.3 ghash_iter_destroy.3 ghash.3 ghash_iter_has_next.3 ghash.3 ghash_iter_next.3 ghash.3 ghash_iter_remove.3 ghash.3 ghash_put.3 ghash.3 ghash_remove.3 ghash.3 ghash_size.3 ghash.3 ghash_walk_init.3 ghash.3 ghash_walk_next.3 gtree.3 gtree_arg.3 gtree.3 gtree_create.3 gtree.3 gtree_destroy.3 gtree.3 gtree_dump.3 gtree.3 gtree_first.3 gtree.3 gtree_get.3 gtree.3 gtree_last.3 gtree.3 gtree_next.3 gtree.3 gtree_prev.3 gtree.3 gtree_print.3 gtree.3 gtree_put.3 gtree.3 gtree_remove.3 gtree.3 gtree_size.3 gtree.3 gtree_traverse.3 mesg_port.3 mesg_port_create.3 mesg_port.3 mesg_port_destroy.3 mesg_port.3 mesg_port_get.3 mesg_port.3 mesg_port_put.3 mesg_port.3 mesg_port_qlen.3 paction.3 paction_start.3 paction.3 paction_cancel.3 pevent.3 pevent_ctx_create.3 pevent.3 pevent_ctx_destroy.3 pevent.3 pevent_ctx_count.3 pevent.3 pevent_register.3 pevent.3 pevent_unregister.3 rsa_util_sign.3 rsa_util_verify.3 rsa_util_sign.3 rsa_util_verify_priv.3 string_enquote.3 string_dequote.3 tinfo.3 TINFO_INIT.3 tinfo.3 tinfo_get.3 tinfo.3 tinfo_set.3 tinfo.3 tinfo_set_nocopy.3 typed_mem.3 ASPRINTF.3 typed_mem.3 CALLOC.3 typed_mem.3 FREE.3 typed_mem.3 MALLOC.3 typed_mem.3 REALLOC.3 typed_mem.3 REALLOCF.3 typed_mem.3 STRDUP.3 typed_mem.3 VASPRINTF.3 typed_mem.3 typed_mem_dump.3 typed_mem.3 typed_mem_enable.3 typed_mem.3 typed_mem_usage.3
CFLAGS=		-O2 -fno-strict-aliasing -pipe -DPOSIX_MISTAKE -I$(srcdir) -I/usr/local/include -DTYPED_MEM_UNDEFINE_ORIGINALS -DBUILDING_PDEL=1 -O2 -Wall -Wcast-align -Wchar-subscripts -Wcomment -Wformat -Wimplicit -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wno-long-long -Wparentheses -Wpointer-arith -Wreturn-type -Wswitch -Wtrigraphs -Wuninitialized -Wunused -Wwrite-strings -g
SUBDIRS=	

# Functional add-ons

ifdef MISSING_FUNOPEN

include stdio/Makefile.inc
SRCS+=		$(STDIO_SRCS)
SRCDIRS+=	$(STDIO_SRCDIR)
CFLAGS+=	-I$(STDIO_SRCDIR) -DMISSING_FUNOPEN=1

endif

ifdef NEED_REGEX

include regex/Makefile.inc

SRCS+=		$(REGEX_SRCS)
SRCDIRS+=	$(REGEX_SRCDIR)

endif

# Compile Rules

VPATH=		$(SRCDIRS) $(PORTSRCDIR)

objs=		$(SRCS:%.c=$(objdir)/%.o)
sobjs=		$(SRCS:%.c=$(objdir)/%.So)

cman=		$(foreach m,$(MAN),$(m).gz)

# Special .o targets
structs_xmlrpc.o \
structs_type_array.o \
structs_type_data.o : CFLAGS := ${CFLAGS} -fno-strict-aliasing 

ifdef DO_SHARED
libraries=	$(objdir)/$(slibrary) $(objdir)/$(dlibrary)
else
libraries=	$(objdir)/$(slibrary)
endif

realall:		objdir $(EXTRALIBDEPS) $(libraries) $(cman)

objdir:		$(objdir)

$(objdir):
	mkdir -p $(objdir)

vpath %.o $(objdir)
vpath %.So $(objdir)
vpath %.$(shlib_ext) $(objdir)

%.gz:		%
	gzip < $+ > $@

$(objdir)/%.o:		%.c
	$(cc) -c $(OBJOUT) $(CFLAGS) $(DFLAGS) $(hack_h_arg) -I$(srcdir) $(EXTRAINCS) $+ $(PPOUT)

$(objdir)/%.So:		%.c
	$(cc) -c $(sharedobjargs) $(picargs) $(OBJOUT) $(CFLAGS) $(DFLAGS) $(hack_h_arg) -I$(srcdir) $(EXTRAINCS) $+ $(PPOUT)

$(objdir)/$(slibrary):	$(objs) # $(shared_extra)
	$(ar) rv $@ $(objs) $(static_extra_objs)
	$(ranlib) $@

$(objdir)/$(dlibrary):	$(sobjs) $(libs)  $(shared_extra)
	$(cc) -o $@ $(sharedlibargs)

clean:
	rm -f $(objs) $(library) $(cman)
	rm -rf $(objdir)

install:	install-libraries install-header-dirs install-headers install-man

uninstall:	uninstall-libraries uninstall-headers uninstall-man

ifdef DO_SHARED
IN_SHARED=	$(install) -c $(install_lib_own) -m 555 \
	    $(objdir)/$(dlibrary) $(DESTDIR)$(slibdir)/$(dlibrary)
ifdef WIN32_SHLIB
IN_SHARED2=	$(install) -c $(install_lib_own) -m 755 \
	    $(objdir)/$(implib) $(DESTDIR)$(libdir)/$(implib)
endif
endif

install-libraries:	$(libraries)
	$(install) -c $(install_lib_own) -m 444 \
	    $(objdir)/$(slibrary) $(DESTDIR)$(libdir)/$(slibrary)
	$(IN_SHARED)
	$(IN_SHARED2)

ifdef DO_SHARED
UNIN_SHARED=	rm -f $(DESTDIR)$(slibdir)/$(dlibrary)
endif
uninstall-libraries:
	rm -f $(DESTDIR)$(libdir)/$(slibrary)
	$(UNIN_SHARED)

install-header-dirs:
	@incdirs=`echo $(INCS) | awk '{for(i=1;i<=NF;i++)print $$i}' \
	    | grep / | sed 's,^\(.*\)/[^/][^/]*$$,\1,g' | sort -u`; \
	for dir in $$incdirs; do \
		if ! [ -d $(DESTDIR)$(includedir)$$dir ]; then \
			echo $(install) -d $(install_include_own) \
			    -m 0755 $(DESTDIR)$(includedir)/$$dir; \
			$(install) -d $(install_include_own) \
			    -m 0755 $(DESTDIR)$(includedir)/$$dir; \
		fi; \
	done

install-headers:
	@for hdr in $(INCS); do \
		echo $(install) -c $(install_include_own) -m 444 \
		    $$hdr $(DESTDIR)$(includedir)/$$hdr; \
		$(install) -c $(install_include_own) -m 444 \
		    $$hdr $(DESTDIR)$(includedir)/$$hdr; \
	done
	@for hdr in $(BASEINCS); do \
		echo $(install) -c $(install_include_own) -m 444 \
		    pdel/$$hdr $(DESTDIR)$(includedir)/$$hdr; \
		$(install) -c $(install_include_own) -m 444 \
		    pdel/$$hdr $(DESTDIR)$(includedir)/$$hdr; \
	done

uninstall-headers:
	@for hdr in $(INCS); do \
		echo rm -f $(DESTDIR)$(includedir)/$$hdr; \
		rm -f $(DESTDIR)$(includedir)/$$hdr; \
	done
	@for hdr in $(BASEINCS); do \
		echo rm -f $(DESTDIR)$(includedir)/pdel/$$hdr; \
		rm -f $(DESTDIR)$(includedir)/pdel/$$hdr; \
	done

install-man:	install-manpages install-manlinks

uninstall-man:	uninstall-manpages uninstall-manlinks

install-manpages:	$(cman)
	@for p in $(MAN); do \
		section=`echo $$p | sed 's/^[^.]*.//g'`; \
		f=`basename $$p`; \
		if [ ! -d $(DESTDIR)$(mandir)/man$$section ]; then \
			echo $(install) -d $(install_man_own) -m 755 \
			    $(DESTDIR)$(mandir)/man$$section; \
			$(install) -d $(install_man_own) -m 755 \
			    $(DESTDIR)$(mandir)/man$$section; \
		fi; \
		echo $(install) -c $(install_man_own) -m 444 \
		    $$p.gz $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
		$(install) -c $(install_man_own) -m 444 \
		    $$p.gz $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
	done

uninstall-manpages:
	@for p in $(MAN); do \
		section=`echo $$p | sed 's/^[^.]*.//g'`; \
		f=`basename $$p`; \
		echo rm -f $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
		rm -f $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
	done

install-manlinks:
	@set `echo $(MLINKS) " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
	while : ; do \
		case $$# in \
			0) break;; \
			[123]) echo "warn: empty MLINK: $$1 $$2 $$3"; break;; \
		esac; \
		name=$$1; shift; sect=$$1; shift; \
		l=$(DESTDIR)$(mandir)/man$$sect/$$name; \
		name=$$1; shift; sect=$$1; shift; \
		t=$(DESTDIR)$(mandir)/man$$sect/$$name; \
		echo $$t.gz -\> $$l.gz; \
		rm -f $$t $$t.gz; \
		ln $$l.gz $$t.gz; \
	done

uninstall-manlinks:
	@set `echo $(MLINKS) " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
	while : ; do \
		case $$# in \
			0) break;; \
			[123]) echo "warn: empty MLINK: $$1 $$2 $$3"; break;; \
		esac; \
		name=$$1; shift; sect=$$1; shift; \
		l=$(DESTDIR)$(mandir)/man$$sect/$$name; \
		name=$$1; shift; sect=$$1; shift; \
		t=$(DESTDIR)$(mandir)/man$$sect/$$name; \
		echo rm -f $$t.gz; \
		rm -f $$t.gz; \
	done

