
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include "structs/structs.h"
#include "structs/type/null.h"

/*********************************************************************
			NULL TYPE
*********************************************************************/

const struct structs_type structs_type_null = {
	0,
	"null",
	STRUCTS_TYPE_PRIMITIVE,
	structs_notsupp_init,
	structs_notsupp_copy,
	structs_notsupp_equal,
	structs_notsupp_ascify,
	structs_notsupp_binify,
	structs_notsupp_encode,
	structs_notsupp_decode,
	structs_nothing_free,
};

