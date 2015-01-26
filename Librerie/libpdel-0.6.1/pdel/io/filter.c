
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "io/filter.h"
#include "util/typed_mem.h"


/************************************************************************
			FILTER STREAMS
************************************************************************/

/* funopen(3) methods used by filter_fopen() */
static int	filter_stream_read(void *cookie, char *buf, int len);
static int	filter_stream_write(void *cookie, const char *buf, int len);
static int	filter_stream_close(void *cookie);

/* State used by filter_fopen() */
struct filter_stream {
	FILE		*fp;			/* underlying stream */
	struct filter	*filter;		/* filter object */
	int		output;			/* input or output? */
	int		eof;			/* eof has been reached */
	int		flags;			/* flags */
};

/*
 * Create a new filter stream using the supplied filter.
 */
FILE *
filter_fopen(struct filter *filter, int flags, FILE *fp, const char *mode)
{
	struct filter_stream *fs;
	int output;

	if (strcmp(mode, "r") == 0)
		output = 0;
	else if (strcmp(mode, "w") == 0)
		output = 1;
	else {
		errno = EINVAL;
		return (NULL);
	}
	if ((fs = MALLOC("filter_stream", sizeof(*fs))) == NULL)
		return (NULL);
	memset(fs, 0, sizeof(*fs));
	fs->filter = filter;
	fs->fp = fp;
	fs->output = output;
	fs->flags = flags;
	if ((fp = funopen(fs,
	    !fs->output ? filter_stream_read : NULL,
	    fs->output ? filter_stream_write : NULL,
	    NULL, filter_stream_close)) == NULL) {
		FREE("filter_stream", fs);
		return (NULL);
	}
	return (fp);
}

static int
filter_stream_read(void *cookie, char *buf, int len)
{
	struct filter_stream *const fs = cookie;
	u_char fbuf[1024];
	int total = 0;
	int flen;
	int num;
	int i;

	/*
	 * Read from underlying stream until we've read "len" bytes
	 * from the filter or the underlying stream returns EOF.
	 */
	for (total = 0; len > 0; ) {

		/* Read any remaining output from the filter */
		if ((num = filter_read(fs->filter, buf, len)) == -1)
			return (-1);
		buf += num;
		len -= num;
		total += num;

		/* No more data to be read from underlying stream? */
		if (fs->eof || len == 0)
			break;

		/*
		 * Read more data from the underlying stream,
		 * but not any more than necessary.
		 */
		flen = filter_convert(fs->filter, len, 0);
		flen = MIN(flen, sizeof(fbuf));
		clearerr(fs->fp);
		if ((num = fread(fbuf, 1, flen, fs->fp)) == 0) {
			if (ferror(fs->fp))
				return (total > 0 ? total : -1);
			fs->eof = 1;
			if (filter_end(fs->filter) == -1)
				return (total > 0 ? total : -1);
		}

		/* Send the data we just read through the filter */
		for (i = 0; i < num; i += flen) {
			if ((flen = filter_write(fs->filter,
			    fbuf + i, num - i)) == -1)
				return (total > 0 ? total : -1);
		}
	}
	return (total);
}

static int
filter_stream_write(void *cookie, const char *buf, int len)
{
	struct filter_stream *const fs = cookie;
	u_char fbuf[1024];
	int flen;

	if ((len = filter_write(fs->filter, buf, len)) == -1)
		return (-1);
	while ((flen = filter_read(fs->filter, fbuf, sizeof(fbuf))) > 0) {
		if (fwrite(fbuf, 1, flen, fs->fp) != flen)
			return (-1);
	}
	return (len);
}

static int
filter_stream_close(void *cookie)
{
	struct filter_stream *const fs = cookie;
	u_char buf[1024];
	int len;

	if (fs->output) {
		(void)filter_end(fs->filter);			/* XXX */
		while ((len = filter_read(fs->filter, buf, sizeof(buf))) > 0) {
			if (fwrite(buf, 1, len, fs->fp) != len)
				break;
		}
		fflush(fs->fp);
	}
	if ((fs->flags & FILTER_NO_CLOSE_STREAM) == 0)
		fclose(fs->fp);
	if ((fs->flags & FILTER_NO_DESTROY_FILTER) == 0)
		filter_destroy(&fs->filter);

	/* Done */
	FREE("filter_stream", fs);
	return (0);
}

/************************************************************************
			FILTER PROCESSING
************************************************************************/

/*
 * Filter data in memory using a filter.
 */
int
filter_process(struct filter *filter, const void *input, int ilen,
	int final, u_char **outputp, const char *mtype)
{
	int nw, w;
	int nr, r;
	int olen;

	/* Allocate buffer big enough to hold filter output */
	olen = filter_convert(filter, ilen, 1) + 10;
	if ((*outputp = MALLOC(mtype, olen)) == NULL)
		return (-1);

	/* Filter data */
	for (w = r = 0; w < ilen; w += nw, r += nr) {
		if ((nw = filter_write(filter,
		    (char *)input + w, MIN(ilen - w, 1024))) == -1)
			goto fail;
		if ((nr = filter_read(filter, *outputp + r, olen - r)) == -1)
			goto fail;
	}
	if (final) {
		if (filter_end(filter) == -1)
			goto fail;
		do {
			if ((nr = filter_read(filter,
			    *outputp + r, olen - r)) == -1)
				goto fail;
			r += nr;
		} while (nr != 0);
	}
	assert(r < olen);

	/* Done */
	(*outputp)[r] = 0;
	return (r);

fail:
	FREE(mtype, *outputp);
	*outputp = NULL;
	return (-1);
}

/************************************************************************
			FILTER METHOD WRAPPERS
************************************************************************/

int
filter_read(struct filter *filter, void *data, int len)
{
	return (*filter->read)(filter, data, len);
}

int
filter_write(struct filter *filter, const void *data, int len)
{
	return (*filter->write)(filter, data, len);
}

int
filter_end(struct filter *filter)
{
	return (*filter->end)(filter);
}

int
filter_convert(struct filter *filter, int num, int forward)
{
	return (*filter->convert)(filter, num, forward);
}

void
filter_destroy(struct filter **filterp)
{
	struct filter *const filter = *filterp;

	if (filter != NULL)
		(*filter->destroy)(filterp);
}

