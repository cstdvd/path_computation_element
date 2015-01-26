
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_UNION_H_
#define _PDEL_STRUCTS_TYPE_UNION_H_

/*********************************************************************
			UNION TYPES
*********************************************************************/

/*
 * Union type.
 *
 * The data must be a 'struct structs_union', or a structure
 * defined using the DEFINE_STRUCTS_UNION() macro.
 */
typedef struct structs_union {
#ifdef __cplusplus
	const char	*field_name;		/* name of field in use */
#else
	const char	*const field_name;	/* name of field in use */
#endif
	void		*un;			/* pointer to field contents */
} structs_union;

/*
 * Use this to get the 'un' field declared having the correct type.
 */
#ifdef __cplusplus
#define DEFINE_STRUCTS_UNION(sname, uname)				\
struct sname {								\
	const char	*field_name;	    /* name of field in use */	\
	union uname	*un;		    /* pointer to the field */	\
}
#define DEFINE_STRUCTS_UNION_T(sname, uname)				\
struct sname {								\
	const char	*field_name;	    /* name of field in use */	\
	union uname	*un;		    /* pointer to the field */	\
}
#else
#define DEFINE_STRUCTS_UNION(sname, uname)				\
struct sname {								\
	const char	*const field_name;  /* name of field in use */	\
	union uname	*un;		    /* pointer to the field */	\
}
#define DEFINE_STRUCTS_UNION_T(sname, uname)				\
typedef struct sname {							\
	const char	*const field_name;  /* name of field in use */	\
	union uname	*un;		    /* pointer to the field */	\
} sname
#endif

/* This structure describes one field in a union */
typedef struct structs_ufield {
	const char			*name;		/* name of field */
	const struct structs_type	*type;		/* type of field */
	char				*dummy;		/* for type checking */
} structs_ufield;

/* Use this macro to initialize a 'struct structs_ufield' */
#define STRUCTS_UNION_FIELD(fname, ftype) 				\
	{ #fname, ftype, NULL }

/* This macro terminates a list of 'struct structs_ufield' structures */
#define STRUCTS_UNION_FIELD_END		{ NULL, NULL, NULL }

__BEGIN_DECLS

/*
 * Union type
 *
 * Type-specific arguments:
 *	[const struct structs_ufield *]	List of union fields, terminated
 *					    by entry with name == NULL.
 *	[const char *]			Memory allocation type for "un"
 */
extern structs_init_t		structs_union_init;
extern structs_copy_t		structs_union_copy;
extern structs_equal_t		structs_union_equal;
extern structs_encode_t		structs_union_encode;
extern structs_decode_t		structs_union_decode;
extern structs_uninit_t		structs_union_free;

#define STRUCTS_UNION_TYPE(uname, flist) {				\
	sizeof(struct structs_union),					\
	"union",							\
	STRUCTS_TYPE_UNION,						\
	structs_union_init,						\
	structs_union_copy,						\
	structs_union_equal,						\
	structs_notsupp_ascify,						\
	structs_notsupp_binify,						\
	structs_union_encode,						\
	structs_union_decode,						\
	structs_union_free,						\
	{ { (void *)(flist) }, { (void *)("union " #uname) }, { NULL } }\
}

/* Functions */
extern int	structs_union_set(const struct structs_type *type,
			const char *name, void *data, const char *field_name);

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_UNION_H_ */

