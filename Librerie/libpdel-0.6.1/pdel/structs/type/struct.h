
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_STRUCT_H_
#define _PDEL_STRUCTS_TYPE_STRUCT_H_

#include <stddef.h>

/*********************************************************************
			STRUCTURE TYPES
*********************************************************************/

/* This structure describes one field in a structure */
typedef struct structs_field {
	const char			*name;
	const struct structs_type	*type;
	u_int16_t			size;
	u_int16_t			offset;
} structs_field;

/* Use this to describe a field and name it with the field's C name */
#define STRUCTS_STRUCT_FIELD(sname, fname, ftype) 			\
	{ #fname, ftype, sizeof(((struct sname *)0)->fname), 		\
	    offsetof(struct sname, fname) }

/* Use this to describe a field and give it a different name */
#define STRUCTS_STRUCT_FIELD2(sname, fname, dname, ftype) 		\
	{ dname, ftype, sizeof(((struct sname *)0)->fname), 		\
	    offsetof(struct sname, fname) }

/* This macro terminates a list of 'struct structs_field' structures */
#define STRUCTS_STRUCT_FIELD_END	{ NULL, NULL, 0, 0 }

/*
 *
 * Macro arguments:
 *	name				Structure name (as in 'struct name')
 *	[const struct structs_field *]	List of structure fields, terminated
 *					    by an entry with name == NULL.
 */
#define STRUCTS_STRUCT_TYPE(sname, flist) {				\
	sizeof(struct sname),						\
	"structure",							\
	STRUCTS_TYPE_STRUCTURE,						\
	structs_struct_init,						\
	structs_struct_copy,						\
	structs_struct_equal,						\
	structs_notsupp_ascify,						\
	structs_notsupp_binify,						\
	structs_struct_encode,						\
	structs_struct_decode,						\
	structs_struct_free,						\
	{ { (void *)(flist) }, { NULL }, { NULL } }			\
}

__BEGIN_DECLS

extern structs_init_t		structs_struct_init;
extern structs_copy_t		structs_struct_copy;
extern structs_equal_t		structs_struct_equal;
extern structs_encode_t		structs_struct_encode;
extern structs_decode_t		structs_struct_decode;
extern structs_uninit_t		structs_struct_free;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_STRUCT_H_ */

