
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_KERNELGLUE_H_
#define _PDEL_UTIL_KERNELGLUE_H_

#ifndef _KERNEL
#error This #include file is only for use within a kernel compilation
#endif

#ifdef _PDEL_UTIL_TYPED_MEM_H_
#error kernelglue.h must be included before typed_mem.h
#endif

/*
 * Undefine kernel MALLOC() and FREE() macros
 */
#undef MALLOC
#undef FREE

/*
 * Declare memory type for typed memory chunks
 */
MALLOC_DECLARE(M_LIBPDEL);

/*
 * Kernel glue for 'errno'
 */
extern int errno;

/*
 * Kernel glue for misc user-land macros
 */
#define MAX(x,y)	max(x,y)
#define MIN(x,y)	min(x,y)
#define assert(x) 	KASSERT((x), \
			    ("%s: assert failed: %s", __FUNCTION__, #x))

/*
 * Kernel glue for misc user-land functions
 */
extern char		*strchr(const char *s, int c);
extern char		*strdup(const char *s);
extern int		asprintf(char **ret, const char *format, ...);
extern int		vasprintf(char **ret, const char *format, va_list ap);
extern int		strcasecmp(const char *s1, const char *s2);
extern size_t		strlcpy(char *dst, const char *src, size_t siz);
extern char		*strerror(int errnum);
extern time_t		time(time_t *);
extern void		*memmove(void *dst, const void *src, size_t len);
extern void		*kern_malloc(size_t size);
extern void		*kern_realloc(void *mem, size_t size);
extern void		kern_free(void *mem);

#endif	/* _PDEL_UTIL_KERNELGLUE_H_ */

