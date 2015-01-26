
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_STRING_FP_H_
#define _PDEL_IO_STRING_FP_H_

#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

__BEGIN_DECLS

/*
 * Create an input stream from a buffer. The buffer is copied if copy != 0.
 *
 * Returns NULL and sets errno if there was an error.
 */
extern FILE	*string_buf_input(const void *buf, size_t len, int copy);

/*
 * Create a new string output buffer. The string will be allocated
 * using the supplied memory type (the string is *not* copied).
 *
 * Returns NULL and sets errno if there was an error.
 */
extern FILE	*string_buf_output(const char *mtype);

/*
 * Get the current contents of the output buffer of a stream created
 * with string_buf_output().
 *
 * If 'reset' is true, then the output buffer is reset and the returned
 * value must be manually free'd (using the same memory type supplied
 * to string_buf_output); otherwise, it should not be freed or modified.
 *
 * In any case, the string is always NUL-terminated.
 *
 * Returns NULL and sets errno if there was an error; the stream will
 * still need to be closed.
 */
extern char	*string_buf_content(FILE *fp, int reset);

/*
 * Get the number of bytes currently in the output buffer
 * of a stream created with string_buf_output().
 */
extern int	string_buf_length(FILE *fp);

__END_DECLS

#endif	/* _PDEL_IO_STRING_FP_H_ */

