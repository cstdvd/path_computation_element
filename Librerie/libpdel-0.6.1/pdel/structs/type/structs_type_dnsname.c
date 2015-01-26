
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/socket.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "pdel/pd_port.h"
#include "pdel/pd_string.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/ip4.h"
#include "structs/type/struct.h"
#include "structs/type/string.h"
#include "structs/type/dnsname.h"
#include "util/typed_mem.h"

/*********************************************************************
			DNS NAME TYPE
*********************************************************************/

#define IPS_MEM_TYPE	"structs_dnsname_ip"

/* Type for 'struct structs_dnsname' if it were a plain structure */
static const struct structs_type structs_dnsname_ips_type
	= STRUCTS_ARRAY_TYPE(&structs_type_ip4, IPS_MEM_TYPE, "ip");
static const struct structs_field structs_dnsname_fields[] = {
	STRUCTS_STRUCT_FIELD(structs_dnsname, name, &structs_type_string),
	STRUCTS_STRUCT_FIELD(structs_dnsname, ips, &structs_dnsname_ips_type),
	STRUCTS_STRUCT_FIELD_END
};
static const struct structs_type structs_dnsname_internal_type
	= STRUCTS_STRUCT_TYPE(structs_dnsname, &structs_dnsname_fields);

/* "structs_type_dnsname" methods */
static structs_init_t		structs_dnsname_init;
static structs_copy_t		structs_dnsname_copy;
static structs_equal_t		structs_dnsname_equal;
static structs_ascify_t		structs_dnsname_ascify;
static structs_binify_t		structs_dnsname_binify;
static structs_encode_t		structs_dnsname_encode;
static structs_decode_t		structs_dnsname_decode;
static structs_uninit_t		structs_dnsname_free;

/* Public structs type "structs_type_dnsname" */
const struct structs_type structs_type_dnsname = {
	sizeof(struct structs_dnsname),
	"dnsname",
	STRUCTS_TYPE_PRIMITIVE,
	structs_dnsname_init,
	structs_dnsname_copy,
	structs_dnsname_equal,
	structs_dnsname_ascify,
	structs_dnsname_binify,
	structs_dnsname_encode,
	structs_dnsname_decode,
	structs_dnsname_free
};

static int
structs_dnsname_init(const struct structs_type *type, void *data)
{
	return (structs_struct_init(&structs_dnsname_internal_type, data));
}

static int
structs_dnsname_copy(const struct structs_type *type,
	const void *from, void *to)
{
	return (structs_struct_copy(&structs_dnsname_internal_type, from, to));
}

static int
structs_dnsname_equal(const struct structs_type *type,
	const void *v1, const void *v2)
{
	return (structs_struct_equal(&structs_dnsname_internal_type, v1, v2));
}

static char *
structs_dnsname_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	return (structs_get_string(&structs_dnsname_internal_type,
	    "name", data, mtype));
}

static int
structs_dnsname_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	struct structs_dnsname *const dn = data;
#ifdef __CYGWIN__
	struct hostent *res;
	char **p;
#else
	struct addrinfo hints, *res, *p;
	int error;
#endif
	int i;

	/* Initialize structure */
	if (structs_init(type, NULL, data) == -1)
		return (-1);

	/* Copy ASCII string */
	if (structs_set_string(&structs_dnsname_internal_type,
	    "name", ascii, data, ebuf, emax) == -1)
		goto fail1;

	/* Allow empty string, which is our default value */
	if (*ascii == '\0')
		return (0);

	/* Perform DNS lookup to get the IP addresses */
#ifndef __CYGWIN__
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	if ((error = getaddrinfo(ascii, NULL, &hints, &res)) != 0) {
		snprintf(ebuf, emax, "%s: %s", ascii, gai_strerror(error));
		goto fail1;
	}

	/* Fill in results array */
	for (i = 0, p = res; p != NULL; i++, p = p->ai_next);
	if ((dn->ips.elems = REALLOC(IPS_MEM_TYPE,
	    dn->ips.elems, i * sizeof(*dn->ips.elems))) == NULL)
		goto fail2;
	for (i = 0, p = res; p != NULL; i++, p = p->ai_next) {
		dn->ips.elems[i] = ((struct sockaddr_in *)
		    (void *)p->ai_addr)->sin_addr;
	}
	dn->ips.length = i;

	/* Cleanup */
	freeaddrinfo(res);
#else
	if (NULL != (res = gethostbyname(ascii))) {
		snprintf(ebuf, emax, "%s: lookup failed-%s", 
			 ascii, strerror(errno));
		goto fail1;
	}
	/* Fill in results array */
	for (i = 0, p = res->h_addr_list; p[i] != NULL; i++);
	if ((dn->ips.elems = REALLOC(IPS_MEM_TYPE,
	    dn->ips.elems, i * sizeof(*dn->ips.elems))) == NULL)
		goto fail2;
	for (i = 0, p = res->h_addr_list; p[i] != NULL ; i++) {
		dn->ips.elems[i] = ((struct sockaddr_in *)(p[i]))->sin_addr;
	}
#endif

	/* Done */
	return (0);

	/* Clean up after failure */
fail2:
#ifndef __CYGWIN__
	freeaddrinfo(res);
#endif
fail1:	structs_free(type, NULL, data);
	return (-1);
}

static int
structs_dnsname_encode(const struct structs_type *type, const char *mtype,
	struct structs_data *code, const void *data)
{
	return (structs_struct_encode(&structs_dnsname_internal_type,
	    mtype, code, data));
}

static int
structs_dnsname_decode(const struct structs_type *type,
	const u_char *code, size_t cmax, void *data, char *ebuf, size_t emax)
{
	return (structs_struct_decode(&structs_dnsname_internal_type,
	    code, cmax, data, ebuf, emax));
}

static void
structs_dnsname_free(const struct structs_type *type, void *data)
{
	structs_struct_free(&structs_dnsname_internal_type, data);
}

