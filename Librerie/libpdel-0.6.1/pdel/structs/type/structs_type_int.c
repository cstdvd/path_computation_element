
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
#include <errno.h>

#include "pdel/pd_base.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/int.h"
#include "util/typed_mem.h"

/*
 * Integral type methods
 *
 * Type-specific arguments:
 *	[int]
 *		0 = char
 *		1 = short
 *		2 = int
 *		3 = long
 *		4 = int8
 *		5 = int16
 *		6 = int32
 *		7 = int64
 *	[int]
 *		0 = unsigned
 *		1 = signed
 *		2 = hex
 */

#define INTTYPES(name, size, arg1)					\
const struct structs_type structs_type_u ## name = {			\
	(size),								\
	"uint",								\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_region_copy,						\
	structs_region_equal,						\
	structs_int_ascify,						\
	structs_int_binify,						\
	structs_region_encode_netorder,					\
	structs_region_decode_netorder,					\
	structs_nothing_free,						\
	{ { (void *)(arg1) }, { (void *)0 } }				\
};									\
const struct structs_type structs_type_ ## name = {			\
	(size),								\
	"int",								\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_region_copy,						\
	structs_region_equal,						\
	structs_int_ascify,						\
	structs_int_binify,						\
	structs_region_encode_netorder,					\
	structs_region_decode_netorder,					\
	structs_nothing_free,						\
	{ { (void *)(arg1) }, { (void *)1 } }				\
};									\
const struct structs_type structs_type_h ## name = {			\
	(size),								\
	"hint",								\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_region_copy,						\
	structs_region_equal,						\
	structs_int_ascify,						\
	structs_int_binify,						\
	structs_region_encode_netorder,					\
	structs_region_decode_netorder,					\
	structs_nothing_free,						\
	{ { (void *)(arg1) }, { (void *)2 } }				\
}

/* Define the types */
INTTYPES(char, sizeof(char), 0);
INTTYPES(short, sizeof(short), 1);
INTTYPES(int, sizeof(int), 2);
INTTYPES(long, sizeof(long), 3);
INTTYPES(int8, sizeof(int8_t), 4);
INTTYPES(int16, sizeof(int16_t), 5);
INTTYPES(int32, sizeof(int32_t), 6);
INTTYPES(int64, sizeof(int64_t), 7);

/*********************************************************************
			INTEGRAL TYPES
*********************************************************************/

char *
structs_int_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const char *const fmts[8][3] = {
	    {	"%u",	"%d",	"0x%02x"	},	/* char */
	    {	"%u",	"%d",	"0x%x"		},	/* short */
	    {	"%u",	"%d",	"0x%x"		},	/* int */
	    {	"%lu",	"%ld",	"0x%lx"		},	/* long */
	    {	"%u",	"%d",	"0x%02x"	},	/* int8_t */
	    {	"%u",	"%d",	"0x%x"		},	/* int16_t */
	    {	"%u",	"%d",	"0x%x"		},	/* int32_t */
	    {	"%qu",	"%qd",	"0x%qx"		},	/* int64_t */
	};
	const char *fmt;
	char buf[32];

	fmt = fmts[type->args[0].i][type->args[1].i];
	switch (type->args[0].i) {
	case 0:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((char *)data) : *((u_char *)data));
		break;
	case 1:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((short *)data) : *((u_short *)data));
		break;
	case 2:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((int *)data) : *((u_int *)data));
		break;
	case 3:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((long *)data) : *((u_long *)data));
		break;
	case 4:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((int8_t *)data) : *((u_int8_t *)data));
		break;
	case 5:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((int16_t *)data) : *((u_int16_t *)data));
		break;
	case 6:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((int32_t *)data) : *((u_int32_t *)data));
		break;
	case 7:
		snprintf(buf, sizeof(buf), fmt, (type->args[1].i == 1) ? 
		    *((int64_t *)data) : *((u_int64_t *)data));
		break;
	default:
		errno = EDOM;
		return (NULL);
	}
	return (STRDUP(mtype, buf));
}

int
structs_int_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	static const long long bounds[][2] = {
		{ SCHAR_MIN,			UCHAR_MAX },
		{ SHRT_MIN,			USHRT_MAX },
		{ INT_MIN,			UINT_MAX },
		{ LONG_MIN,			ULONG_MAX },
		{ -0x7f - 1,			0xff },
		{ -0x7fff - 1,			0xffff },
		{ -0x7fffffff - 1,		0xffffffff },
		{ -0x7fffffffffffffffLL - 1,	0x7fffffffffffffffLL /*XXX*/ },
	};
	const int format = type->args[1].i;
	long long value;
	char *eptr;

	/* Sanity check */
	if (type->args[0].i < 0 || type->args[0].i >= 8) {
		errno = EDOM;
		return (-1);
	}

	/* Parse value */
#if _KERNEL
	value = strtoq(ascii, &eptr, 0);	/* assume quad_t == long long */
#else
	value = strtoll(ascii, &eptr, 0);
#endif
	if (eptr == ascii) {
		strlcpy(ebuf, "invalid integer value", emax);
		errno = EINVAL;
		return (-1);
	}
	while (isspace(*eptr))
		eptr++;
	if (*eptr != '\0') {
		errno = EINVAL;
		return (-1);
	}

	/* Check that value is in bounds XXX fails for LLONG_MAX..ULLONG_MAX */
	if ((errno == ERANGE && (value == LLONG_MIN || value == LLONG_MAX))
	    || (format == 0 && value < 0LL)		/* check unsignedness */
	    || value < bounds[type->args[0].i][0]
	    || value > bounds[type->args[0].i][1]) {
		strlcpy(ebuf, "integer value is out of range", emax);
		errno = EINVAL;
		return (-1);
	}

	/* Set value */
	if (type->args[1].i == 1) {
		switch (type->args[0].i) {
		case 0:
			*((char *)data) = (char)value;
			break;
		case 1:
			*((short *)data) = (short)value;
			break;
		case 2:
			*((int *)data) = (int)value;
			break;
		case 3:
			*((long *)data) = (long)value;
			break;
		case 4:
			*((int8_t *)data) = (int8_t)value;
			break;
		case 5:
			*((int16_t *)data) = (int16_t)value;
			break;
		case 6:
			*((int32_t *)data) = (int32_t)value;
			break;
		case 7:
			*((int64_t *)data) = (int64_t)value;
			break;
		default:
			errno = EDOM;
			return (-1);
		}
	} else {
		switch (type->args[0].i) {
		case 0:
			*((u_char *)data) = (u_char)value;
			break;
		case 1:
			*((u_short *)data) = (u_short)value;
			break;
		case 2:
			*((u_int *)data) = (u_int)value;
			break;
		case 3:
			*((u_long *)data) = (u_long)value;
			break;
		case 4:
			*((u_int8_t *)data) = (u_int8_t)value;
			break;
		case 5:
			*((u_int16_t *)data) = (u_int16_t)value;
			break;
		case 6:
			*((u_int32_t *)data) = (u_int32_t)value;
			break;
		case 7:
			*((u_int64_t *)data) = (u_int64_t)value;
			break;
		default:
			errno = EDOM;
			return (-1);
		}
	}

	/* Done */
	return (0);
}

