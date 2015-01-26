
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_util.h"

#define PATH_RANDOM		"/dev/urandom"

/*
 * Decode ASCII message
 */
void
ppp_util_ascify(char *buf, size_t bsiz, const u_char *data, size_t len)
{
	char *bp = buf;
	int i;

	for (bp = buf, i = 0; i < len; i++) {
		const char ch = (char)data[i];

		if (bsiz < 3)
			break;
		switch (ch) {
		case '\t':
			*bp++ = '\\';
			*bp++ = 't';
			bsiz -= 2;
			break;
		case '\n':
			*bp++ = '\\';
			*bp++ = 'n';
			bsiz -= 2;
			break;
		case '\r':
			*bp++ = '\\';
			*bp++ = 'r';
			bsiz -= 2;
			break;
		default:
			if (isprint(ch & 0x7f)) {
				*bp++ = ch;
				bsiz--;
			} else {
				*bp++ = '^';
				*bp++ = '@' + (ch & 0x1f);
				bsiz -= 2;
			}
			break;
		}
	}
	*bp = '\0';
}

/*
 * Generate random bytes
 */
int
ppp_util_random(void *buf, size_t len)
{
	int rlen;
	int fd;

	if ((fd = open(PATH_RANDOM, O_RDONLY)) == -1)
		return (-1);
	rlen = read(fd, buf, len);
	(void)close(fd);
	if (rlen == -1)
		return (-1);
	if (rlen != len) {
		errno = ENXIO;
		return (-1);
	}
	return (0);
}


