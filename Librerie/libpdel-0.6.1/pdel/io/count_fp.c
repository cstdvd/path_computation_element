
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/typed_mem.h"
#include "io/count_fp.h"

#define MEM_TYPE		"count_fp"

/* Our state */
struct count_info {
	FILE		*fp;
	off_t		remain;
	int		closeit;
};

/*
 * Internal functions
 */
static int	count_read(void *cookie, char *buf, int len);
static int	count_close(void *cookie);

/*
 * Create a FILE * that reads a limited amount from another FILE *.
 */
FILE *
count_fopen(FILE *fp, off_t count, int closeit)
{
	struct count_info *c;

	/* Create info */
	if ((c = MALLOC(MEM_TYPE, sizeof(*c))) == NULL)
		return (NULL);
	memset(c, 0, sizeof(*c));
	c->fp = fp;
	c->remain = count;
	c->closeit = closeit;

	/* Create new FILE * */
	if ((fp = funopen(c, count_read, NULL, NULL, count_close)) == NULL) {
		FREE(MEM_TYPE, c);
		return (NULL);
	}

	/* Set to non-buffered so we don't read to much from underlying fp */
	setbuf(fp, NULL);

	/* Done */
	return (fp);
}

/*
 * Read from a count_fp.
 */
static int
count_read(void *cookie, char *buf, int len)
{
	struct count_info *const c = cookie;
	int ret;

	if (c->remain == 0)
		return (0);
	if (len > c->remain)
		len = c->remain;
	if ((ret = fread(buf, 1, len, c->fp)) != len) {
		if (ferror(c->fp))
			return (-1);
	}
	c->remain -= ret;
	return (ret);
}

/*
 * Close a count_fp.
 */
static int
count_close(void *cookie)
{
	struct count_info *const c = cookie;

	if (c->closeit)
		fclose(c->fp);
	FREE(MEM_TYPE, c);
	return (0);
}

