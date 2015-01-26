
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_STRING_H_
#define _PDEL_STRUCTS_TYPE_STRING_H_

/*********************************************************************
			STRING TYPES
*********************************************************************/

__BEGIN_DECLS

/*
 * Dynamically allocated string types
 *
 * Type-specific arguments:
 *	[const char *]	Memory allocation type
 *	[int]		Whether to store empty string as NULL or ""
 */
extern structs_init_t		structs_string_init;
extern structs_equal_t		structs_string_equal;
extern structs_ascify_t		structs_string_ascify;
extern structs_binify_t		structs_string_binify;
extern structs_encode_t		structs_string_encode;
extern structs_decode_t		structs_string_decode;
extern structs_uninit_t		structs_string_free;

__END_DECLS

#define STRUCTS_TYPE_STRING_MTYPE	"structs_type_string"

#define STRUCTS_STRING_TYPE(mtype, asnull) {				\
	sizeof(char *),							\
	"string",							\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_string_init,						\
	structs_ascii_copy,						\
	structs_string_equal,						\
	structs_string_ascify,						\
	structs_string_binify,						\
	structs_string_encode,						\
	structs_string_decode,						\
	structs_string_free,						\
	{ { (void *)(mtype) }, { (void *)(asnull) }, { NULL } }		\
}

__BEGIN_DECLS

/* A string type with allocation type "structs_type_string" and never NULL */
PD_IMPORT const struct structs_type	structs_type_string;

/* A string type with allocation type "structs_type_string" and can be NULL */
PD_IMPORT const struct structs_type	structs_type_string_null;

__END_DECLS

/*********************************************************************
		    BOUNDED LENGTH STRING TYPES
*********************************************************************/

__BEGIN_DECLS

/*
 * Bounded length string types
 *
 * Type-specific arguments:
 *	[int]		Size of string buffer
 */
extern structs_equal_t		structs_bstring_equal;
extern structs_ascify_t		structs_bstring_ascify;
extern structs_binify_t		structs_bstring_binify;

__END_DECLS

#define STRUCTS_FIXEDSTRING_TYPE(bufsize) {				\
	(bufsize),							\
	"fixedstring",							\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_region_copy,						\
	structs_bstring_equal,						\
	structs_bstring_ascify,						\
	structs_bstring_binify,						\
	structs_string_encode,						\
	structs_string_decode,						\
	structs_nothing_free,						\
	{ { NULL }, { NULL }, { NULL } }				\
}

#endif	/* _PDEL_STRUCTS_TYPE_STRING_H_ */

