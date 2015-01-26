
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
#include <assert.h>
#include <errno.h>

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"
#endif
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/id.h"
#include "util/typed_mem.h"

/*********************************************************************
			IDENTIFIER TYPES
*********************************************************************/

int
structs_id_init(const struct structs_type *type, void *data)
{
	const struct structs_id *const ids = type->args[0].v;

	switch (type->size) {
	case 1:
	    {
		const u_int8_t value = (u_int8_t)ids[0].value;

		memcpy(data, &value, type->size);
		break;
	    }
	case 2:
	    {
		const u_int16_t value = (u_int16_t)ids[0].value;

		memcpy(data, &value, type->size);
		break;
	    }
	case 4:
	    {
		const u_int32_t value = (u_int32_t)ids[0].value;

		memcpy(data, &value, type->size);
		break;
	    }
	default:
		assert(0);
	}
	return (0);
}

char *
structs_id_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const struct structs_id *id;
	u_int32_t value = 0;

	switch (type->size) {
	case 1:
		value = *((u_int8_t *)data);
		break;
	case 2:
		value = *((u_int16_t *)data);
		break;
	case 4:
		value = *((u_int32_t *)data);
		break;
	default:
		assert(0);
	}
	for (id = type->args[0].v; id->id != NULL; id++) {
		if (value == id->value)
			return (STRDUP(mtype, id->id));
	}
	return (STRDUP(mtype, "INVALID"));
}

int
structs_id_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const struct structs_id *id;

	for (id = type->args[0].v; id->id != NULL; id++) {
		int (*const cmp)(const char *, const char *)
		    = id->imatch ? strcasecmp : strcmp;

		if ((*cmp)(ascii, id->id) == 0) {
			switch (type->size) {
			case 1:
				*((u_int8_t *)data) = id->value;
				break;
			case 2:
				*((u_int16_t *)data) = id->value;
				break;
			case 4:
				*((u_int32_t *)data) = id->value;
				break;
			default:
				assert(0);
			}
			return (0);
		}
	}
	snprintf(ebuf, emax, "invalid value \"%s\"", ascii);
	errno = EINVAL;
	return (-1);
}

