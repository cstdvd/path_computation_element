# $Id: Makefile.inc 1021 2006-12-07 23:01:46Z mgooderum $

# Base structs code

structs_PATH+=	${PDELTOP}/structs

structs_SRCS+=	structs.c \
		structs_generic.c \
		structs_xml.c \
		structs_xmlrpc.c

structs_INCS+=	structs.h \
		types.h \
		xml.h \
		xmlrpc.h

structs_MAN+=	structs.3
structs_MLINKS+=structs.3 structs_equal.3 \
		structs.3 structs_find.3 \
		structs.3 structs_free.3 \
		structs.3 structs_get.3 \
		structs.3 structs_get_binary.3 \
		structs.3 structs_get_string.3 \
		structs.3 structs_init.3 \
		structs.3 structs_reset.3 \
		structs.3 structs_set.3 \
		structs.3 structs_set_binary.3 \
		structs.3 structs_set_string.3 \
		structs.3 structs_traverse.3

structs_MAN+=	structs_type.3
structs_MLINKS+=structs_type.3 structs_ascii_copy.3 \
		structs_type.3 structs_nothing_free.3 \
		structs_type.3 structs_notsupp_ascify.3 \
		structs_type.3 structs_notsupp_binify.3 \
		structs_type.3 structs_notsupp_copy.3 \
		structs_type.3 structs_notsupp_decode.3 \
		structs_type.3 structs_notsupp_encode.3 \
		structs_type.3 structs_notsupp_equal.3 \
		structs_type.3 structs_notsupp_init.3 \
		structs_type.3 structs_region_copy.3 \
		structs_type.3 structs_region_decode.3 \
		structs_type.3 structs_region_decode_netorder.3 \
		structs_type.3 structs_region_encode.3 \
		structs_type.3 structs_region_encode_netorder.3 \
		structs_type.3 structs_region_equal.3 \
		structs_type.3 structs_region_init.3

structs_MAN+=	structs_xml_input.3
structs_MLINKS+=structs_xml_input.3 structs_xml_output.3

structs_MAN+=	structs_xmlrpc.3
structs_MLINKS+=structs_xmlrpc.3 structs_struct2xmlrpc.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_array.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_compact_fault.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_fault.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_member.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_request.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_response.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_struct.3
structs_MLINKS+=structs_xmlrpc.3 structs_type_xmlrpc_value.3
structs_MLINKS+=structs_xmlrpc.3 structs_xmlrpc2struct.3
structs_MLINKS+=structs_xmlrpc.3 structs_xmlrpc_build_fault_response.3
structs_MLINKS+=structs_xmlrpc.3 structs_xmlrpc_build_request.3
structs_MLINKS+=structs_xmlrpc.3 structs_xmlrpc_build_response.3

# Predefined structs types

structs_PATH+=	${PDELTOP}/structs/type

structs_SRCS+=	structs_type_array.c \
		structs_type_bpf.c \
		structs_type_boolean.c \
		structs_type_data.c \
		structs_type_dnsname.c \
		structs_type_ether.c \
		structs_type_float.c \
		structs_type_id.c \
		structs_type_int.c \
		structs_type_ip4.c \
		structs_type_ip6.c \
		structs_type_null.c \
		structs_type_pointer.c \
		structs_type_regex.c \
		structs_type_string.c \
		structs_type_struct.c \
		structs_type_time.c \
		structs_type_union.c

structs_INCS+=	type/array.h \
		type/array_define.h \
		type/boolean.h \
		type/bpf.h \
		type/data.h \
		type/dnsname.h \
		type/ether.h \
		type/float.h \
		type/id.h \
		type/int.h \
		type/ip4.h \
		type/ip6.h \
		type/null.h \
		type/pointer.h \
		type/regex.h \
		type/string.h \
		type/struct.h \
		type/time.h \
		type/union.h

structs_MAN+=	type/structs_type_array.3
structs_MLINKS+=structs_type_array.3 DEFINE_STRUCTS_ARRAY.3
structs_MLINKS+=structs_type_array.3 STRUCTS_ARRAY_TYPE.3
structs_MLINKS+=structs_type_array.3 STRUCTS_FIXEDARRAY_TYPE.3
structs_MLINKS+=structs_type_array.3 structs_array_delete.3
structs_MLINKS+=structs_type_array.3 structs_array_insert.3
structs_MLINKS+=structs_type_array.3 structs_array_length.3
structs_MLINKS+=structs_type_array.3 structs_array_prep.3
structs_MLINKS+=structs_type_array.3 structs_array_reset.3
structs_MLINKS+=structs_type_array.3 structs_type_fixedarray.3

structs_MAN+=	type/structs_type_boolean.3
structs_MLINKS+=structs_type_boolean.3 structs_type_boolean_char.3
structs_MLINKS+=structs_type_boolean.3 structs_type_boolean_int.3
structs_MLINKS+=structs_type_boolean.3 structs_type_boolean_char_01.3
structs_MLINKS+=structs_type_boolean.3 structs_type_boolean_int_01.3

structs_MAN+=	type/structs_type_bpf.3
structs_MLINKS+=structs_type_bpf.3 BPF_STRUCTS_TYPE.3

structs_MAN+=	type/structs_type_data.3
structs_MLINKS+=structs_type_data.3 STRUCTS_DATA_TYPE.3
structs_MLINKS+=structs_type_data.3 STRUCTS_FIXEDDATA_TYPE.3
structs_MLINKS+=structs_type_data.3 structs_type_hexdata.3

structs_MAN+=	type/structs_type_dnsname.3

structs_MAN+=	type/structs_type_ether.3
structs_MLINKS+=structs_type_ether.3 structs_type_ether_nocolon.3

structs_MAN+=	type/structs_type_float.3
structs_MLINKS+=structs_type_float.3 structs_type_double.3

structs_MAN+=	type/structs_type_id.3
structs_MLINKS+=structs_type_id.3 STRUCTS_ID_TYPE.3

structs_MAN+=	type/structs_type_int.3
structs_MLINKS+=structs_type_int.3 structs_type_char.3
structs_MLINKS+=structs_type_int.3 structs_type_uchar.3
structs_MLINKS+=structs_type_int.3 structs_type_hchar.3
structs_MLINKS+=structs_type_int.3 structs_type_short.3
structs_MLINKS+=structs_type_int.3 structs_type_ushort.3
structs_MLINKS+=structs_type_int.3 structs_type_hshort.3
structs_MLINKS+=structs_type_int.3 structs_type_uint.3
structs_MLINKS+=structs_type_int.3 structs_type_hint.3
structs_MLINKS+=structs_type_int.3 structs_type_long.3
structs_MLINKS+=structs_type_int.3 structs_type_ulong.3
structs_MLINKS+=structs_type_int.3 structs_type_hlong.3
structs_MLINKS+=structs_type_int.3 structs_type_int8.3
structs_MLINKS+=structs_type_int.3 structs_type_uint8.3
structs_MLINKS+=structs_type_int.3 structs_type_hint8.3
structs_MLINKS+=structs_type_int.3 structs_type_int16.3
structs_MLINKS+=structs_type_int.3 structs_type_uint16.3
structs_MLINKS+=structs_type_int.3 structs_type_hint16.3
structs_MLINKS+=structs_type_int.3 structs_type_int32.3
structs_MLINKS+=structs_type_int.3 structs_type_uint32.3
structs_MLINKS+=structs_type_int.3 structs_type_hint32.3
structs_MLINKS+=structs_type_int.3 structs_type_int64.3
structs_MLINKS+=structs_type_int.3 structs_type_uint64.3
structs_MLINKS+=structs_type_int.3 structs_type_hint64.3

structs_MAN+=	type/structs_type_ip4.3

structs_MAN+=	type/structs_type_ip6.3

structs_MAN+=	type/structs_type_null.3

structs_MAN+=	type/structs_type_pointer.3
structs_MLINKS+=structs_type_pointer.3 STRUCTS_POINTER_TYPE.3

structs_MAN+=	type/structs_type_regex.3
structs_MLINKS+=structs_type_regex.3 STRUCTS_REGEX_TYPE.3
structs_MLINKS+=structs_type_regex.3 structs_type_regex_icase.3

structs_MAN+=	type/structs_type_string.3
structs_MLINKS+=structs_type_string.3 STRUCTS_FIXEDSTRING_TYPE.3
structs_MLINKS+=structs_type_string.3 STRUCTS_STRING_TYPE.3
structs_MLINKS+=structs_type_string.3 structs_type_string_null.3

structs_MAN+=	type/structs_type_struct.3
structs_MLINKS+=structs_type_struct.3 STRUCTS_STRUCT_FIELD.3
structs_MLINKS+=structs_type_struct.3 STRUCTS_STRUCT_FIELD2.3
structs_MLINKS+=structs_type_struct.3 STRUCTS_STRUCT_TYPE.3

structs_MAN+=	type/structs_type_time.3
structs_MLINKS+=structs_type_time.3 structs_type_time_abs.3
structs_MLINKS+=structs_type_time.3 structs_type_time_gmt.3
structs_MLINKS+=structs_type_time.3 structs_type_time_iso8601.3
structs_MLINKS+=structs_type_time.3 structs_type_time_local.3
structs_MLINKS+=structs_type_time.3 structs_type_time_rel.3

structs_MAN+=	type/structs_type_union.3
structs_MLINKS+=structs_type_union.3 DEFINE_STRUCTS_UNION.3
structs_MLINKS+=structs_type_union.3 STRUCTS_UNION_FIELD.3
structs_MLINKS+=structs_type_union.3 STRUCTS_UNION_TYPE.3
structs_MLINKS+=structs_type_union.3 structs_union_set.3

