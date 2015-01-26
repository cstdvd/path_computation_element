
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_DNSNAME_H_
#define _PDEL_STRUCTS_TYPE_DNSNAME_H_

/*********************************************************************
			    DNS NAME TYPE
*********************************************************************/

/*
 * The data is a variable of type "struct structs_dnsname".
 * This structure contains both an ASCII DNS name and a list
 * of one or more IP addresses.
 *
 * The ASCII representation is the DNS name in the "name" field.
 * One or more IP addresses resolved by DNS are stored in the "ips"
 * array field. When converting from ASCII -> binary, a DNS lookup
 * of the ASCII string is performed.
 */

DEFINE_STRUCTS_ARRAY(structs_dnsname_ips, struct in_addr);

struct structs_dnsname {
	const char			*name;	/* dns name to look up */
	struct structs_dnsname_ips	ips;	/* array of >= 1 ip addresses */
};

__BEGIN_DECLS

PD_IMPORT const struct structs_type	structs_type_dnsname;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_DNSNAME_H_ */

