
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "net/if_util.h"
#include "net/uroute.h"
#include "util/typed_mem.h"

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

#define ADDADDR(cp, s)							\
	do {								\
		memcpy(cp, &s, sizeof(s));				\
		cp += ROUNDUP(sizeof(s));				\
	} while (0)

#define TEMP_EXPIRE	(20 * 60)

struct rt {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
};

/*
 * Internal functions
 */
static int	arp_set(int sock, struct in_addr ip,
			const u_char *ether, int temp, int publish);
static int	arp_delete(int sock, struct in_addr ip);
static int	arp_rtmsg(int sock, struct rt *rt, struct sockaddr_inarp *sin,
			struct sockaddr_dl *sdl, int cmd, int flags,
			int export_only, int doing_proxy, u_long expiry);

/*
 * Internal variables
 */
static const	struct sockaddr_in so_mask = { 8, 0, 0, { 0xffffffff} };
static const	struct sockaddr_inarp zero_sin = { sizeof(zero_sin), AF_INET };
static const	struct sockaddr_dl zero_sdl = { sizeof(zero_sdl), AF_LINK };

/*
 * Get an ARP entry.
 */
int
if_get_arp(struct in_addr ip, u_char *ether)
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;

	/* Get ARP table */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		return (-1);
	needed += 128;
	if ((buf = MALLOC(TYPED_MEM_TEMP, needed)) == NULL)
		return (-1);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		FREE(TYPED_MEM_TEMP, buf);
		return (-1);
	}

	/* Find desired entry */
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		sin = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(void *)
		   ((char *)sin + ROUNDUP(sin->sin_len));
		if (sin->sin_addr.s_addr != ip.s_addr)
			continue;
		if (sdl->sdl_alen == 0)
			break;
		memcpy(ether, LLADDR(sdl), ETHER_ADDR_LEN);
		FREE(TYPED_MEM_TEMP, buf);
		return (0);
	}

	/* Not found */
	FREE(TYPED_MEM_TEMP, buf);
	errno = ENOENT;
	return (-1);
}

/*
 * Set or remove an ARP entry.
 */
int
if_set_arp(struct in_addr ip, const u_char *ether, int temp, int publish)
{
	int ret = -1;
	int sock;

	/* Get socket */
	if ((sock = socket(PF_ROUTE, SOCK_RAW, 0)) == -1)
		return (-1);

	/* Delete any existing entries */
	while ((ret = arp_delete(sock, ip)) != -1);
	if (errno != ENOENT)
		goto done;

	/* If not setting a new one, done */
	if (ether == NULL) {
		ret = 0;
		goto done;
	}

	/* Set a new one */
	ret = arp_set(sock, ip, ether, temp, publish);

done:
	/* Done */
	(void)close(sock);
	return (ret);
}

/*
 * Set an individual arp entry
 */
static int
arp_set(int sock, struct in_addr ip, const u_char *ether, int temp, int publish)
{
	struct rt m_rtmsg;
	struct sockaddr_inarp sin_m;
	struct sockaddr_dl sdl_m;
	struct sockaddr_inarp *sin = &sin_m;
	struct rt_msghdr *const rtm = &m_rtmsg.m_rtm;
	struct sockaddr_dl *sdl;
	int doing_proxy;
	int export_only;
	int expiry;
	int flags;

	sdl_m = zero_sdl;
	sin_m = zero_sin;
	sin->sin_addr = ip;
	doing_proxy = flags = export_only = expiry = 0;
	if (temp)
		expiry = time(NULL) + TEMP_EXPIRE;
	if (publish) {
		flags |= RTF_ANNOUNCE;
		doing_proxy = SIN_PROXY;
	}
	memcpy(LLADDR(&sdl_m), ether, ETHER_ADDR_LEN);
	sdl_m.sdl_alen = 6;
tryagain:
	if (arp_rtmsg(sock, &m_rtmsg, &sin_m, &sdl_m,
	    RTM_GET, flags, export_only, doing_proxy, expiry) < 0)
		return (-1);
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(void *)
	    (ROUNDUP(sin->sin_len) + (char *)sin);
	if (sin->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK
		    && (rtm->rtm_flags & (RTF_LLINFO|RTF_GATEWAY))
		      == RTF_LLINFO) {
			switch (sdl->sdl_type) {
			case IFT_ETHER:
			case IFT_FDDI:
			case IFT_ISO88023:
			case IFT_ISO88024:
			case IFT_ISO88025:
			case IFT_L2VLAN:
				goto overwrite;
			default:
				break;
			}
		}
		if (doing_proxy == 0) {
			errno = EINVAL;
			return (-1);
		}
		if (sin_m.sin_other & SIN_PROXY) {
			errno = EINVAL;
			return (-1);
		}
		sin_m.sin_other = SIN_PROXY;
		export_only = 1;
		goto tryagain;
	}
overwrite:
	if (sdl->sdl_family != AF_LINK) {
		errno = ENOENT;
		return (-1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (arp_rtmsg(sock, &m_rtmsg, &sin_m, &sdl_m,
	    RTM_ADD, flags, export_only, doing_proxy, expiry));
}

/*
 * Delete an arp entry
 */
static int
arp_delete(int sock, struct in_addr ip)
{
	struct rt m_rtmsg;
	struct sockaddr_inarp sin_m;
	struct sockaddr_dl sdl_m;
	struct sockaddr_inarp *sin = &sin_m;
	struct rt_msghdr *const rtm = &m_rtmsg.m_rtm;
	struct sockaddr_dl *sdl;

	sin_m = zero_sin;
	sin->sin_addr = ip;
tryagain:
	if (arp_rtmsg(sock, &m_rtmsg, &sin_m, &sdl_m, RTM_GET, 0, 0, 0, 0) < 0)
		return (-1);
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(void *)
	    (ROUNDUP(sin->sin_len) + (char *)sin);
	if (sin->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025:
			goto delete;
		}
	}
	if (sin_m.sin_other & SIN_PROXY) {
		errno = ENOENT;
		return (-1);
	}
	sin_m.sin_other = SIN_PROXY;
	goto tryagain;
delete:
	if (sdl->sdl_family != AF_LINK) {
		errno = ENOENT;
		return (-1);
	}
	if (arp_rtmsg(sock, &m_rtmsg, &sin_m, &sdl_m,
	    RTM_DELETE, 0, 0, 0, 0) == -1)
		return (-1);
	return (0);
}

static int
arp_rtmsg(int sock, struct rt *rt, struct sockaddr_inarp *sin,
	struct sockaddr_dl *sdl, int cmd, int flags,
	int export_only, int doing_proxy, u_long expiry)
{
	struct rt_msghdr *const rtm = &rt->m_rtm;
	const pid_t pid = getpid();
	u_char *cp = (u_char *)rt->m_space;
	static int seq;
	int l;
	int rlen;

	if (cmd == RTM_DELETE)
		goto doit;
	memset(rtm, 0, sizeof(*rtm));
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expiry;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
		sin->sin_other = 0;
		if (doing_proxy) {
			if (export_only)
				sin->sin_other = SIN_PROXY;
			else {
				rtm->rtm_addrs |= RTA_NETMASK;
				rtm->rtm_flags &= ~RTF_HOST;
			}
		}
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
		break;
	default:
		assert(0);
	}
#define NEXTADDR(w, s) \
	if (rtm->rtm_addrs & (w)) { \
		memcpy(cp, s, sizeof(*s)); cp += ROUNDUP(sizeof(*s));}

	NEXTADDR(RTA_DST, sin);
	NEXTADDR(RTA_GATEWAY, sdl);
	NEXTADDR(RTA_NETMASK, &so_mask);

	rtm->rtm_msglen = cp - (u_char *)rtm;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(sock, (char *)rt, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE)
			return (-1);
	}
	do {
		l = read(sock, (char *)rt, sizeof(*rt));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		return (-1);
	return (0);
}

/*
 * Flush all ARP entries.
 */
int
if_flush_arp(void)
{
	int errno_save = errno;
	struct uroute **list;
	int rtn = 0;
	int num;
	int i;

	/* Get list of routes */
	if ((num = uroute_get_all(&list, TYPED_MEM_TEMP)) == -1)
		return (-1);

	/* Delete ARP routes */
	for (i = 0; i < num; i++) {
		struct uroute *const route = list[i];
		const struct sockaddr *dest;
		const struct sockaddr *gw;

		/* Is this an ARP entry? */
		dest = uroute_get_dest(route);
		gw = uroute_get_gateway(route);
		if ((uroute_get_flags(route)
		      & (RTF_HOST|RTF_LLINFO|RTF_WASCLONED))
		      != (RTF_HOST|RTF_LLINFO|RTF_WASCLONED)
		    || dest->sa_family != AF_INET
		    || gw->sa_family != AF_LINK)
			continue;

		/* Delete it */
		if (uroute_delete(route) == -1) {
			errno_save = errno;
			rtn = -1;
		}
	}

	/* Clean up */
	while (num > 0)
		uroute_destroy(&list[--num]);
	FREE(TYPED_MEM_TEMP, list);
	errno = errno_save;
	return (rtn);
}

