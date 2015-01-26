
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_TINFO_H_
#define _PDEL_UTIL_TINFO_H_

/*
 * Support for generic per-thread data described by a structs type.
 */

struct tinfo;

/*
 * Optional user-supplied initialization routine. The routine should
 * initialize the memory pointed to by "data" as an instance of the
 * structs type for this tinfo data (i.e., t->type). If this method
 * is not supplied (i.e., NULL), then structs_init() is invoked instead.
 */
typedef int	tinfo_init_t(struct tinfo *t, void *data);

/* User-supplied data structure for a tinfo variable */
struct tinfo {
	const struct structs_type	*const type;	/* type for data */
	const char			*const mtype;	/* typed mem string */
	pthread_key_t			pkey;		/* per-thread var */
	tinfo_init_t			*init;		/* initialize data */
};

/* Invalid key value */
#define TINFO_KEY_INIT		((pthread_key_t)(-1))	/* XXX not portable */

/* Always use this macro to initialize a struct tinfo */
#define TINFO_INIT(type, mtype, init)	{ type, mtype, TINFO_KEY_INIT, init }

__BEGIN_DECLS

/*
 * Get thread-local variable.
 *
 * If the variable has not been initialized, a new one is instantiated
 * using the t->type's init method, and t->init is called (if not NULL).
 *
 * The caller should NOT free the returned value, but may modify it
 * in a way consistent with its structs type (i.e., t->type).
 */
extern void	*tinfo_get(struct tinfo *t);

/*
 * Set thread-local variable by copying "data".
 *
 * The "data" pointer must point to an object having structs type t->type.
 * The data is copied using structs_get(). Any previous thread-local
 * value is free'd and replaced.
 *
 * If "data" is NULL, then any existing information is free'd, so that
 * the next call to tinfo_get() will cause the init routine to be called
 * again.
 */
extern int	tinfo_set(struct tinfo *t, const void *data);

/*
 * Set thread-local variable to "data".
 *
 * This sets the thread-local variable to "data" without copying it.
 * "data" must point to memory allocated with type t->mtype.
 * Any previous thread-local value is free'd and replaced.
 *
 * The caller should not reference "data" if this function returns
 * successfully.
 */
extern int	tinfo_set_nocopy(struct tinfo *t, void *data);

__END_DECLS

#endif	/* _PDEL_UTIL_TINFO_H_ */

