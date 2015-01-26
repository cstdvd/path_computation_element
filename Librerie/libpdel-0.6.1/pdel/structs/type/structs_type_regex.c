
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "pdel/pd_regex.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/string.h"
#include "structs/type/regex.h"
#include "util/typed_mem.h"

/* Pre-defined types */
const struct structs_type structs_type_regex =
	STRUCTS_REGEX_TYPE(STRUCTS_REGEX_MTYPE, REG_EXTENDED);
const struct structs_type structs_type_regex_icase =
	STRUCTS_REGEX_TYPE(STRUCTS_REGEX_MTYPE, REG_EXTENDED | REG_ICASE);

int
structs_regex_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	const struct structs_regex *const r1 = v1;
	const struct structs_regex *const r2 = v2;

	if (r1->pat == NULL)
		return (r2->pat == NULL);
	if (r2->pat == NULL)
		return (0);
	return (strcmp(r1->pat, r2->pat) == 0);
}

char *
structs_regex_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const struct structs_regex *const r = data;

	return (STRDUP(mtype, (r->pat != NULL) ? r->pat : ""));
}

int
structs_regex_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	struct structs_regex *const r = data;
	const char *mtype = type->args[0].s;
	const int flags = type->args[1].i;
	int errno_save;
	int err;

	/* Empty string? */
	if (*ascii == '\0') {
		memset(r, 0, sizeof(*r));
		return (0);
	}

	/* Compile pattern */
	if ((err = pd_regcomp(&r->reg, ascii, flags)) != 0) {
		pd_regerror(err, &r->reg, ebuf, emax);
		switch (err) {
		case REG_ESPACE:
			errno = ENOMEM;
			break;
		default:
			errno = EINVAL;
			break;
		}
		return (-1);
	}

	/* Save a copy of the pattern string */
	if ((r->pat = STRDUP(mtype, ascii)) == NULL) {
		errno_save = errno;
		pd_regfree(&r->reg);
		errno = errno_save;
		return (-1);
	}

	/* OK */
	return (0);
}

void
structs_regex_free(const struct structs_type *type, void *data)
{
	const char *mtype = type->args[0].s;
	struct structs_regex *const r = data;

	if (r->pat != NULL) {
		FREE(mtype, (char *)r->pat);
		pd_regfree(&r->reg);
		memset(r, 0, sizeof(*r));
	}
}

