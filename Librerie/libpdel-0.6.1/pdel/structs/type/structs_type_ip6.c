
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
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "pdel/pd_inet.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/ip6.h"
#include "util/typed_mem.h"

/*********************************************************************
			IPv6 ADDRESS TYPE
*********************************************************************/

static structs_ascify_t		structs_ip6_ascify;
static structs_binify_t		structs_ip6_binify;

static char *
structs_ip6_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	char *res;

	if ((res = MALLOC(mtype, INET6_ADDRSTRLEN + 1)) != NULL)
		pd_inet_ntop(AF_INET6, data, res, INET6_ADDRSTRLEN);
	return (res);
}

static int
structs_ip6_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	switch (pd_inet_pton(AF_INET6, ascii, data)) {
	case 0:
		strlcpy(ebuf, "invalid IPv6 address", emax);
		errno = EINVAL;
		break;
	case 1:
		return (0);
	default:
		break;
	}
	return (-1);
}

const struct structs_type structs_type_ip6 = {
	sizeof(struct in6_addr),
	"ip6",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_ip6_ascify,
	structs_ip6_binify,
	structs_region_encode,
	structs_region_decode,
	structs_nothing_free,
};

