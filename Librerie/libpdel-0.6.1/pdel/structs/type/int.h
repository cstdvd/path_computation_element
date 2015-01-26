
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_INT_H_
#define _PDEL_STRUCTS_TYPE_INT_H_

/*********************************************************************
			INTEGRAL TYPES
*********************************************************************/

__BEGIN_DECLS

PD_IMPORT const struct structs_type	structs_type_char;
PD_IMPORT const struct structs_type	structs_type_uchar;
PD_IMPORT const struct structs_type	structs_type_hchar;
PD_IMPORT const struct structs_type	structs_type_short;
PD_IMPORT const struct structs_type	structs_type_ushort;
PD_IMPORT const struct structs_type	structs_type_hshort;
PD_IMPORT const struct structs_type	structs_type_int;
PD_IMPORT const struct structs_type	structs_type_uint;
PD_IMPORT const struct structs_type	structs_type_hint;
PD_IMPORT const struct structs_type	structs_type_long;
PD_IMPORT const struct structs_type	structs_type_ulong;
PD_IMPORT const struct structs_type	structs_type_hlong;
PD_IMPORT const struct structs_type	structs_type_int8;
PD_IMPORT const struct structs_type	structs_type_uint8;
PD_IMPORT const struct structs_type	structs_type_hint8;
PD_IMPORT const struct structs_type	structs_type_int16;
PD_IMPORT const struct structs_type	structs_type_uint16;
PD_IMPORT const struct structs_type	structs_type_hint16;
PD_IMPORT const struct structs_type	structs_type_int32;
PD_IMPORT const struct structs_type	structs_type_uint32;
PD_IMPORT const struct structs_type	structs_type_hint32;
PD_IMPORT const struct structs_type	structs_type_int64;
PD_IMPORT const struct structs_type	structs_type_uint64;
PD_IMPORT const struct structs_type	structs_type_hint64;

extern structs_ascify_t		structs_int_ascify;
extern structs_binify_t		structs_int_binify;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_INT_H_ */

