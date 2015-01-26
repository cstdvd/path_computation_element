
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

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/string.h"
#include "structs/type/union.h"
#include "util/typed_mem.h"

/*********************************************************************
			PUBLIC FUNCTIONS
*********************************************************************/

int
structs_union_set(const struct structs_type *type, const char *name,
	void *data, const char *field_name)
{
	const struct structs_ufield *fields;
	const struct structs_ufield *ofield = NULL;
	const struct structs_ufield *field;
	struct structs_union *un;
	const char *mtype;
	void *new_un;
	int have_old = 1;

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL)
		return (-1);

	/* Sanity check */
	if (type->tclass != STRUCTS_TYPE_UNION) {
		errno = EINVAL;
		return (-1);
	}

	/* Get union info */
	fields = type->args[0].v;
	mtype = type->args[1].s;
	un = data;

	/* Allow 0 initialized union */
	if (un->field_name == NULL &&
	    un->un == NULL) {
	   have_old = 0;
	   goto union_set_new;
	}

	/* Find the old field */
	for (ofield = fields; ofield->name != NULL
	    && strcmp(ofield->name, un->field_name) != 0; ofield++);
	if (ofield->name == NULL) {
		assert(0);
		errno = EINVAL;
		return (-1);
	}

	/* Check if the union is already set to the desired field */
	if (strcmp(un->field_name, field_name) == 0)
		return (0);

 union_set_new:

	/* Find the new field */
	for (field = fields; field->name != NULL
	    && strcmp(field->name, field_name) != 0; field++);
	if (field->name == NULL) {
		errno = ENOENT;
		return (-1);
	}

	/* Create a new union with the new field type */
	if ((new_un = MALLOC(mtype, field->type->size)) == NULL)
		return (-1);
	if ((*field->type->init)(field->type, new_un) == -1) {
		FREE(mtype, new_un);
		return (-1);
	}

	/* Replace existing union with new one having desired type */
	if (have_old) {
	   (*ofield->type->uninit)(ofield->type, un->un);
	   FREE(mtype, un->un);
	}
	un->un = new_un;


	/* Set field name */
	*((const char **)&un->field_name) = field->name;
	return (0);
}

/*********************************************************************
			UNION TYPE METHODS
*********************************************************************/

int
structs_union_init(const struct structs_type *type, void *data)
{
	const struct structs_ufield *const field = type->args[0].v;
	const char *const mtype = type->args[1].s;
	struct structs_union *const un = data;

	/* Sanity */
	if (field->name == NULL) {
		errno = EINVAL;
		return (-1);
	}
	assert(type->tclass == STRUCTS_TYPE_UNION);

	/* Allocate union memory */
	if ((un->un = MALLOC(mtype, field->type->size)) == NULL)
		return (-1);

	/* Initialize field using first member type */
	if ((*field->type->init)(field->type, un->un) == -1) {
		FREE(mtype, un->un);
		return (-1);
	}

	/* Set field name */
	*((const char **)&un->field_name) = field->name;
	return (0);
}

int
structs_union_copy(const struct structs_type *type, const void *from, void *to)
{
	const struct structs_union *const fun = from;
	const struct structs_ufield *const fields = type->args[0].v;
	const struct structs_ufield *field;
	const char *const mtype = type->args[1].s;
	struct structs_union *const tun = to;

	/* Sanity check */
	assert(type->tclass == STRUCTS_TYPE_UNION);

	/* Find field */
	for (field = fields; field->name != NULL
	    && strcmp(fun->field_name, field->name) != 0; field++);
	if (field->name == NULL) {
		assert(0);
		errno = EINVAL;
		return (-1);
	}

	/* Allocate copy union */
	if ((tun->un = MALLOC(mtype, field->type->size)) == NULL)
		return (-1);

	/* Copy field */
	if ((*field->type->copy)(field->type, fun->un, tun->un) == -1) {
		FREE(mtype, tun->un);
		return (-1);
	}

	/* Set field name */
	*((const char **)&tun->field_name) = fun->field_name;
	return (0);
}

int
structs_union_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	const struct structs_union *const un1 = v1;
	const struct structs_union *const un2 = v2;
	const struct structs_ufield *const fields = type->args[0].v;
	const struct structs_ufield *field;

	/* Sanity check */
	assert(type->tclass == STRUCTS_TYPE_UNION);

	/* Check if fields are the same */
	if (strcmp(un1->field_name, un2->field_name) != 0)
		return (0);

	/* Find field */
	for (field = fields; field->name != NULL
	    && strcmp(un1->field_name, field->name) != 0; field++);
	if (field->name == NULL) {
		assert(0);
		errno = EINVAL;
		return (-1);
	}

	/* Compare them */
	return ((*field->type->equal)(field->type, un1->un, un2->un));
}

int
structs_union_encode(const struct structs_type *type, const char *mtype,
	struct structs_data *code, const void *data)
{
	const struct structs_union *const un = data;
	const struct structs_ufield *const fields = type->args[0].v;
	const struct structs_ufield *field;
	struct structs_data ncode;
	struct structs_data fcode;

	/* Sanity check */
	assert(type->tclass == STRUCTS_TYPE_UNION);

	/* Find field */
	for (field = fields; field->name != NULL
	    && strcmp(un->field_name, field->name) != 0; field++);
	if (field->name == NULL) {
		assert(0);
		errno = EINVAL;
		return (-1);
	}

	/* Encode name */
	if (structs_string_encode(&structs_type_string,
	    TYPED_MEM_TEMP, &ncode, &field->name) == -1)
		return (-1);

	/* Encode field */
	if ((*field->type->encode)(field->type,
	    TYPED_MEM_TEMP, &fcode, un->un) == -1) {
		FREE(TYPED_MEM_TEMP, ncode.data);
		return (-1);
	}

	/* Allocate code buffer */
	code->length = ncode.length + fcode.length;
	if ((code->data = MALLOC(mtype, code->length)) == NULL) {
		FREE(TYPED_MEM_TEMP, fcode.data);
		FREE(TYPED_MEM_TEMP, ncode.data);
		return (-1);
	}

	/* Copy encoded name and field */
	memcpy(code->data, ncode.data, ncode.length);
	memcpy(code->data + ncode.length, fcode.data, fcode.length);

	/* Done */
	FREE(TYPED_MEM_TEMP, ncode.data);
	FREE(TYPED_MEM_TEMP, fcode.data);
	return (0);
}

int
structs_union_decode(const struct structs_type *type, const u_char *code,
	size_t cmax, void *data, char *ebuf, size_t emax)
{
	const struct structs_ufield *const fields = type->args[0].v;
	const char *const mtype = type->args[1].s;
	struct structs_union *const un = data;
	const struct structs_ufield *field;
	char *field_name;
	int nlen;
	int flen;

	/* Sanity check */
	assert(type->tclass == STRUCTS_TYPE_UNION);

	/* Decode field name */
	if ((nlen = structs_string_decode(&structs_type_string,
	    code, cmax, &field_name, ebuf, emax)) == -1)
		return (-1);

	/* Find field */
	for (field = fields; field->name != NULL
	    && strcmp(field_name, field->name) != 0; field++);
	if (field->name == NULL) {
		snprintf(ebuf, emax, "unknown union field \"%s\"", field_name);
		FREE(STRUCTS_TYPE_STRING_MTYPE, field_name);
		return (-1);
	}
	FREE(STRUCTS_TYPE_STRING_MTYPE, field_name);

	/* Allocate field memory */
	if ((un->un = MALLOC(mtype, field->type->size)) == NULL) {
		memset(un, 0, sizeof(*un));
		return (-1);
	}

	/* Decode field */
	if ((flen = (*field->type->decode)(field->type,
	    code + nlen, cmax - nlen, un->un, ebuf, emax)) == -1) {
		FREE(mtype, un->un);
		memset(un, 0, sizeof(*un));
		return (-1);
	}

	/* Set field name */
	*((const char **)&un->field_name) = field->name;
	return (nlen + flen);
}

void
structs_union_free(const struct structs_type *type, void *data)
{
	const char *const mtype = type->args[1].s;
	const struct structs_union *const un = data;
	const struct structs_ufield *const fields = type->args[0].v;
	const struct structs_ufield *field;

	/* Sanity check */
	assert(type->tclass == STRUCTS_TYPE_UNION);

	/* Find field */
	for (field = fields; field->name != NULL
	    && strcmp(un->field_name, field->name) != 0; field++);
	if (field->name == NULL) {
		assert(0);
		return;
	}

	/* Free it */
	(*field->type->uninit)(field->type, un->un);
	FREE(mtype, un->un);
}

