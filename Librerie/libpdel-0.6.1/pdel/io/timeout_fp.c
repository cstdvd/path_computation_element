
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "pdel/pd_poll.h"
#include "pdel/pd_io.h"
#include "structs/structs.h"
#include "structs/type/array.h"

#include "io/timeout_fp.h"
#include "util/typed_mem.h"

#define MEM_TYPE	"timeout_fp"

struct fdt_info {
	int	fd;
	int	timeout;
};

typedef int	fun_reader(void *cookie, char *buf, int len);
typedef int	fun_writer(void *cookie, const char *buf, int len);

/*
 * Internal functions
 */
static int 	timeout_fp_readwrite(struct fdt_info *fdt,
			char *buf, int len, int wr);
static int	timeout_fp_close(void *cookie);

static fun_reader	timeout_fp_read;
static fun_writer	timeout_fp_write;

/*
 * Same as fdopen(3) but with a timeout.
 */
FILE *
timeout_fdopen(int fd, const char *mode, int timeout)
{
	fun_reader *reader = NULL;
	fun_writer *writer = NULL;
	struct fdt_info *fdt;
	const char *s;
	FILE *fp;

	/* If no timeout, fall back to normal case */
	if (timeout <= 0)
		return (fdopen(fd, mode));

	/* Create the info structure */
	if ((fdt = MALLOC(MEM_TYPE, sizeof(*fdt))) == NULL)
		return (NULL);
	memset(fdt, 0, sizeof(*fdt));
	fdt->fd = fd;
	fdt->timeout = timeout;

	/* Get reader & writer functions */
	for (s = mode; *s != '\0'; s++) {
		switch (*s) {
		case 'r':
			reader = timeout_fp_read;
			break;
		case 'w':
		case 'a':
			writer = timeout_fp_write;
			break;
		case '+':
			reader = timeout_fp_read;
			writer = timeout_fp_write;
			break;
		}
	}

	/* Get FILE * wrapper */
	if ((fp = funopen(fdt, reader, writer,
	    NULL, timeout_fp_close)) == NULL) {
		FREE(MEM_TYPE, fdt);
		return (NULL);
	}

	/* Done */
	return (fp);
}

static int
timeout_fp_read(void *cookie, char *buf, int len)
{
	struct fdt_info *const fdt = cookie;

	return (timeout_fp_readwrite(fdt, buf, len, 0));
}

static int
timeout_fp_write(void *cookie, const char *buf, int len)
{
	struct fdt_info *const fdt = cookie;

	return (timeout_fp_readwrite(fdt, (char *)buf, len, 1));
}

/*
 * Do the threaded read or write depending on the value of 'wr'.
 */
static int
timeout_fp_readwrite(struct fdt_info *fdt, char *buf, int len, int wr)
{
	pd_pollfd pfd;
	int nfds;

	/* Set up read/write pd_poll(2) event */
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fdt->fd;
	pfd.events = wr ? PD_POLLWRNORM : PD_POLLRDNORM;

	/* Wait for readability or writability */
	if ((nfds = pd_poll(&pfd, 1, fdt->timeout * 1000)) == -1)
		return (-1);

	/* Check for timeout */
	if (nfds == 0) {
		errno = ETIMEDOUT;
		return (-1);
	}

	/* Do I/O */
	return (wr ? pd_write(fdt->fd, buf, len) : pd_read(fdt->fd, buf, len));
}

/*
 * Closer for timeout_fp streams.
 */
static int
timeout_fp_close(void *cookie)
{
	struct fdt_info *const fdt = cookie;
	int r;

	r = pd_close(fdt->fd);
	FREE(MEM_TYPE, fdt);
	return (r);
}

