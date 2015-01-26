
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/string_quote.h"
#include "util/typed_mem.h"

struct string_dequote_info {
	const char	*mtype;
	char		**bufp;
};

/*
 * Internal variables
 */
static const	char *escapes[2] = { "tnrvf\"\\", "\t\n\r\v\f\"\\" };
static const	char hexdigit[16] = "0123456789abcdef";

/*
 * Internal functions
 */
static void	string_dequote_cleanup(void *arg);

/*
 * Parse a doubly-quoted string token.
 */
char *
string_dequote(FILE *fp, const char *mtype)
{
	struct string_dequote_info info;
	char *buf = NULL;
	int alloc = 0;
	int len = 0;
	void *mem;
	int ch;

	/* Cleanup properly if thread is canceled */
	info.bufp = &buf;
	info.mtype = mtype;
	pthread_cleanup_push(string_dequote_cleanup, &info);

	/* Parse string */
	while ((ch = getc(fp)) != EOF) {

		/* Increase buffer length if necessary */
		if (len + 8 >= alloc) {
			alloc += 64;
			if ((mem = REALLOC(mtype, buf, alloc)) == NULL)
				goto fail;
			buf = mem;
		}

		/* Check special characters */
		switch (ch) {
		case '"':
			buf[len] = '\0';
			goto done;
		case '\\':
			switch ((ch = getc(fp))) {
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			    {
				char chsave[3];
				int x, k;

				for (x = k = 0; k < 3; k++) {
					if (k > 0 && (ch = getc(fp)) == EOF) {
						k--;	/* char not saved */
						break;
					}
					chsave[k] = ch;
					if (ch < '0' || ch > '7')
						break;
					x = (x << 3) + (ch - '0');
				}
				if (k == 3)		/* got a whole byte */
					buf[len++] = (char)x;
				else {			/* copy chars as-is */
					buf[len++] = '\\';
					for (x = 0; x <= k; x++)
						buf[len++] = chsave[x];
				}
				break;
			    }
			case 'x':
			    {
				char chsave[2];
				int x, k;

				for (x = k = 0; k < 2; k++) {
					if ((ch = getc(fp)) == EOF) {
						k--;	/* char not saved */
						break;
					}
					chsave[k] = ch;
					if (!isxdigit(ch))
						break;
					x = (x << 4) + (isdigit(ch) ?
					      (ch - '0') :
					      (tolower(ch) - 'a' + 10));
				}
				if (k == 2)		/* got a whole byte */
					buf[len++] = (char)x;
				else {			/* copy chars as-is */
					buf[len++] = '\\';
					buf[len++] = 'x';
					for (x = 0; x <= k; x++)
						buf[len++] = chsave[x];
				}
				break;
			    }

			case EOF:
				goto got_eof;

			default:
			    {
				char *x;

				if ((x = strchr(escapes[0], ch)) != NULL)
					buf[len++] = escapes[1][x - escapes[0]];
				else
					buf[len++] = ch;
			    }
			}
			break;
		default:
			buf[len++] = (char)ch;
			break;
		}
	}

got_eof:
	/* EOF was read: check for error or actual end of file */
	if (!ferror(fp))
		errno = EINVAL;

fail:
	/* Error */
	FREE(mtype, buf);
	buf = NULL;

done:;
	/* Done */
	pthread_cleanup_pop(0);
	return (buf);
}

/*
 * Cleanup for string_dequote()
 */
static void
string_dequote_cleanup(void *arg)
{
	struct string_dequote_info *const info = arg;

	FREE(info->mtype, *info->bufp);
}

/*
 * Enquote a string.
 */
char *
string_enquote(const char *s, const char *mtype)
{
	char *buf = NULL;
	int pass2 = 0;
	int len;
	char *t;
	int i;

pass2:
	/* Encode characters */
	len = 0;
	if (pass2)
		buf[len] = '"';
	len++;
	for (i = 0; s[i] != '\0'; i++) {
		if ((t = strchr(escapes[1], s[i])) != NULL) {
			if (pass2) {
				buf[len] = '\\';
				buf[len + 1] = escapes[0][t - escapes[1]];
			}
			len += 2;
		} else if (isprint(s[i])) {
			if (pass2)
				buf[len] = s[i];
			len++;
		} else {
			if (pass2) {
				buf[len] = '\\';
				buf[len + 1] = 'x';
				buf[len + 2] = hexdigit[((s[i]) >> 4) & 0x0f];
				buf[len + 3] = hexdigit[(s[i]) & 0x0f];
			}
			len += 4;
		}
	}
	if (pass2)
		buf[len] = '"';
	len++;

	/* Finish up */
	if (pass2) {
		buf[len] = '\0';
		return (buf);
	}

	/* Initialize buffer */
	if ((buf = MALLOC(mtype, len + 1)) == NULL)
		return (NULL);
	pass2 = 1;
	goto pass2;
}

#ifdef STRING_QUOTE_TEST

#include <unistd.h>
#include <err.h>

int
main(int ac, char **av)
{
	int decode = -1;
	FILE *fp;
	char *s;
	int ch;

	while ((ch = getopt(ac, av, "de")) != -1) {
		switch (ch) {
		case 'd':
			decode = 1;
			break;
		case 'e':
			decode = 0;
			break;
		default:
		usage:
			errx(1, "usage: string_quote -d dquotefile\n"
				"\tstring_quote -e [rawtext]");
		}
	}
	ac -= optind;
	av += optind;
	if (decode == -1)
		goto usage;
	if (decode && ac != 1)
		goto usage;
	if (!decode && ac != 0 && ac != 1)
		goto usage;

	/* Encode or decode */
	if (ac == 0)
		fp = stdin;
	else if ((fp = fopen(av[0], "r")) == NULL)
		err(1, "%s", av[0]);
	if (decode) {
		if ((ch = getc(fp)) != '"')
			errx(1, "input does not start with a double quote");
		if ((s = string_dequote(fp, TYPED_MEM_TEMP)) == NULL)
			err(1, "error dequoting %s", av[0]);
		fputs(s, stdout);
		FREE(TYPED_MEM_TEMP, s);
	} else {
		char buf[1024];
		int len;

		len = fread(buf, 1, sizeof(buf) - 1, fp);
		if (ferror(fp))
			err(1, "reading rawtext input");
		if (len == sizeof(buf) - 1)
			warnx("warning: only %u characters dealt with", len);
		buf[len] = '\0';
		if ((s = string_enquote(buf, TYPED_MEM_TEMP)) == NULL)
			err(1, "error dequoting %s", av[0]);
		fputs(s, stdout);
		putchar('\n');
		FREE(TYPED_MEM_TEMP, s);
	}
	return (0);
}

#endif /* STRING_QUOTE_TEST */

