
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/data.h"
#include "io/filter.h"
#include "io/base64.h"
#include "util/typed_mem.h"

#define HEXVAL(c)	(isdigit(c) ? (c) - '0' : tolower(c) - 'a' + 10)

static const char hexchars[] = "0123456789abcdef";

/*********************************************************************
			BINARY DATA TYPE
*********************************************************************/

int
structs_data_copy(const struct structs_type *type, const void *from, void *to)
{
	const char *const mtype = type->args[1].v;
	const struct structs_data *const fdata = from;
	struct structs_data *const tdata = to;
	u_char *copy;

	if (fdata->length == 0) {
		memset(tdata, 0, sizeof(*tdata));
		return (0);
	}
	if ((copy = MALLOC(mtype, fdata->length)) == NULL)
		return (-1);
	memcpy(copy, fdata->data, fdata->length);
	tdata->data = copy;
	tdata->length = fdata->length;
	return (0);
}

int
structs_data_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	const struct structs_data *const d1 = v1;
	const struct structs_data *const d2 = v2;

	return (d1->length == d2->length
	    && memcmp(d1->data, d2->data, d1->length) == 0);
}

char *
structs_data_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const char *const charmap = type->args[0].v;
	const struct structs_data *const d = data;
	struct filter *encoder;
	char *edata;
	int elen;
	int i;

	/* Handle hex encoding */
	if (charmap != NULL && *charmap == '\0') {
		if ((edata = MALLOC(mtype, d->length * 2 + 1)) == NULL)
			return (NULL);
		for (i = 0; i < d->length; i++) {
			edata[i * 2] = hexchars[(d->data[i] >> 4) & 0x0f];
			edata[i * 2 + 1] = hexchars[d->data[i] & 0x0f];
		}
		edata[i * 2] = '\0';
		return (edata);
	}

	/* Get encoding filter */
	if ((encoder = b64_encoder_create(charmap)) == NULL)
		return (NULL);

	/* Encode data */
	elen = filter_process(encoder, d->data,
	    d->length, 1, (u_char **)&edata, mtype);
	filter_destroy(&encoder);
	if (elen == -1)
		return (NULL);

	/* Return encoded data as a string */
	edata[elen] = '\0';
	return (edata);
}

int
structs_data_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const char *const charmap = type->args[0].v;
	const char *const mtype = type->args[1].v;
	struct structs_data *const d = data;
	struct filter *decoder;
	u_char *bdata;
	int blen;
	int i;

	/* Handle hex encoding */
	if (charmap != NULL && *charmap == '\0') {
		if ((bdata = MALLOC(mtype, (strlen(ascii) + 1) / 2)) == NULL)
			return (-1);
		for (blen = 0; *ascii != '\0'; blen++) {
			while (isspace(*ascii))
				ascii++;
			if (*ascii == '\0')
				break;
			bdata[blen] = 0;
			for (i = 4; i >= 0; i -= 4) {
				if (!isxdigit(*ascii)) {
					strlcpy(ebuf, *ascii == '\0' ?
					    "odd length hex sequence" :
					    "non-hex character seen", emax);
					FREE(mtype, bdata);
					return (-1);
				}
				bdata[blen] |= HEXVAL(*ascii) << i;
				ascii++;
			}
		}
		goto done;
	}

	/* Get decoding filter */
	if ((decoder = b64_decoder_create(charmap, 1)) == NULL)
		return (-1);

	/* Decode data */
	blen = filter_process(decoder, ascii, strlen(ascii), 1, &bdata, mtype);
	filter_destroy(&decoder);
	if (blen == -1) {
		strlcpy(ebuf, "invalid encoded binary data", emax);
		return (-1);
	}

done:
	/* Fill in structure */
	d->length = blen;
	d->data = bdata;
	return (0);
}

void
structs_data_free(const struct structs_type *type, void *data)
{
	const char *const mtype = type->args[1].v;
	struct structs_data *const d = data;

	FREE(mtype, d->data);
	memset(d, 0, sizeof(*d));
}

int
structs_data_encode(const struct structs_type *type, const char *mtype,
	struct structs_data *code, const void *data)
{
	const struct structs_data *const d = data;
	u_int32_t elength;

	if ((code->data = MALLOC(mtype, 4 + d->length)) == NULL)
		return (-1);
	elength = htonl(d->length);
	memcpy(code->data, &elength, 4);
	memcpy(code->data + 4, d->data, d->length);
	code->length = 4 + d->length;
	return (0);
}

int
structs_data_decode(const struct structs_type *type, const u_char *code,
	size_t cmax, void *data, char *ebuf, size_t emax)
{
	const char *const mtype = type->args[1].v;
	struct structs_data *const d = data;
	u_int32_t elength;

	if (cmax < 4)
		goto bogus;
	memcpy(&elength, code, 4);
	d->length = ntohl(elength);
	if (cmax < d->length) {
bogus:		strlcpy(ebuf, "encoded data is corrupted", emax);
		errno = EINVAL;
		return (-1);
	}
	if ((d->data = MALLOC(mtype, d->length)) == NULL)
		return (-1);
	memcpy(d->data, code + 4, d->length);
	return (4 + d->length);
}

const struct structs_type structs_type_data =
	STRUCTS_DATA_TYPE(NULL, STRUCTS_DATA_MEMTYPE);

const struct structs_type structs_type_hexdata =
	STRUCTS_DATA_TYPE("", STRUCTS_DATA_MEMTYPE);

/*********************************************************************
		    FIXED LENGTH BINARY DATA TYPE
*********************************************************************/

char *
structs_fixeddata_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const u_char *bytes = data;
	char *s;
	int i;

	if ((s = MALLOC(mtype, type->size * 2 + 1)) == NULL)
		return (NULL);
	for (i = 0; i < type->size; i++) {
		s[i * 2] = hexchars[(bytes[i] >> 4) & 0x0f];
		s[i * 2 + 1] = hexchars[bytes[i] & 0x0f];
	}
	s[i * 2] = '\0';
	return (s);
}

int
structs_fixeddata_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	u_char *bytes = data;
	int i;
	int j;

	for (i = 0; i < type->size; i++) {
		bytes[i] = 0;
		for (j = 4; j >= 0; j -= 4) {
			while (isspace(*ascii))
				ascii++;
			if (!isxdigit(*ascii)) {
				strlcpy(ebuf, *ascii == '\0' ?
				    "hex string is too short" :
				    "non-hex character seen", emax);
				goto fail;
			}
			bytes[i] |= HEXVAL(*ascii) << j;
			ascii++;
		}
	}
	while (isspace(*ascii))
		ascii++;
	if (*ascii != '\0') {
		strlcpy(ebuf, "hex string is too long", emax);
fail:		errno = EINVAL;
		return (-1);
	}
	return (0);
}

int
structs_fixeddata_encode(const struct structs_type *type, const char *mtype,
	struct structs_data *code, const void *data)
{
	if ((code->data = MALLOC(mtype, type->size)) == NULL)
		return (-1);
	memcpy(code->data, data, type->size);
	code->length = type->size;
	return (0);
}

int
structs_fixeddata_decode(const struct structs_type *type, const u_char *code,
	size_t cmax, void *data, char *ebuf, size_t emax)
{
	if (cmax < type->size) {
		strlcpy(ebuf, "encoded data is truncated", emax);
		errno = EINVAL;
		return (-1);
	}
	memcpy(data, code, type->size);
	return (type->size);
}

