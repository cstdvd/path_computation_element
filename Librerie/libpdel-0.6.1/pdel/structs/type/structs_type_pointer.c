
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/pointer.h"
#include "util/typed_mem.h"

/*********************************************************************
			POINTER TYPE
*********************************************************************/

int
structs_pointer_init(const struct structs_type *type, void *data)
{
	const struct structs_type *const ptype = type->args[0].v;
	const char *const mtype = type->args[1].v;
	void *pdata;

	/* Create a new instance of the pointed to thing */
	if ((pdata = MALLOC(mtype, ptype->size)) == NULL)
		return (-1);

	/* Initialize it */
	if ((*ptype->init)(ptype, pdata) == -1) {
		FREE(mtype, pdata);
		return (-1);
	}

	/* Point to it */
	*((void **)data) = pdata;
	return (0);
}

int
structs_pointer_copy(const struct structs_type *type,
	const void *from, void *to)
{
	const struct structs_type *const ptype = type->args[0].v;
	const char *const mtype = type->args[1].v;
	void *const from_pdata = *((void **)from);
	void *to_pdata;

	/* Create a new instance of the pointed to thing */
	if ((to_pdata = MALLOC(mtype, ptype->size)) == NULL)
		return (-1);

	/* Copy into it */
	if ((*ptype->copy)(ptype, from_pdata, to_pdata) == -1) {
		FREE(mtype, to_pdata);
		return (-1);
	}

	/* Set new pointer to point to it */
	*((void **)to) = to_pdata;
	return (0);
}

int
structs_pointer_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	const struct structs_type *const ptype = type->args[0].v;
	void *const pdata1 = *((void **)v1);
	void *const pdata2 = *((void **)v2);

	return ((*ptype->equal)(ptype, pdata1, pdata2));
}

char *
structs_pointer_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const struct structs_type *const ptype = type->args[0].v;
	void *const pdata = *((void **)data);

	return ((*ptype->ascify)(ptype, mtype, pdata));
}

int
structs_pointer_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const struct structs_type *const ptype = type->args[0].v;
	const char *const mtype = type->args[1].v;
	void *pdata;

	/* Allocate new data area */
	if ((pdata = MALLOC(mtype, ptype->size)) == NULL)
		return (-1);

	/* Binify string into data area */
	if ((*ptype->binify)(ptype, ascii, pdata, ebuf, emax) == -1) {
		FREE(mtype, pdata);
		return (-1);
	}

	/* Set pointer to point to it */
	*((void **)data) = pdata;
	return (0);
}

int
structs_pointer_encode(const struct structs_type *type, const char *mtype,
	struct structs_data *code, const void *data)
{
	const struct structs_type *const ptype = type->args[0].v;
	void *const pdata = *((void **)data);

	return ((*ptype->encode)(ptype, mtype, code, pdata));
}

int
structs_pointer_decode(const struct structs_type *type, const u_char *code,
	size_t cmax, void *data, char *ebuf, size_t emax)
{
	const struct structs_type *const ptype = type->args[0].v;
	const char *const mtype = type->args[1].v;
	void *pdata;
	int r;

	/* Allocate new data area */
	if ((pdata = MALLOC(mtype, ptype->size)) == NULL)
		return (-1);

	/* Decode referent data */
	if ((r = (*ptype->decode)(ptype,
	    code, cmax, pdata, ebuf, emax)) == -1) {
		FREE(mtype, pdata);
		return (-1);
	}

	/* Set pointer to point to it */
	*((void **)data) = pdata;
	return (r);
}

void
structs_pointer_free(const struct structs_type *type, void *data)
{
	const struct structs_type *const ptype = type->args[0].v;
	const char *const mtype = type->args[1].v;
	void *const pdata = *((void **)data);

	/* Recursively free data pointed to by pointer */
	(*ptype->uninit)(ptype, pdata);
	FREE(mtype, pdata);
	*((void **)data) = NULL;
}

