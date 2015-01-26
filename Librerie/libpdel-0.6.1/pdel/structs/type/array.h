
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_ARRAY_H_
#define _PDEL_STRUCTS_TYPE_ARRAY_H_

/*********************************************************************
		    VARIABLE LENGTH ARRAYS
*********************************************************************/

#ifndef DEFINE_STRUCTS_ARRAY
#ifdef BUILDING_PDEL
#include "structs/type/array_define.h"
#else
#include "pdel/structs/type/array_define.h"
#endif
#endif

/*
 * Macro arguments:
 *	[const struct structs_type *]	Element type
 *	[const char *]			Memory allocation type for "elems"
 *	[const char *]			XML tag for individual elements
 */
#define STRUCTS_ARRAY_TYPE(etype, mtype, etag) {			\
	sizeof(struct structs_array),					\
	"array",							\
	STRUCTS_TYPE_ARRAY,						\
	structs_region_init,						\
	structs_array_copy,						\
	structs_array_equal,						\
	structs_notsupp_ascify,						\
	structs_notsupp_binify,						\
	structs_array_encode,						\
	structs_array_decode,						\
	structs_array_free,						\
	{ { (void *)(etype) }, { (void *)(mtype) }, { (void *)(etag) } }\
}

__BEGIN_DECLS

extern structs_copy_t		structs_array_copy;
extern structs_equal_t		structs_array_equal;
extern structs_encode_t		structs_array_encode;
extern structs_decode_t		structs_array_decode;
extern structs_uninit_t		structs_array_free;

/*
 * Additional functions for handling arrays
 */
extern int	structs_array_length(const struct structs_type *type,
			const char *name, const void *data);
extern int	structs_array_setsize(const struct structs_type *type,
			const char *name, u_int nitems, void *data, 
			int do_init);
extern int	structs_array_reset(const struct structs_type *type,
			const char *name, void *data);
extern int	structs_array_insert(const struct structs_type *type,
			const char *name, u_int indx, void *data);
extern int	structs_array_delete(const struct structs_type *type,
			const char *name, u_int indx, void *data);
extern int	structs_array_prep(const struct structs_type *type,
			const char *name, void *data);

__END_DECLS

/*********************************************************************
			FIXED LENGTH ARRAYS
*********************************************************************/

/*
 * Fixed length array type.
 */

/*
 * Macro arguments:
 *	[const struct structs_type *]	Element type
 *	[u_int]				Size of each element
 *	[u_int]				Length of array
 *	[const char *]			XML tag for individual elements
 */
#define STRUCTS_FIXEDARRAY_TYPE(etype, esize, alen, etag) {		\
	(esize) * (alen),						\
	"fixedarray",							\
	STRUCTS_TYPE_FIXEDARRAY,					\
	structs_fixedarray_init,					\
	structs_fixedarray_copy,					\
	structs_fixedarray_equal,					\
	structs_notsupp_ascify,						\
	structs_notsupp_binify,						\
	structs_fixedarray_encode,					\
	structs_fixedarray_decode,					\
	structs_fixedarray_free,					\
	{ { (void *)(etype) }, { (void *)(etag) }, { (void *)(alen) } }	\
}

__BEGIN_DECLS

PD_IMPORT structs_init_t		structs_fixedarray_init;
PD_IMPORT structs_copy_t		structs_fixedarray_copy;
PD_IMPORT structs_equal_t		structs_fixedarray_equal;
PD_IMPORT structs_encode_t		structs_fixedarray_encode;
PD_IMPORT structs_decode_t		structs_fixedarray_decode;
PD_IMPORT structs_uninit_t		structs_fixedarray_free;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_ARRAY_H_ */

