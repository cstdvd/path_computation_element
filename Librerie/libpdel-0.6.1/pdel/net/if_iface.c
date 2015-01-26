
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

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/typed_mem.h"
#include "net/if_util.h"

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static struct	if_msghdr *if_find(const char *ifname,
			u_char **bufp, const char *mtype);
static int	if_info(u_char **bufp, const char *mtype);

/*
 * Get list of IP addresses and netmasks configured on an interface.
 */
int
if_get_ip_addrs(const char *ifname, struct in_addr **iplistp,
	struct in_addr **nmlistp, const char *mtype)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct in_addr *iplist = NULL;
	struct in_addr *nmlist = NULL;
	int errno_save;
	int num = 0;
	u_char *buf;

	/* Find interface info */
	if ((ifm = if_find(ifname, &buf, TYPED_MEM_TEMP)) == NULL)
		return (-1);

	/* Search for first IP address */
	for (ifam = (struct ifa_msghdr *)(void *)
	      ((u_char *)ifm + ifm->ifm_msglen);
	    ifam->ifam_type == RTM_NEWADDR;
	    ifam = (struct ifa_msghdr *)(void *)
	      ((u_char *)ifam + ifam->ifam_msglen))
		num++;

	/* Allocate arrays */
	if ((iplist = MALLOC(mtype, num * sizeof(*iplist))) == NULL
	    || (nmlist = MALLOC(mtype, num * sizeof(*nmlist))) == NULL) {
		errno_save = errno;
		FREE(mtype, iplist);				/* ok if NULL */
		FREE(TYPED_MEM_TEMP, buf);
		errno = errno_save;
		return (-1);
	}

	/* Search for IP address/netmask combinations */
	num = 0;
	for (ifam = (struct ifa_msghdr *)(void *)
	     ((u_char *)ifm + ifm->ifm_msglen);
	    ifam->ifam_type == RTM_NEWADDR;
	    ifam = (struct ifa_msghdr *)(void *)
	      ((u_char *)ifam + ifam->ifam_msglen)) {
		char *cp = (char *)(ifam + 1);
		int need = RTA_IFA | RTA_NETMASK;
		int i;

		/* Find IP address and netmask, if any */
		if ((ifam->ifam_addrs & need) != need)
			continue;
		for (i = 1; i != 0 && need != 0; i <<= 1) {
			if ((ifam->ifam_addrs & i) == 0)
				continue;
			if (i == RTA_IFA
			    && ((struct sockaddr *)(void *)cp)->sa_family
			      == AF_INET) {
				iplist[num] = ((struct sockaddr_in *)
				    (void *)cp)->sin_addr;
			} else if (i == RTA_NETMASK) {
				nmlist[num] = ((struct sockaddr_in *)
				    (void *)cp)->sin_addr;
			}
			need &= ~i;
			ADVANCE(cp, (struct sockaddr *)cp);
		}
		if (need == 0)
			num++;
	}

	/* Done */
	FREE(TYPED_MEM_TEMP, buf);
	*iplistp = iplist;
	*nmlistp = nmlist;
	return (num);
}

/*
 * Get the first IP address on an interface.
 */
int
if_get_ip_addr(const char *ifname, struct in_addr *ipp, struct in_addr *nmp)
{
	struct in_addr *iplist;
	struct in_addr *nmlist;
	int nip;

	if ((nip = if_get_ip_addrs(ifname,
	    &iplist, &nmlist, TYPED_MEM_TEMP)) == -1)
		return (-1);
	if (nip == 0) {
		FREE(TYPED_MEM_TEMP, iplist);
		FREE(TYPED_MEM_TEMP, nmlist);
		errno = ENOENT;
		return (-1);
	}
	if (ipp != NULL)
		*ipp = iplist[0];
	if (nmp != NULL)
		*nmp = nmlist[0];
	FREE(TYPED_MEM_TEMP, iplist);
	FREE(TYPED_MEM_TEMP, nmlist);
	return (0);
}

/*
 * Get the flags associated with an interface.
 */
int
if_get_flags(const char *ifname)
{
	struct if_msghdr *ifm;
	u_char *buf;
	int rtn;

	if ((ifm = if_find(ifname, &buf, TYPED_MEM_TEMP)) == NULL)
		return (-1);
	rtn = ifm->ifm_flags;
	FREE(TYPED_MEM_TEMP, buf);
	return (rtn);
}

/*
 * Set the flags associated with an interface.
 */
int
if_set_flags(const char *ifname, int flags)
{
	struct ifreq ifr;
	int r;
	int s;

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		return (-1);
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_flags = flags;
	r = ioctl(s, SIOCSIFFLAGS, (char *)&ifr);
	(void)close(s);
	return (r);
}

/*
 * Get the MTU associated with an interface.
 */
int
if_get_mtu(const char *ifname)
{
	struct if_msghdr *ifm;
	u_char *buf;
	int rtn;

	if ((ifm = if_find(ifname, &buf, TYPED_MEM_TEMP)) == NULL)
		return (-1);
	rtn = ifm->ifm_data.ifi_mtu;
	FREE(TYPED_MEM_TEMP, buf);
	return (rtn);
}

/*
 * Set the MTU associated with an interface.
 */
int
if_set_mtu(const char *ifname, u_int mtu)
{
	struct ifreq ifr;
	int r;
	int s;

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		return (-1);
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = mtu;
	r = ioctl(s, SIOCSIFMTU, (char *)&ifr);
	(void)close(s);
	return (r);
}

/*
 * Get the interface type.
 */
int
if_get_type(const char *ifname)
{
	struct if_msghdr *ifm;
	u_char *buf;
	int rtn;

	if ((ifm = if_find(ifname, &buf, TYPED_MEM_TEMP)) == NULL)
		return (-1);
	rtn = ifm->ifm_data.ifi_type;
	FREE(TYPED_MEM_TEMP, buf);
	return (rtn);
}

/*
 * Get the link address for an interface.
 */
int
if_get_link_addr(const char *ifname,
	struct sockaddr_dl **sdlp, const char *mtype)
{
	struct if_msghdr *ifm;
	struct sockaddr_dl *sdl;
	int errno_save;
	u_char *buf;

	if ((ifm = if_find(ifname, &buf, TYPED_MEM_TEMP)) == NULL)
		return (-1);
	sdl = (struct sockaddr_dl *)(ifm + 1);
	if ((*sdlp = MALLOC(mtype, sdl->sdl_len)) == NULL) {
		errno_save = errno;
		FREE(TYPED_MEM_TEMP, buf);
		errno = errno_save;
		return (-1);
	}
	memcpy(*sdlp, sdl, sdl->sdl_len);
	FREE(TYPED_MEM_TEMP, buf);
	return (0);
}

/*
 * Gets the names of all system interfaces.
 */
int
if_get_list(char ***listp, const char *mtype)
{
	char **list = NULL;
	int errno_save;
	int num = 0;
	u_char *buf;
	u_char *ptr;
	int len;

	/* Get raw data from kernel */
	if ((len = if_info(&buf, TYPED_MEM_TEMP)) == -1)
		return (-1);

	/* Scan interfaces */
	for (ptr = buf;
	    ptr < buf + len;
	    ptr += ((struct if_msghdr *)(void *)ptr)->ifm_msglen) {
		struct if_msghdr *const ifm = (struct if_msghdr *)(void *)ptr;

		/* Sanity check version */
		if (ifm->ifm_version != RTM_VERSION) {
			errno = EIO;
			goto fail;
		}

		/* Next interface? */
		if (ifm->ifm_type == RTM_IFINFO) {
			struct sockaddr_dl *const sdl
			    = (struct sockaddr_dl *)(ifm + 1);
			char nbuf[IF_NAMESIZE + 1];
			void *new_list;

			/* Get name */
			if (sdl->sdl_nlen > sizeof(nbuf) - 1)
				continue;
			memcpy(nbuf, sdl->sdl_data, sdl->sdl_nlen);
			nbuf[sdl->sdl_nlen] = '\0';

			/* Add name to list */
			if ((new_list = REALLOC(mtype, list,
			    (num + 1) * sizeof(*list))) == NULL)
				goto fail;
			list = new_list;
			if ((list[num] = STRDUP(mtype, nbuf)) == NULL)
				goto fail;
			num++;
		}
	}

	/* Done */
	FREE(TYPED_MEM_TEMP, buf);
	*listp = list;
	return (num);

fail:
	errno_save = errno;
	while (num > 0)
		FREE(mtype, list[--num]);
	FREE(mtype, list);
	FREE(TYPED_MEM_TEMP, buf);
	errno = errno_save;
	return (-1);
}

/*
 * Get the flags or type associated with an interface.
 * Caller must free *bufp.
 *
 * Returns -1 and sets errno if there was a problem.
 */
static struct if_msghdr *
if_find(const char *ifname, u_char **bufp, const char *mtype)
{
	u_char *buf;
	u_char *ptr;
	size_t len;

	/* Get raw data from kernel */
	if ((len = if_info(&buf, mtype)) == -1)
		return (NULL);

	/* Scan for desired interface */
	for (ptr = buf;
	    ptr < buf + len;
	    ptr += ((struct if_msghdr *)(void *)ptr)->ifm_msglen) {
		struct if_msghdr *const ifm = (struct if_msghdr *)(void *)ptr;
		struct sockaddr_dl *sdl;

		/* Sanity check version */
		if (ifm->ifm_version != RTM_VERSION) {
			FREE(mtype, buf);
			errno = EIO;
			return (NULL);
		}

		/* Next interface or previous interface address? */
		if (ifm->ifm_type != RTM_IFINFO)
			continue;

		/* Compare interface name */
		sdl = (struct sockaddr_dl *)(ifm + 1);
		if (strlen(ifname) != sdl->sdl_nlen ||
		    strncmp(ifname, sdl->sdl_data, sdl->sdl_nlen) != 0)
			continue;

		/* Found it */
		*bufp = buf;
		return (ifm);
	}

	/* Not found */
	FREE(mtype, buf);
	errno = ENOENT;
	return (NULL);
}

/*
 * Get the interface list. Caller must free *bufp.
 */
static int
if_info(u_char **bufp, const char *mtype)
{
	int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_IFLIST, 0 };
	size_t length;
	u_char *buf;

	if (sysctl(mib, 6, NULL, &length, NULL, 0) == -1)
		return (-1);
	length += 256;
	if ((buf = MALLOC(mtype, length)) == NULL)
		return (-1);
	if (sysctl(mib, 6, buf, &length, NULL, 0) == -1)
		return (-1);
	*bufp = buf;
	return (length);
}

