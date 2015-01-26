
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "net/if_util.h"

/*
 * Internal functions
 */
static int	if_do_ip(const char *iface, int cmd, struct in_addr ip,
			struct in_addr mask, struct in_addr dest);

/*
 * Add an IP address assignment to a broadcast or p2p interface.
 *
 * Returns -1 and sets errno if there was a problem.
 */
int
if_add_ip_addr(const char *iface, struct in_addr ip,
	struct in_addr mask, struct in_addr dest)
{
	return (if_do_ip(iface, SIOCAIFADDR, ip, mask, dest));
}

/*
 * Remove an IP address assignment from a or p2p broadcast interface.
 *
 * Returns -1 and sets errno if there was a problem.
 */
int
if_del_ip_addr(const char *iface, struct in_addr ip,
	struct in_addr mask, struct in_addr dest)
{
	return (if_do_ip(iface, SIOCDIFADDR, ip, mask, dest));
}

static int
if_do_ip(const char *iface, int cmd, struct in_addr ip,
	struct in_addr mask, struct in_addr dest)
{
	struct in_addr bcast;
	const struct in_addr *ips[] = {
		&ip,
		&mask,
		&bcast
	};
	struct ifaliasreq ifa;
	struct sockaddr_in *const sas[] = {
		(struct sockaddr_in *)(void *)&ifa.ifra_addr,
		(struct sockaddr_in *)(void *)&ifa.ifra_mask,
		(struct sockaddr_in *)(void *)&ifa.ifra_broadaddr
	};
	int sock;
	int rtn;
	int i;

	/* Set up broadcast/point2point destination */
	if (dest.s_addr == 0)
		bcast.s_addr = ip.s_addr | ~mask.s_addr;
	else
		bcast = dest;

	/* Set up message */
	memset(&ifa, 0, sizeof(ifa));
	strncpy(ifa.ifra_name, iface, sizeof(ifa.ifra_name));
	for (i = 0; i < 3; i++) {
		struct sockaddr_in *const sin = sas[i];

		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = *ips[i];
	}

	/* Send message */
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		return (-1);
	rtn = ioctl(sock, cmd, &ifa);
	(void)close(sock);

	/* Done */
	return (rtn);
}

