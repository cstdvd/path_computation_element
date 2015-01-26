
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
#include <pthread.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "io/base64.h"
#include "io/filter.h"
#include "util/typed_mem.h"

/************************************************************************
			BASE 64 ENCODER
************************************************************************/

#define ENCODER_MEM_TYPE	"base64_encoder"

/* Encoder state */
struct b64_encoder {
	struct filter	filter;
	pthread_mutex_t	mutex;
	char		*cmap;
	u_char		dbuf[3];
	int		doff;
	char		*obuf;
	int		olen;
	int		osize;
	u_char		done;
};

/* Internal functions */
static filter_read_t		b64_encoder_read;
static filter_write_t		b64_encoder_write;
static filter_end_t		b64_encoder_end;
static filter_convert_t		b64_encoder_convert;
static filter_destroy_t		b64_encoder_destroy;

static void	b64_encoder_encode(struct b64_encoder *enc, char *buf);
static int	b64_encoder_output(struct b64_encoder *enc, const char *buf);
static int	b64_cmap_check(const char *cmap);

/* Public variables */
PD_EXPORT const char b64_rfc2045_charset[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/*
 * Create a new base-64 encoder with optional custom character set.
 */
struct filter *
b64_encoder_create(const char *cmap)
{
	struct b64_encoder *enc;

	/* Default to HTTP charset */
	if (cmap == NULL)
		cmap = b64_rfc2045_charset;
	else if (b64_cmap_check(cmap) == -1)
		return (NULL);

	/* Create object */
	if ((enc = MALLOC(ENCODER_MEM_TYPE, sizeof(*enc))) == NULL)
		return (NULL);
	memset(enc, 0, sizeof(*enc));

	/* Copy character map */
	if ((enc->cmap = STRDUP(ENCODER_MEM_TYPE, cmap)) == NULL) {
		FREE(ENCODER_MEM_TYPE, enc);
		return (NULL);
	}

	/* Create mutex */
	if ((errno = pthread_mutex_init(&enc->mutex, NULL)) != 0) {
		FREE(ENCODER_MEM_TYPE, enc->cmap);
		FREE(ENCODER_MEM_TYPE, enc);
		return (NULL);
	}

	/* Set up methods */
	enc->filter.read = b64_encoder_read;
	enc->filter.write = b64_encoder_write;
	enc->filter.end = b64_encoder_end;
	enc->filter.convert = b64_encoder_convert;
	enc->filter.destroy = b64_encoder_destroy;

	/* Done */
	return (&enc->filter);
}

/*
 * Destroy a base 64 encoder.
 */
void
b64_encoder_destroy(struct filter **filterp)
{
	struct b64_encoder **const encp = (struct b64_encoder **)filterp;
	struct b64_encoder *const enc = *encp;

	if (enc != NULL) {
		pthread_mutex_destroy(&enc->mutex);
		FREE(ENCODER_MEM_TYPE, enc->cmap);
		FREE(ENCODER_MEM_TYPE, enc->obuf);
		FREE(ENCODER_MEM_TYPE, enc);
		*encp = NULL;
	}
}

/*
 * Write raw bytes into the encoder.
 */
int
b64_encoder_write(struct filter *filter, const void *data, int len)
{
	struct b64_encoder *const enc = (struct b64_encoder *)filter;
	char buf[4];
	int total;
	int chunk;
	int r;

	/* Lock encoder */
	r = pthread_mutex_lock(&enc->mutex);
	assert(r == 0);

	/* Check if closed */
	if (enc->done) {
		r = pthread_mutex_unlock(&enc->mutex);
		assert(r == 0);
		errno = EPIPE;
		return (-1);
	}

	/* Process bytes */
	for (total = 0; len > 0; total += chunk) {

		/* Read in next chunk of raw data */
		chunk = MIN(len, 3 - enc->doff);
		memcpy(enc->dbuf + enc->doff, data, chunk);
		data = (char *)data + chunk;
		len -= chunk;

		/* Process data if data buffer full */
		if ((enc->doff += chunk) == 3) {
			b64_encoder_encode(enc, buf);
			if (b64_encoder_output(enc, buf) == -1) {
				total = (total == 0) ? -1 : total;
				break;
			}
			enc->doff = 0;
		}

		/* Unlock encoder in case there is a reader */
		r = pthread_mutex_unlock(&enc->mutex);
		assert(r == 0);
		r = pthread_mutex_lock(&enc->mutex);
		assert(r == 0);
	}

	/* Done */
	r = pthread_mutex_unlock(&enc->mutex);
	assert(r == 0);
	return (total);
}

/*
 * Read out encoded data.
 */
int
b64_encoder_read(struct filter *filter, void *data, int len)
{
	struct b64_encoder *const enc = (struct b64_encoder *)filter;
	int r;

	r = pthread_mutex_lock(&enc->mutex);
	assert(r == 0);
	len = MIN(len, enc->olen);
	memcpy(data, enc->obuf, len);
	memmove(enc->obuf, enc->obuf + len, enc->olen - len);
	enc->olen -= len;
	r = pthread_mutex_unlock(&enc->mutex);
	assert(r == 0);
	return (len);
}

/*
 * Mark end of written data.
 */
int
b64_encoder_end(struct filter *filter)
{
	struct b64_encoder *const enc = (struct b64_encoder *)filter;
	char buf[4];
	int r;

	/* Lock encoder */
	r = pthread_mutex_lock(&enc->mutex);
	assert(r == 0);

	/* Only do this once */
	if (enc->done)
		goto done;

	/* Pad with termination character(s) */
	if (enc->doff > 0) {
		memset(enc->dbuf + enc->doff, 0, 3 - enc->doff);
		b64_encoder_encode(enc, buf);
		switch (enc->doff) {
		case 1:
			buf[2] = enc->cmap[64];
			/* fall through */
		case 2:
			buf[3] = enc->cmap[64];
			break;
		}
		if (b64_encoder_output(enc, buf) == -1) {
			r = pthread_mutex_unlock(&enc->mutex);
			assert(r == 0);
			return (-1);
		}
	}

done:
	/* Done */
	enc->done = 1;
	r = pthread_mutex_unlock(&enc->mutex);
	assert(r == 0);
	return (0);
}

/*
 * Convert byte count to read before and after filter.
 */
static int
b64_encoder_convert(struct filter *filter, int num, int forward)
{
	if (forward)
		return (((num * 4) + 2) / 3);
	else
		return (((num * 3) + 3) / 4);
}

/*
 * Encode one chunk of data (3 bytes -> 4 characters).
 *
 * This assumes the encoder is locked.
 */
static void
b64_encoder_encode(struct b64_encoder *enc, char *buf)
{
	buf[0] = (enc->dbuf[0] >> 2) & 0x3f;
	buf[0] = enc->cmap[(u_char)buf[0]];
	buf[1] = ((enc->dbuf[0] << 4) & 0x30) | ((enc->dbuf[1] >> 4) & 0x0f);
	buf[1] = enc->cmap[(u_char)buf[1]];
	buf[2] = ((enc->dbuf[1] << 2) & 0x3c) | ((enc->dbuf[2] >> 6) & 0x03);
	buf[2] = enc->cmap[(u_char)buf[2]];
	buf[3] = enc->dbuf[2] & 0x3f;
	buf[3] = enc->cmap[(u_char)buf[3]];
}

/*
 * Append encoded characters to the output buffer.
 *
 * This assumes the encoder is locked.
 */
static int
b64_encoder_output(struct b64_encoder *enc, const char *buf)
{
	if (enc->olen + 4 > enc->osize) {
		const int new_osize = (enc->olen * 2) + 31;
		char *new_obuf;

		if ((new_obuf = REALLOC(ENCODER_MEM_TYPE,
		    enc->obuf, new_osize)) == NULL)
			return (-1);
		enc->obuf = new_obuf;
		enc->osize = new_osize;
	}
	memcpy(enc->obuf + enc->olen, buf, 4);
	enc->olen += 4;
	return (0);
}

/*
 * Sanity check character map.
 */
static int
b64_cmap_check(const char *cmap)
{
	int i;
	int j;

	if (strlen(cmap) != 65)
		goto bogus;
	for (i = 0; i < 64; i++) {
		for (j = i + 1; j < 65; j++) {
			if (cmap[i] == cmap[j]) {
bogus:				errno = EINVAL;
				return (-1);
			}
		}
	}
	return (0);
}

/************************************************************************
			BASE 64 DECODER
************************************************************************/

#define DECODER_MEM_TYPE	"base64_decoder"

/* Encoder state */
struct b64_decoder {
	struct filter	filter;
	pthread_mutex_t	mutex;
	u_char		cmap[256];
	u_char		cbuf[4];
	int		coff;
	u_char		*obuf;
	int		olen;
	int		osize;
	char		pad;
	u_char		done;
	u_char		strict;
};

/* Internal functions */
static filter_read_t		b64_decoder_read;
static filter_write_t		b64_decoder_write;
static filter_end_t		b64_decoder_end;
static filter_convert_t		b64_decoder_convert;
static filter_destroy_t		b64_decoder_destroy;

static void	b64_decoder_decode(struct b64_decoder *dec, u_char *buf);
static int	b64_decoder_output(struct b64_decoder *dec,
			u_char *buf, int len);

/*
 * Create a new base-64 decoder with optional custom character set.
 */
struct filter *
b64_decoder_create(const char *cmap, int strict)
{
	struct b64_decoder *dec;
	const char *s;
	u_char byte;
	int i;

	/* Default to HTTP charset */
	if (cmap == NULL)
		cmap = b64_rfc2045_charset;
	else if (b64_cmap_check(cmap) == -1)
		return (NULL);

	/* Create object */
	if ((dec = MALLOC(DECODER_MEM_TYPE, sizeof(*dec))) == NULL)
		return (NULL);
	memset(dec, 0, sizeof(*dec));
	dec->strict = !!strict;
	dec->pad = cmap[64];

	/* Create mutex */
	if ((errno = pthread_mutex_init(&dec->mutex, NULL)) != 0) {
		FREE(DECODER_MEM_TYPE, dec);
		return (NULL);
	}

	/* Create inverse character map */
	memset(dec->cmap, 0xff, sizeof(dec->cmap));
	for (i = 1; i < sizeof(dec->cmap); i++) {
		if ((s = strchr(cmap, (char)i)) != NULL) {
			byte = (u_char)(s - cmap);
			dec->cmap[i] = (byte == 64) ? 0xfe : byte;
		}
	}

	/* Set up methods */
	dec->filter.read = b64_decoder_read;
	dec->filter.write = b64_decoder_write;
	dec->filter.end = b64_decoder_end;
	dec->filter.convert = b64_decoder_convert;
	dec->filter.destroy = b64_decoder_destroy;

	/* Done */
	return (&dec->filter);
}

/*
 * Destroy a base 64 decoder.
 */
void
b64_decoder_destroy(struct filter **filterp)
{
	struct b64_decoder **const decp = (struct b64_decoder **)filterp;
	struct b64_decoder *const dec = *decp;

	if (dec != NULL) {
		pthread_mutex_destroy(&dec->mutex);
		FREE(DECODER_MEM_TYPE, dec->obuf);
		FREE(DECODER_MEM_TYPE, dec);
		*decp = NULL;
	}
}

/*
 * Write encoded characters into the decoder.
 */
int
b64_decoder_write(struct filter *filter, const void *data, int len)
{
	struct b64_decoder *const dec = (struct b64_decoder *)filter;
	u_char buf[3];
	int total = 0;
	int r;

	/* Lock decoder */
	r = pthread_mutex_lock(&dec->mutex);
	assert(r == 0);

	/* Check if closed */
	if (dec->done) {
		r = pthread_mutex_unlock(&dec->mutex);
		assert(r == 0);
		errno = EPIPE;
		return (-1);
	}

	/* Process bytes */
	while (total < len) {

		/* Fill up input buffer */
		for ( ; dec->coff < 4 && total < len; total++) {
			const u_char val = dec->cmap[((u_char *)data)[total]];

			if (val == 0xff) {
				if (dec->strict) {
					errno = EINVAL;
					total = (total == 0) ? -1 : total;
					goto done;
				}
				continue;
			}
			if (val == 0xfe)			/* pad char */
				continue;
			assert(val < 64);
			dec->cbuf[dec->coff++] = val;
		}

		/* Process characters if character buffer is full */
		if (dec->coff == 4) {
			b64_decoder_decode(dec, buf);
			if (b64_decoder_output(dec, buf, 3) == -1) {
				total = (total == 0) ? -1 : total;
				goto done;
			}
			dec->coff = 0;
		}

		/* Take a breather */
		r = pthread_mutex_unlock(&dec->mutex);
		assert(r == 0);
		r = pthread_mutex_lock(&dec->mutex);
		assert(r == 0);
	}

done:
	/* Done */
	r = pthread_mutex_unlock(&dec->mutex);
	assert(r == 0);
	return (total);
}

/*
 * Read out decoded data.
 */
int
b64_decoder_read(struct filter *filter, void *buf, int len)
{
	struct b64_decoder *const dec = (struct b64_decoder *)filter;
	int r;

	r = pthread_mutex_lock(&dec->mutex);
	assert(r == 0);
	len = MIN(len, dec->olen);
	memcpy(buf, dec->obuf, len);
	memmove(dec->obuf, dec->obuf + len, dec->olen - len);
	dec->olen -= len;
	r = pthread_mutex_unlock(&dec->mutex);
	assert(r == 0);
	return (len);
}

/*
 * Mark end of written data.
 */
int
b64_decoder_end(struct filter *filter)
{
	struct b64_decoder *const dec = (struct b64_decoder *)filter;
	u_char buf[3];
	int len;
	int r;

	/* Lock decoder */
	r = pthread_mutex_lock(&dec->mutex);
	assert(r == 0);

	/* Only do this once */
	if (dec->done)
		goto done;

	/* Spit out any trailing characters */
	if (dec->coff > 0) {
		switch (dec->coff) {
		case 1:				/* really not valid */
		case 2:
			len = 1;
			break;
		case 3:
			len = 2;
			break;
		default:
			assert(0);
			len = 0;		/* silence gcc warning */
		}
		memset(dec->cbuf + dec->coff, 0, 4 - dec->coff);
		b64_decoder_decode(dec, buf);
		if (b64_decoder_output(dec, buf, len) == -1) {
			r = pthread_mutex_unlock(&dec->mutex);
			assert(r == 0);
			return (-1);
		}
		dec->coff = 0;
	}

done:
	/* Done */
	dec->done = 1;
	r = pthread_mutex_unlock(&dec->mutex);
	assert(r == 0);
	return (0);
}

/*
 * Convert byte count to read before and after filter.
 */
static int
b64_decoder_convert(struct filter *filter, int num, int forward)
{
	if (forward)
		return (((num * 3) + 3) / 4);
	else
		return (((num * 4) + 2) / 3);
}

/*
 * Decode one chunk of characters (4 characters -> 3 bytes).
 *
 * This assumes the decoder is locked.
 */
static void
b64_decoder_decode(struct b64_decoder *dec, u_char *buf)
{
	buf[0] = (dec->cbuf[0] << 2) | (dec->cbuf[1] >> 4);
	buf[1] = (dec->cbuf[1] << 4) | (dec->cbuf[2] >> 2);
	buf[2] = (dec->cbuf[2] << 6) | dec->cbuf[3];
}

/*
 * Append decoded characters to the output buffer.
 *
 * This assumes the decoder is locked.
 */
static int
b64_decoder_output(struct b64_decoder *dec, u_char *buf, int len)
{
	if (dec->olen + len > dec->osize) {
		const int new_osize = (dec->olen * 2) + 31;
		u_char *new_obuf;

		if ((new_obuf = REALLOC(DECODER_MEM_TYPE,
		    dec->obuf, new_osize)) == NULL)
			return (-1);
		dec->obuf = new_obuf;
		dec->osize = new_osize;
	}
	memcpy(dec->obuf + dec->olen, buf, len);
	dec->olen += len;
	return (0);
}

#ifdef BASE64_TEST

#include <err.h>
#include <unistd.h>

int
main(int ac, char **av)
{
	struct filter *filter;
	int do_encode = 1;			/* default encode */
	int do_input = 1;			/* default test input stream */
	int strict = 0;
	char buf[123];
	FILE *input;
	FILE *output;
	int len;
	int ch;

	/* Process command line */
	while ((ch = getopt(ac, av, "deiso")) != -1) {
		switch (ch) {
		case 'd':			/* decode */
			do_encode = 0;
			break;
		case 'e':			/* encode */
			do_encode = 1;
			break;
		case 'i':			/* test input stream */
			do_input = 1;
			break;
		case 'o':			/* test output stream */
			do_input = 0;
			break;
		case 's':			/* enforce strict decoding */
			strict = 1;
			break;
		default:
		usage:
			errx(1, "usage: base64 <-e|-d> <-i|-o> [-s]");
		}
	}
	ac -= optind;
	av += optind;

	/* Sanity */
	if (do_input == -1 || do_encode == -1)
		goto usage;

	/* Get filter */
	if (do_encode) {
		if ((filter = b64_encoder_create(NULL)) == NULL)
			err(1, "b64_encoder_create");
	} else {
		if ((filter = b64_decoder_create(NULL, strict)) == NULL)
			err(1, "b64_decoder_create");
	}

	/* Get filter stream */
	if (do_input) {
		if ((output = filter_fopen(filter, 0, stdout, "w")) == NULL)
			err(1, "filter_fopen");
		input = stdin;
	} else {
		if ((input = filter_fopen(filter, 0, stdin, "r")) == NULL)
			err(1, "filter_fopen");
		output = stdout;
	}

	/* Convert */
	while ((len = fread(buf, 1, sizeof(buf), input)) != 0)
		fwrite(buf, 1, len, output);

	/* Done */
	fclose(output);
	return (0);
}

#endif	/* BASE64_TEST */

