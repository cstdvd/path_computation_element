
/*
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com>
 */

#ifndef _PDEL_STRUCTS_TYPE_ARRAY_DEFINE_H_
#define _PDEL_STRUCTS_TYPE_ARRAY_DEFINE_H_

#include <sys/types.h>

/*
 * The data must be a 'struct structs_array', or a structure
 * defined using the DEFINE_STRUCTS_ARRAY() macro.
 */
typedef struct structs_array {
	u_int	length;		/* number of elements in array */
	void	*elems;		/* array elements */
} structs_array;

/*
 * Use this to get 'elems' declared as the right type
 */
#define DEFINE_STRUCTS_ARRAY(name, etype)				\
struct name {								\
	u_int	length;		/* number of elements in array */	\
	etype	*elems;		/* array elements */			\
}

#define DEFINE_STRUCTS_ARRAY_T(name, etype)				\
typedef struct name {							\
	u_int	length;		/* number of elements in array */	\
	etype	*elems;		/* array elements */			\
} name

#define DEFINE_STRUCTS_CARRAY(name, etype)				\
struct name {								\
	u_int	length;		/* number of elements in array */	\
	const etype *elems;	/* array elements */			\
}

#define DEFINE_STRUCTS_CARRAY_T(name, etype)				\
typedef struct name {							\
	u_int	length;		/* number of elements in array */	\
	const etype *elems;	/* array elements */			\
} name

#endif
