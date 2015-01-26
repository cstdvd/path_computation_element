
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
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "net/route_msg.h"
#include "net/uroute.h"
#include "util/typed_mem.h"

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n)	((x) += ROUNDUP((n)->sa_len))

#define WRITABLE_FLAGS	(RTF_STATIC | RTF_LLINFO | RTF_REJECT | RTF_BLACKHOLE \
			    | RTF_PROTO1 | RTF_PROTO2 | RTF_CLONING \
			    | RTF_XRESOLVE | RTF_UP | RTF_GATEWAY)

struct route_flag {
	const char	*name;
	int		bit;
};

static const	struct route_flag route_flags[] = {
#define FLAG(x)	{ #x, RTF_ ## x }
	FLAG(UP),
	FLAG(GATEWAY),
	FLAG(HOST),
	FLAG(REJECT),
	FLAG(DYNAMIC),
	FLAG(MODIFIED),
	FLAG(DONE),
	FLAG(CLONING),
	FLAG(XRESOLVE),
	FLAG(LLINFO),
	FLAG(STATIC),
	FLAG(BLACKHOLE),
	FLAG(PROTO2),
	FLAG(PROTO1),
	FLAG(PRCLONING),
	FLAG(WASCLONED),
	FLAG(PROTO3),
	FLAG(PINNED),
	FLAG(LOCAL),
	FLAG(BROADCAST),
	FLAG(MULTICAST),
#undef FLAG
	{ NULL, 0 }
};

/* Route structure */
struct uroute {
	int flags;
	struct sockaddr	*dest;
	struct sockaddr	*gateway;
	struct sockaddr	*netmask;
};

/*
 * Internal functions
 */
static int	uroute_do_route(struct uroute *route, int sock, int type);
static const	char *uroute_sockaddr_string(const struct sockaddr *sa);

/*
 * Create a new route.
 */
struct uroute *
uroute_create(const struct sockaddr *dest, const struct sockaddr *gateway,
	const struct sockaddr *netmask)
{
	struct uroute *route;

	if (dest == NULL || gateway == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	if ((route = MALLOC("uroute", sizeof(*route))) == NULL)
		return (NULL);
	memset(route, 0, sizeof(*route));
	if ((route->dest = MALLOC("uroute.dest", dest->sa_len)) == NULL)
		goto fail;
	memcpy(route->dest, dest, dest->sa_len);
	if ((route->gateway = MALLOC("uroute.gateway",
	    gateway->sa_len)) == NULL)
		goto fail;
	memcpy(route->gateway, gateway, gateway->sa_len);
	if (netmask != NULL) {
		if ((route->netmask = MALLOC("uroute.netmask",
		    netmask->sa_len)) == NULL)
			goto fail;
		memcpy(route->netmask, netmask, netmask->sa_len);
	}
	route->flags = RTF_STATIC;
	return (route);

fail:
	/* Clean up */
	uroute_destroy(&route);
	return (NULL);
}

/*
 * Free a route.
 */
void
uroute_destroy(struct uroute **routep)
{
	struct uroute *const route = *routep;

	if (route == NULL)
		return;
	FREE("uroute.dest", route->dest);
	FREE("uroute.gateway", route->gateway);
	FREE("uroute.netmask", route->netmask);
	FREE("uroute", route);
	*routep = NULL;
}

/*
 * Get the destination address.
 */
const struct sockaddr *
uroute_get_dest(struct uroute *route)
{
	return (route->dest);
}

/*
 * Get the gateway address.
 */
const struct sockaddr *
uroute_get_gateway(struct uroute *route)
{
	return (route->gateway);
}

/*
 * Get the netmask.
 *
 * Returns NULL for a host route.
 */
const struct sockaddr *
uroute_get_netmask(struct uroute *route)
{
	return (route->netmask);
}

/*
 * Get the flags.
 */
int
uroute_get_flags(struct uroute *route)
{
	return (route->flags);
}

/*
 * Set the flags.
 */
void
uroute_set_flags(struct uroute *route, int flags)
{
	route->flags = flags;
}

/*
 * Add route to the kernel routing table.
 */
int
uroute_add(struct uroute *route)
{
	return (uroute_do_route(route, -1, RTM_ADD));
}

/*
 * Delete a route.
 */
int
uroute_delete(struct uroute *route)
{
	return (uroute_do_route(route, -1, RTM_DELETE));
}

/*
 * Get the installed kernel route that matches the destination "dest".
 */
struct uroute *
uroute_get(const struct sockaddr *dest)
{
	struct uroute *route = NULL;
	struct route_msg *msg = NULL;
	struct route_msg **list = NULL;
	struct sockaddr_dl sdl;
	int errno_save;
	int sock = -1;
	int num = 0;
	int seq;
	int i;

	/* Build new route message */
	if ((msg = route_msg_create()) == NULL)
		goto fail;
	seq = route_msg_get_seq(msg);
	route_msg_set_type(msg, RTM_GET);
	if (route_msg_set_dest(msg, dest) == -1)
		goto fail;
	memset(&sdl, 0, sizeof(sdl));
	sdl.sdl_type = IFT_OTHER;
	sdl.sdl_len = 8;
	sdl.sdl_family = AF_LINK;
	if (route_msg_set_ifp(msg, (struct sockaddr *)&sdl) == -1)
		goto fail;
	route_msg_set_flags(msg, RTF_UP|RTF_GATEWAY|RTF_HOST|RTF_STATIC);

	/* Send request */
	if ((sock = socket(PF_ROUTE, SOCK_RAW, 0)) == -1)
		goto fail;
	if (route_msg_send(msg, sock) == -1)
		goto fail;
	route_msg_destroy(&msg);
	msg = NULL;

	/* Get response */
	while (1) {

		/* Decode it */
		if ((num = route_msg_recv(&list, sock, TYPED_MEM_TEMP)) == -1)
			goto fail;

		/* Check returned messages for a match */
		for (i = 0; i < num; i++) {
			struct route_msg *const msg = list[i];

			if (route_msg_get_pid(msg) == getpid()
			    && route_msg_get_seq(msg) == seq) {
				if ((route = uroute_create(route_msg_get_dest(
				    msg), route_msg_get_gateway(msg),
				    route_msg_get_netmask(msg))) == NULL)
					goto fail;
				break;
			}
		}
		if (route != NULL)
			break;

		/* Free list and try again */
		for (i = 0; i < num; i++)
			route_msg_destroy(&list[i]);
		FREE(TYPED_MEM_TEMP, list);
		list = NULL;
	}

fail:
	/* Clean up and exit */
	errno_save = errno;
	if (sock != -1)
		(void)close(sock);
	if (msg != NULL)
		route_msg_destroy(&msg);
	if (list != NULL) {
		for (i = 0; i < num; i++)
			route_msg_destroy(&list[i]);
		FREE(TYPED_MEM_TEMP, list);
	}
	errno = errno_save;
	return (route);
}

/*
 * Build and send a route message by type.
 */
static int
uroute_do_route(struct uroute *route, int sock, int type)
{
	struct route_msg *msg;
	int close_sock = 0;
	int r = -1;
	int flags;

	/* Create new route message */
	if ((msg = route_msg_create()) == NULL)
		return (-1);

	/* Open socket if requested */
	if (sock == -1) {
		if ((sock = socket(PF_ROUTE, SOCK_RAW, 0)) == -1)
			goto fail;
		close_sock = 1;
	}

	/* Build message */
	route_msg_set_type(msg, type);
	flags = route->flags & WRITABLE_FLAGS;
	flags |= RTF_UP;
	if (route->gateway->sa_family != AF_LINK)
		flags |= RTF_GATEWAY;
	if (route->netmask == NULL)
		flags |= RTF_HOST;
	else
		flags &= ~RTF_HOST;
	route_msg_set_flags(msg, flags);
	if (route_msg_set_dest(msg, route->dest) == -1)
		goto fail;
	if (route_msg_set_gateway(msg, route->gateway) == -1)
		goto fail;
	if (route_msg_set_netmask(msg, route->netmask) == -1)
		goto fail;

	/* Send messsage */
	r = route_msg_send(msg, sock);

fail:
	/* Clean up */
	route_msg_destroy(&msg);
	if (close_sock)
		(void)close(sock);

	/* Done */
	return (r);
}

/*
 * Get the entire routing table.
 */
int
uroute_get_all(struct uroute ***listp, const char *mtype)
{
	static int oid[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_DUMP, 0 };
	static const int noid = sizeof(oid) / sizeof(*oid);
	struct route_msg **list = NULL;
	struct uroute **routes = NULL;
	u_char *buf = NULL;
	size_t actual;
	size_t needed;
	int num = 0;
	int i;

	/* Get size estimate */
	if (sysctl(oid, noid, NULL, &needed, NULL, 0) < 0)
		goto fail;

	/* Allocate buffer for returned value */
	if ((buf = MALLOC(TYPED_MEM_TEMP, needed)) == NULL)
		goto fail;

	/* Get actual data */
	actual = needed;
	if (sysctl(oid, noid, buf, &actual, NULL, 0) < 0)
		goto fail;

	/* Decode route messages */
	if ((num = route_msg_decode(buf, actual, &list, TYPED_MEM_TEMP)) == -1)
		goto fail;

	/* Create routes array from route message array */
	if ((routes = MALLOC(mtype, num * sizeof(*routes))) == NULL)
		goto fail;
	memset(routes, 0, num * sizeof(*routes));
	for (i = 0; i < num; i++) {
		struct route_msg *const msg = list[i];

		if ((routes[i] = uroute_create(route_msg_get_dest(msg),
		    route_msg_get_gateway(msg),
		    route_msg_get_netmask(msg))) == NULL) {
			while (i > 0)
				uroute_destroy(&routes[--i]);
			FREE(mtype, routes);
			routes = NULL;
			goto fail;
		}
		routes[i]->flags = route_msg_get_flags(msg);
	}

fail:
	/* Clean up and exit */
	if (buf != NULL)
		FREE(TYPED_MEM_TEMP, buf);
	if (list != NULL) {
		for (i = 0; i < num; i++)
			route_msg_destroy(&list[i]);
		FREE(TYPED_MEM_TEMP, list);
	}
	if (routes != NULL) {
		*listp = routes;
		return (num);
	}
	return (-1);
}

/*
 * Print a route.
 */
void
uroute_print(struct uroute *route, FILE *fp)
{
	int didflag;
	int i;

	fprintf(fp, "dest %s gateway %s",
	    uroute_sockaddr_string(route->dest),
	    uroute_sockaddr_string(route->gateway));
	if (route->netmask != NULL) {
		fprintf(fp, " netmask %s",
		    uroute_sockaddr_string(route->netmask));
	}
	fprintf(fp, " flags=<");
	for (i = didflag = 0; route_flags[i].name != NULL; i++) {
		if ((route->flags & route_flags[i].bit) != 0) {
			if (didflag)
				fprintf(fp, ",");
			didflag = 1;
			fprintf(fp, "%s", route_flags[i].name);
		}
	}
	fprintf(fp, ">");
}

static const char *
uroute_sockaddr_string(const struct sockaddr *sa)
{
	static char buf[2][256];
	static int n;
	int i;

	n ^= 1;
	switch (sa->sa_family) {
	case AF_INET:
		strlcpy(buf[n], inet_ntoa(((struct sockaddr_in *)(void *)
		    sa)->sin_addr), sizeof(buf[n]));
		break;
	case AF_LINK:
		/* XXX implement me */
	default:
		snprintf(buf[n], sizeof(buf[n]), "{ len=%d af=%d data=[",
		    sa->sa_len, sa->sa_family);
		for (i = 2; i < sa->sa_len; i++) {
			snprintf(buf[n] + strlen(buf[n]),
			    sizeof(buf[n]) - strlen(buf[n]),
			    " %02X", ((u_char *)sa)[i]);
		}
		strlcat(buf[n], " ] }", sizeof(buf[n]));
	}
	return (buf[n]);
}


#ifdef UROUTE_TEST

int
main(int ac, char **av)
{
	struct sockaddr_in dest;
	struct sockaddr_in gateway;
	struct sockaddr_in netmask;
	struct uroute *route = NULL;
	int do_netmask = 0;
	int fail = 0;
	int cmd = 0;

	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_len = sizeof(dest);
	memset(&gateway, 0, sizeof(gateway));
	gateway.sin_family = AF_INET;
	gateway.sin_len = sizeof(gateway);
	memset(&netmask, 0, sizeof(netmask));
	netmask.sin_family = AF_INET;
	netmask.sin_len = sizeof(netmask);

	av++;
	ac--;
	switch (ac) {
	case 4:
		if (inet_aton(av[3], &netmask.sin_addr))
			;
		else {
			u_long haddr;

			if (sscanf(av[3], "%lu", &haddr) != 1)
				err(1, "invalid netmask \"%s\"", av[3]);
			netmask.sin_addr.s_addr = htonl(haddr);
		}
		do_netmask = 1;
		// fall through
	case 3:
		if (!inet_aton(av[2], &gateway.sin_addr))
			err(1, "invalid IP address \"%s\"", av[2]);
		if (!strcmp(av[0], "add"))
			cmd = 1;
		else if (!strcmp(av[0], "delete"))
			cmd = 2;
		else {
			fail = 1;
			break;
		}
		if (!inet_aton(av[1], &dest.sin_addr))
			err(1, "invalid IP address \"%s\"", av[1]);
		if ((route = uroute_create((struct sockaddr *)&dest,
		    (struct sockaddr *)&gateway,
		    do_netmask ? (struct sockaddr *)&netmask : NULL)) == NULL)
			err(1, "uroute_create");
		break;
	case 2:
		if (strcmp(av[0], "get") != 0) {
			fail = 1;
			break;
		}
		if (!inet_aton(av[1], &dest.sin_addr))
			err(1, "invalid IP address \"%s\"", av[1]);
		break;
	case 1:
		if (strcmp(av[0], "list") != 0) {
			fail = 1;
			break;
		}
		// fall through
	case 0:
		cmd = 3;
		break;
	default:
		fail = 1;
		break;
	}
	if (fail) {
		fprintf(stderr, "Usage: uroute <add | delete | get | list>"
		    " [dest [ gateway [netmask] ]]\n");
		exit(1);
	}

	switch (cmd) {
	case 0:				// get
		if ((route = uroute_get((struct sockaddr *)&dest)) == NULL)
			err(1, "uroute_get");
		uroute_print(route, stdout);
		printf("\n");
		uroute_destroy(&route);
		break;
	case 1:				// add
		if (uroute_add(route) == -1)
			err(1, "uroute_add");
		uroute_destroy(&route);
		break;
	case 2:				// delete
		if (uroute_delete(route) == -1)
			err(1, "uroute_delete");
		uroute_destroy(&route);
		break;
	case 3:				// list
	    {
		struct uroute **list;
		int num;
		int i;

		if ((num = uroute_get_all(&list, "main")) == -1)
			err(1, "uroute_get_all");
		for (i = 0; i < num; i++) {
			uroute_print(list[i], stdout);
			printf("\n");
		}
		for (i = 0; i < num; i++)
			uroute_destroy(&list[i]);
		FREE("main", list);
		break;
	    }
	default:
		assert(0);
	}

	/* Done */
	typed_mem_dump(stdout);
	return (0);
}
#endif

