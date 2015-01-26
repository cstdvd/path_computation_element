
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
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/float.h"
#include "util/typed_mem.h"

/*
 * Float type methods
 *
 * Type-specific arguments:
 *	[int]
 *		0 = float
 *		1 = double
 */

static structs_equal_t	structs_float_equal;
static structs_ascify_t	structs_float_ascify;
static structs_binify_t	structs_float_binify;

#define FTYPE_FLOAT		0
#define FTYPE_DOUBLE		1

#define STRUCTS_TYPE_FLOAT(type, ftype)					\
const struct structs_type structs_type_ ## type = {			\
	sizeof(type),							\
	#type,								\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_region_copy,						\
	structs_float_equal,						\
	structs_float_ascify,						\
	structs_float_binify,						\
	structs_region_encode_netorder,					\
	structs_region_decode_netorder,					\
	structs_nothing_free,						\
	{ { (void *)(ftype) } }						\
}									\

/* Define the types */
STRUCTS_TYPE_FLOAT(float, FTYPE_FLOAT);
STRUCTS_TYPE_FLOAT(double, FTYPE_DOUBLE);

/*********************************************************************
			FLOAT TYPE METHODS
*********************************************************************/

int
structs_float_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	const int ftype = type->args[0].i;

	switch (ftype) {
	case FTYPE_FLOAT:
	    {
		const float *f1 = (const float *)v1;
		const float *f2 = (const float *)v2;

		return (*f1 == *f2);
	    }
	case FTYPE_DOUBLE:
	    {
		const double *d1 = (const double *)v1;
		const double *d2 = (const double *)v2;

		return (*d1 == *d2);
	    }
	default:
		assert(0);
		return (-1);
	}
}

char *
structs_float_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const int ftype = type->args[0].i;
	char buf[32];

	switch (ftype) {
	case FTYPE_FLOAT:
	    {
		const float *f = (const float *)data;

		snprintf(buf, sizeof(buf), "%.16g", *f);
		break;
	    }
	case FTYPE_DOUBLE:
	    {
		const double *d = (const double *)data;

		snprintf(buf, sizeof(buf), "%.16g", *d);
		break;
	    }
	default:
		assert(0);
		return (NULL);
	}
	return (STRDUP(mtype, buf));
}

int
structs_float_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const int ftype = type->args[0].i;
	double value;
	double temp;
	char *eptr;

	/* Parse value */
	errno = 0;
	value = strtod(ascii, &eptr);
	if (eptr == ascii)
		goto invalid;
	while (isspace(*eptr))
		eptr++;
	if (*eptr != '\0') {
invalid:	strlcpy(ebuf, "invalid floating point value", emax);
		errno = EINVAL;
		return (-1);
	}

	/* Check for overflow/underflow */
	if (errno == ERANGE) {
		if (value == 0.0) {
underflow:		strlcpy(ebuf, "floating point value underflow", emax);
			errno = EINVAL;
			return (-1);
		}
		if (value == HUGE_VAL || value == -HUGE_VAL) {
overflow:		strlcpy(ebuf, "floating point value overflow", emax);
			errno = EINVAL;
			return (-1);
		}
	}
	switch (ftype) {
	case FTYPE_FLOAT:
		if (value == 0.0)
			break;
		temp = (value < 0) ? -value : value;
		if (temp < 5.877473155409902e-39)
			goto underflow;
		if (temp > 3.4028234663852886e+38)
			goto overflow;
		*(float *)data = (float)value;
		break;
	case FTYPE_DOUBLE:
		*(double *)data = value;
		break;
	}

	/* Done */
	return (0);
}

