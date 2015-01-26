
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/ether.h"
#include "util/typed_mem.h"

/*********************************************************************
			ETHERNET ADDRESS TYPE
*********************************************************************/

static structs_ascify_t		structs_ether_ascify;
static structs_binify_t		structs_ether_binify;

static char *
structs_ether_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const int colons = type->args[0].i;
	const char *fmt = colons ?
	    "%02x:%02x:%02x:%02x:%02x:%02x" : "%02x%02x%02x%02x%02x%02x";
	const u_char *const ether = data;
	char buf[18];

	snprintf(buf, sizeof(buf), fmt,
	    ether[0], ether[1], ether[2], ether[3], ether[4], ether[5]);
	return (STRDUP(mtype, buf));
}

static int
structs_ether_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const int colons = type->args[0].i;
	int ether[6];
	int i;

	if (colons) {
		if (sscanf(ascii, "%x:%x:%x:%x:%x:%x",
		    &ether[0], &ether[1], &ether[2],
		    &ether[3], &ether[4], &ether[5]) != 6) {
			strlcpy(ebuf, "invalid Ethernet address", emax);
			errno = EINVAL;
			return (-1);
		}
	} else {
		while (isspace(*ascii))
			ascii++;
		for (i = 0; i < 6; i++) {
			char buf[3];

			if (!isxdigit(ascii[i * 2])
			    || !isxdigit(ascii[i * 2 + 1])) {
				errno = EINVAL;
				return (-1);
			}
			buf[0] = ascii[i * 2];
			buf[1] = ascii[i * 2 + 1];
			buf[2] = '\0';
			if (sscanf(buf, "%x", &ether[i]) != 1) {
				errno = EINVAL;
				return (-1);
			}
		}
	}
	for (i = 0; i < 6; i++)
		((u_char *)data)[i] = (u_char)ether[i];
	return (0);
}

const struct structs_type structs_type_ether = {
	ETHER_ADDR_LEN,
	"ether",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_ether_ascify,
	structs_ether_binify,
	structs_region_encode,
	structs_region_decode,
	structs_nothing_free,
	{ { (void *)1 } }
};

const struct structs_type structs_type_ether_nocolon = {
	ETHER_ADDR_LEN,
	"ether",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_ether_ascify,
	structs_ether_binify,
	structs_region_encode,
	structs_region_decode,
	structs_nothing_free,
	{ { (void *)0 } }
};

