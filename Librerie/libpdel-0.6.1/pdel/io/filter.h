
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_IO_FILTER_H_
#define _PDEL_IO_FILTER_H_

/*
 * Filters
 *
 * A filter is an object that has an input side and an output side
 * and performs some kind of encoding on the data as it passes through.
 *
 * Filters are useful because they can be pushed on top of a FILE *
 * stream in order to encode or decode data.
 */

struct filter;

/***********************************************************************
			FILTER METHODS
***********************************************************************/

/*
 * Read up to 'len' bytes from a filter.
 *
 * Returns the number of bytes read, or -1 and sets errno on error.
 * Returns zero if the filter is empty and needs more data to be written in.
 */
typedef int	filter_read_t(struct filter *f, void *buf, int len);

/*
 * Write up to 'len' bytes into a filter.
 *
 * Returns the number of bytes written, or -1 and sets errno on error.
 * Returns zero if the filter is full and needs more data to be read out.
 */ 
typedef int	filter_write_t(struct filter *f, const void *data, int len);

/*
 * Indicate to a filter that no more data is to be written.
 *
 * Returns zero if successful, or -1 and sets errno on error.
 * Once this is called, the write method will return -1 and errno = EPIPE.
 */
typedef int	filter_end_t(struct filter *f);

/*
 * Convert the number of bytes of input vs. output.
 *
 * If 'forward' is true, this should return an upper bound on the
 * number of bytes of that 'num' bytes of input will generate.
 * Otherwise, this should return an upper bound on the number of bytes
 * of input that would be required to generate 'num' (or more) bytes
 * of output.
 */
typedef int	filter_convert_t(struct filter *f, int num, int forward);

/*
 * Destroy a filter.
 *
 * This method must do two things:
 *	(a) Free all resources associated with the filter *fp
 *	(b) Set *fp to NULL
 *
 * If *fp is already equal to NULL, this method must do nothing.
 */
typedef void	filter_destroy_t(struct filter **fp);

/***********************************************************************
			FILTER STRUCTURE
***********************************************************************/

/* Filter object structure */
struct filter {
	filter_read_t		*read;		/* read data out of filter */
	filter_write_t		*write;		/* write data into filter */
	filter_end_t		*end;		/* signal end of data */
	filter_convert_t	*convert;	/* map # bytes in <-> out */
	filter_destroy_t	*destroy;	/* destroy filter */
	void			*private;	/* object private data */
};

/***********************************************************************
			FILTER LIBRARY ROUTINES
***********************************************************************/

__BEGIN_DECLS

#ifndef _KERNEL

/*
 * Given a read-only or write-only stream, return another stream
 * that reads or writes data from the supplied stream using
 * the filter. In other words, in the returned stream, the filter
 * is sitting on top of the supplied stream.
 *
 * When the stream is closed, the underlying stream is closed unless
 * the FILTER_NO_CLOSE_STREAM flag was given, and the filter is destroyed
 * unless the FILTER_NO_DESTROY_FILTER flags was given.
 *
 * Returns NULL and sets errno on error.
 */
#define FILTER_NO_CLOSE_STREAM		0x01	/* fclose() doesn't close fp */
#define FILTER_NO_DESTROY_FILTER	0x02	/* fclose() doesn't rm filter */

extern FILE	*filter_fopen(struct filter *filter, int flags,
			FILE *fp, const char *mode);

#endif	/* !_KERNEL */

/*
 * Filter data in memory using a filter.
 *
 * The filtered data is in *outputp and the length of the data is returned.
 * *outputp is guaranteed to have one extra byte allocated and set to zero.
 * If 'final' is non-zero then the write side of the filter is closed.
 *
 * Returns -1 and sets errno on error.
 */
extern int	filter_process(struct filter *filter, const void *input,
			int len, int final, u_char **outputp,
			const char *mtype);

/*
 * Wrappers for the filter methods.
 */
extern filter_read_t	filter_read;
extern filter_write_t	filter_write;
extern filter_end_t	filter_end;
extern filter_convert_t	filter_convert;
extern filter_destroy_t	filter_destroy;

__END_DECLS

#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

#endif	/* _PDEL_IO_FILTER_H_ */

