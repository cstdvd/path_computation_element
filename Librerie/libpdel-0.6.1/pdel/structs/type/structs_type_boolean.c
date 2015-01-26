
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
#include <errno.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/boolean.h"
#include "util/typed_mem.h"

/*********************************************************************
			BOOLEAN TYPES
*********************************************************************/

/*
 * Boolean types
 *
 * Type-specific arguments:
 *	[int]
 *		0 = stored in a char
 *		1 = stored in an int
 *	[int]
 *		0 = True/False
 *		1 = Yes/No
 *		2 = On/Off
 *		3 = Enabled/Disabled
 *		4 = 1/0
 */

/* Macro for defining a boolean type */
#define STRUCTS_BOOLEAN_TYPE(type, arg1, arg2) {			\
	sizeof(type),							\
	"boolean",							\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_region_copy,						\
	structs_region_equal,						\
	structs_boolean_ascify,						\
	structs_boolean_binify,						\
	structs_region_encode_netorder,					\
	structs_region_decode_netorder,					\
	structs_nothing_free,						\
	{ { (void *)arg1 }, { (void *)arg2 } }				\
}

/* ASCII possibilities (not all are used yet) */
static const char *boolean_strings[][2] = {
	{ "False", 	"True"		},
	{ "No", 	"Yes"		},
	{ "Off",	"On"		},
	{ "Disabled",	"Enabled"	},
	{ "0",		"1"		},
	{ NULL,		NULL		}
};

/* Methods */
static structs_ascify_t		structs_boolean_ascify;
static structs_binify_t		structs_boolean_binify;

/* Pre-defined types */
const struct structs_type structs_type_boolean_char
	= STRUCTS_BOOLEAN_TYPE(char, 0, 0);
const struct structs_type structs_type_boolean_int
	= STRUCTS_BOOLEAN_TYPE(int, 1, 0);
const struct structs_type structs_type_boolean_char_01
	= STRUCTS_BOOLEAN_TYPE(char, 0, 4);
const struct structs_type structs_type_boolean_int_01
	= STRUCTS_BOOLEAN_TYPE(int, 1, 4);

static char *
structs_boolean_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	int truth;

	truth = type->args[0].i ? *((u_int *)data) : *((u_char *)data);
	return (STRDUP(mtype, boolean_strings[type->args[1].i][!!truth]));
}

static int
structs_boolean_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	int truth = 0;
	char buf[64];
	int i;

	/* Trim whitespace */
	while (isspace(*ascii))
		ascii++;
	for (i = strlen(ascii); i > 0 && isspace(ascii[i - 1]); i--);
	if (i > sizeof(buf) - 1)
		i = sizeof(buf) - 1;
	strncpy(buf, ascii, i);
	buf[i] = '\0';

	/* Compare value */
	for (i = 0; boolean_strings[i][0] != NULL; i++) {
		if (strcasecmp(buf, boolean_strings[i][0]) == 0) {
			truth = 0;
			break;
		} else if (strcasecmp(buf, boolean_strings[i][1]) == 0) {
			truth = 1;
			break;
		}
	}
	if (boolean_strings[i][0] == NULL) {
		strlcpy(ebuf, "invalid Boolean value", emax);
		errno = EINVAL;
		return (-1);
	}
	if (type->args[0].i)
		*((u_int *)data) = truth;
	else
		*((u_char *)data) = truth;
	return (0);
}

