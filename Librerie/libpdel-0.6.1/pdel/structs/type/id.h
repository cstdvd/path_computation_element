
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_ID_H_
#define _PDEL_STRUCTS_TYPE_ID_H_

/*********************************************************************
			IDENTIFIER TYPES
*********************************************************************/

/*
 * Types where the range of possible values is a list of unique string
 * identifiers which have corresponding unique integral values.
 * The integral value is stored in memory as a 1, 2, or 4 byte value.
 *
 * The default value for an instance is first identifier in the list.
 *
 * For example, "red", "green", or "blue".
 *
 * Arguments to STRUCTS_ID_TYPE() macro:
 *	[struct structs_id *]	List of identifiers, terminated by NULL "id"
 *	[int]			Size of identifier word in bytes (1, 2, or 4)
 */

struct structs_id {
	const char	*id;		/* string representation */
	u_int32_t	value;		/* integer representation */
	u_int		imatch;		/* case-insensitive matching allowed */
};

__BEGIN_DECLS

extern structs_init_t		structs_id_init;
extern structs_ascify_t		structs_id_ascify;
extern structs_binify_t		structs_id_binify;

__END_DECLS

#define STRUCTS_ID_TYPE(idlist, vsize) {				\
	(vsize),							\
	"id",								\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_id_init,						\
	structs_region_copy,						\
	structs_region_equal,						\
	structs_id_ascify,						\
	structs_id_binify,						\
	structs_region_encode_netorder,					\
	structs_region_decode_netorder,					\
	structs_nothing_free,						\
	{ { (idlist) }, { NULL }, { NULL } }				\
}

#endif	/* _PDEL_STRUCTS_TYPE_ID_H_ */

