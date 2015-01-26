
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "sls_global.h"
#include "sls_config.h"

#define MANAGER_MEM_TYPE			"sls_bundle"

/*
 * PPP manager definition.
 */
static ppp_manager_bundle_config_t	sls_manager_bundle_config;
static ppp_manager_bundle_plumb_t	sls_manager_bundle_plumb;
static ppp_manager_bundle_unplumb_t	sls_manager_bundle_unplumb;
static ppp_manager_release_ip_t		sls_manager_release_ip;

static struct ppp_manager_meth sls_manager_methods = {
	sls_manager_bundle_config,
	sls_manager_bundle_plumb,
	sls_manager_bundle_unplumb,
	sls_manager_release_ip,
};

struct ppp_manager sls_manager = {
	&sls_manager_methods
};

/* Internal functions */
static int	sls_manager_proxy_arp(struct in_addr ip, u_char *ether);

/*
 * Private per-bundle info.
 */
struct sls_bundle {
	int		pool_index;
	int		proxy_arp;
	struct in_addr	ip;
	char		nodepath[64];		/* ng_iface(4) node path */
};

/***********************************************************************
			MANAGER METHODS
***********************************************************************/

static void *
sls_manager_bundle_config(struct ppp_manager *manager,
	struct ppp_link *link, struct ppp_bundle_config *conf)
{
	const char *const username = ppp_link_get_authname(link, PPP_PEER);
	u_char ether[ETHER_ADDR_LEN];
	struct ppp_bundle **bundles;
	struct in_addr ip = { 0 };
	struct sls_bundle *info;
	struct sls_user *user;
	int num_bundles;
	int i;

	/* Allocate return value */
	if ((info = MALLOC(MANAGER_MEM_TYPE, sizeof(*info))) == NULL) {
		alog(LOG_ERR, "%s: %m", "malloc");
		goto fail;
	}
	memset(info, 0, sizeof(*info));
	info->pool_index = -1;
	info->ip = ip;

	/* Find user */
	for (i = 0; i < sls_curconf->users.length
	    && strcmp(sls_curconf->users.elems[i].name, username) != 0; i++);
	if (i == sls_curconf->users.length) {
		errno = ENOENT;		/* this should never happen */
		goto fail;
	}
	user = &sls_curconf->users.elems[i];

	/* See if user is already connected */
	if ((num_bundles = ppp_engine_get_bundles(engine,
	    &bundles, TYPED_MEM_TEMP)) == -1) {
		alog(LOG_ERR, "%s: %m", "ppp_engine_get_bundles");
		goto fail;
	}
	for (i = 0; i < num_bundles; i++) {
		const char *const bundle_owner
		    = ppp_bundle_get_authname(bundles[i], PPP_PEER);
		struct ppp_link *links[64];
		int num_links;
		int j;

		/* Skip the same bundle we're configuring */
		num_links = ppp_bundle_get_links(bundles[i],
		    links, sizeof(links) / sizeof(*links));
		for (j = 0; j < num_links && links[j] != link; j++);
		if (j < num_links)
			continue;

		/* See if user is also connected via a different bundle */
		if (strcmp(username, bundle_owner) == 0) {
			alog(LOG_NOTICE, "user \"%s\" tried to connect twice",
			    username);
			FREE(TYPED_MEM_TEMP, bundles);
			goto fail;
		}
	}
	FREE(TYPED_MEM_TEMP, bundles);

	/* Allocate an IP address */
	if (user->ip.s_addr != 0)
		ip = user->ip;
	else if (ip_pool != NULL) {
		for (i = 0; i < sls_curconf->ip_pool_size; i++) {
			if ((ip_pool[i / 32] & (1 << (i % 32))) == 0) {
				ip.s_addr = htonl(ntohl(
				    sls_curconf->ip_pool_start.s_addr) + i);
				break;
			}
		}
		if (ip.s_addr != 0) {
			ip_pool[i / 32] |= (1 << (i % 32));
			info->pool_index = i;
		}
	}
	if (ip.s_addr == 0) {
		alog(LOG_ERR, "no IP address available for \"%s\"", username);
		errno = EALREADY;
		goto fail;
	}

	/* See if we should proxy-ARP */
	if (sls_manager_proxy_arp(ip, ether)) {
		if (if_set_arp(ip, ether, 0, 1) == -1) {
			alog(LOG_ERR, "can't proxy-arp for %s: %m",
			    inet_ntoa(ip));
			goto fail;
		}
		info->proxy_arp = 1;
	}

	/* Configure bundle */
	memset(conf, 0, sizeof(*conf));
	conf->ip[PPP_SELF] = sls_curconf->inside_ip;
	conf->ip[PPP_PEER] = ip;
	for (i = 0; i < 2 && i < sls_curconf->dns_servers.length; i++)
		conf->dns_servers[i] = sls_curconf->dns_servers.elems[i];
	for (i = 0; i < 2 && i < sls_curconf->nbns_servers.length; i++)
		conf->nbns_servers[i] = sls_curconf->nbns_servers.elems[i];
	conf->vjc = 1;
	conf->mppe_128 = 1;
	conf->mppe_stateless = 1;

	/* Done */
	return info;

fail:
	/* Clean up after failure */
	if (info != NULL) {
		if (info->proxy_arp) {
			if (if_set_arp(info->ip, NULL, 0, 0) == -1)
				alog(LOG_ERR, "%s: %m", "if_set_arp");
		}
		if ((i = info->pool_index) != -1)
			ip_pool[i / 32] &= ~(1 << (i % 32));
		FREE(MANAGER_MEM_TYPE, info);
	}
	return NULL;
}

static void *
sls_manager_bundle_plumb(struct ppp_manager *manager,
	struct ppp_bundle *bundle, const char *path, const char *hook,
	struct in_addr *ips, struct in_addr *dns, struct in_addr *nbns,
	u_int mtu)
{
	struct sls_bundle *const info = ppp_bundle_get_cookie(bundle);
	static const struct in_addr fullmask = { ~0 };
	union {
	    u_char buf[sizeof(struct ng_mesg) + 128];
	    struct ng_mesg reply;
	} repbuf;
	struct ng_mesg *const reply = &repbuf.reply;
	char *const ifname = (char *)reply->data;
	char ifpath[64];
	int csock = -1;
	int esave;

	/* Get temporary socket node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1) {
		alog(LOG_ERR, "%s: %m", "NgMkSockNode");
		goto fail;
	}
	snprintf(ifpath, sizeof(ifpath), "%s%s", path, hook);

	/* Attach iface node */
	if (NgSendAsciiMsg(csock, path, "mkpeer { type=\"%s\""
	    " ourhook=\"%s\" peerhook=\"%s\" }", NG_IFACE_NODE_TYPE,
	    hook, NG_IFACE_HOOK_INET) == -1) {
		alog(LOG_ERR, "%s: %m", "mkpeer");
		goto fail;
	}

	/* Get node name */
	if (NgSendMsg(csock, ifpath, NGM_IFACE_COOKIE, NGM_IFACE_GET_IFNAME,
	    NULL, 0) == -1) {
		alog(LOG_ERR, "%s: %m", "NgSendMsg");
		goto fail;
	}
	if (NgRecvMsg(csock, reply, sizeof(repbuf), NULL) == -1) {
		alog(LOG_ERR, "%s: %m", "NgRecvMsg");
		goto fail;
	}

	/* Configure iface node */
	if (if_add_ip_addr(ifname,
	    ips[PPP_SELF], fullmask, ips[PPP_PEER]) == -1) {
		alog(LOG_ERR, "if_add_ip(%s, %s): %m",
		    ifname, inet_ntoa(ips[PPP_SELF]));
		goto fail;
	}
	if (if_set_mtu(ifname, mtu) == -1) {
		alog(LOG_ERR, "if_setmtu(%s, %d): %m", ifname, mtu);
		goto fail;
	}

	/* Get return value which is the name of the interface node */
	snprintf(info->nodepath, sizeof(info->nodepath), "%s:", ifname);

	/* Done */
	(void)close(csock);
	return info;

fail:
	/* Clean up after failure */
	esave = errno;
	if (csock != -1) {
		(void)NgSendMsg(csock, ifpath, NGM_GENERIC_COOKIE,
		    NGM_SHUTDOWN, NULL, 0);
		(void)close(csock);
	}
	errno = esave;
	return NULL;
}

static void
sls_manager_bundle_unplumb(struct ppp_manager *manager, void *arg,
	struct ppp_bundle *bundle)
{
	struct sls_bundle *const info = ppp_bundle_get_cookie(bundle);
	char *const ifpath = arg;
	int csock;

	/* Get temporary socket node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1) {
		alog(LOG_ERR, "%s: %m", "NgMkSockNode");
		return;
	}

	/* Remove proxy ARP entry (if any) */
	if (info->proxy_arp) {
		if (if_set_arp(info->ip, NULL, 0, 0) == -1)
			alog(LOG_ERR, "%s: %m", "if_set_arp");
	}

	/* Kill iface node */
	if (NgSendMsg(csock, info->nodepath,
	    NGM_GENERIC_COOKIE, NGM_SHUTDOWN, NULL, 0) == -1)
		alog(LOG_ERR, "shutdown(%s): %m", ifpath);

	/* Done */
	(void)close(csock);
}

static void
sls_manager_release_ip(struct ppp_manager *manager,
	struct ppp_bundle *bundle, struct in_addr ip)
{
	struct sls_bundle *const info = ppp_bundle_get_cookie(bundle);
	int i;

	/* Free up pool IP address */
	if ((i = info->pool_index) != -1 && ip_pool != NULL) {
		assert((ip_pool[i / 32] & (1 << (i % 32))) != 0);
		ip_pool[i / 32] &= ~(1 << (i % 32));
	}
	FREE(MANAGER_MEM_TYPE, info);
}

/*
 * Determine if IP address should be proxy-ARP'd.
 * If so, return 1 and fill in the ethernet address.
 */
static int
sls_manager_proxy_arp(struct in_addr ip, u_char *ether)
{
	char **ifaces;
	int num_ifaces;
	int found = 0;
	int i;

	/* Get interface list */
	if ((num_ifaces = if_get_list(&ifaces, TYPED_MEM_TEMP)) == -1) {
		alog(LOG_ERR, "%s: %m", "if_get_list");
		return 0;
	}

	/* Check for a suitable match */
	for (i = 0; i < num_ifaces; i++) {
		const char *const iface = ifaces[i];
		const int flags = if_get_flags(iface);
		struct sockaddr_dl *sdl;
		struct in_addr *ips;
		struct in_addr *masks;
		int num_ips;
		int j;

		/* Check interface */
		if ((flags & (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT
		    |IFF_LOOPBACK|IFF_NOARP)) != (IFF_UP|IFF_BROADCAST))
			continue;

		/* Get configured subnets */
		if ((num_ips = if_get_ip_addrs(iface,
		    &ips, &masks, TYPED_MEM_TEMP)) == -1) {
			alog(LOG_ERR, "%s: %m", "if_get_ip_addrs");
			continue;
		}

		/* Look for a matching subnet */
		for (j = 0; j < num_ips; j++) {
			const struct in_addr netip = ips[j];
			const struct in_addr netmask = masks[j];

			if ((netip.s_addr & netmask.s_addr)
			    == (ip.s_addr & netmask.s_addr))
				break;
		}
		FREE(TYPED_MEM_TEMP, ips);
		FREE(TYPED_MEM_TEMP, masks);
		if (j == num_ips)
			continue;

		/* Get Ethernet address associated with interface */
		if (if_get_link_addr(iface, &sdl, TYPED_MEM_TEMP) == -1) {
			alog(LOG_ERR, "%s: %m", "if_get_link_addr");
			continue;
		}
		if (sdl->sdl_alen != ETHER_ADDR_LEN) {
			FREE(TYPED_MEM_TEMP, sdl);
			continue;
		}
		memcpy(ether, LLADDR(sdl), ETHER_ADDR_LEN);
		FREE(TYPED_MEM_TEMP, sdl);
		found = 1;
		break;
	}

	/* Clean up */
	for (i = 0; i < num_ifaces; i++)
		FREE(TYPED_MEM_TEMP, ifaces[i]);
	FREE(TYPED_MEM_TEMP, ifaces);

	/* Done */
	return found;
}

