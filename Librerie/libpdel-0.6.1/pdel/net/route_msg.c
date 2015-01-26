
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
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/typed_mem.h"
#include "net/route_msg.h"

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n)	((x) += ROUNDUP((n)->sa_len))

struct route_msg {
	int		type;		/* message type */
	int		index;		/* index for associated ifp */
	int		flags;		/* flags, incl. kern & msg, e.g. DONE */
	pid_t		pid;		/* for sender to identify action */
	int		seq;		/* for sender to identify action */
	int		error;		/* why failed */
	struct sockaddr	*dest;		/* destination sockaddr */
	struct sockaddr	*gateway;	/* gateway sockaddr */
	struct sockaddr	*netmask;	/* netmask sockaddr */
	struct sockaddr	*genmask;	/* genmask sockaddr */
	struct sockaddr	*ifp;		/* interface name sockaddr */
	struct sockaddr	*ifa;		/* interface addr sockaddr */
	struct sockaddr	*author;	/* author of redirect sockaddr */
	struct sockaddr	*brd;		/* for NEWADDR, broadcast or p2p dest */
};

/*
 * Internal variables
 */

/* List of sockaddr's in a route message from the kernel */
struct msg_sock {
	const char	*name;
	u_int		offset;
};

static const	struct msg_sock msg_socks[] = {
	{ "dest",	offsetof(struct route_msg, dest)	},
	{ "gateway",	offsetof(struct route_msg, gateway)	},
	{ "netmask",	offsetof(struct route_msg, netmask)	},
	{ "genmask",	offsetof(struct route_msg, genmask)	},
	{ "ifp",	offsetof(struct route_msg, ifp)		},
	{ "ifa",	offsetof(struct route_msg, ifa)		},
	{ "author",	offsetof(struct route_msg, author)	},
	{ "brd",	offsetof(struct route_msg, brd)		},
	{ NULL, 0 }
};

static int	prev_seq;		/* last used sequence number */

/*
 * Internal functions
 */
static int	route_msg_set_sa(struct route_msg *msg,
			const char *name, const struct sockaddr *sa);

/*
 * Create a new route message.
 */
struct route_msg *
route_msg_create(void)
{
	struct route_msg *msg;

	if ((msg = MALLOC("route_msg", sizeof(*msg))) == NULL)
		return (NULL);
	memset(msg, 0, sizeof(*msg));
	msg->seq = ++prev_seq;				/* XXX not atomic */
	msg->pid = getpid();
	return (msg);
}

/*
 * Destroy a routing message.
 */
void
route_msg_destroy(struct route_msg **msgp)
{
	struct route_msg *const msg = *msgp;

	if (msg == NULL)
		return;
	FREE("route_msg.dest", msg->dest);
	FREE("route_msg.gateway", msg->gateway);
	FREE("route_msg.netmask", msg->netmask);
	FREE("route_msg.genmask", msg->genmask);
	FREE("route_msg.ifp", msg->ifp);
	FREE("route_msg.ifa", msg->ifa);
	FREE("route_msg.author", msg->author);
	FREE("route_msg.brd", msg->brd);
	FREE("route_msg", msg);
	*msgp = NULL;
}

/*
 * Get message type.
 */
int
route_msg_get_type(struct route_msg *msg)
{
	return (msg->type);
}

/*
 * Set message type.
 */
void
route_msg_set_type(struct route_msg *msg, int type)
{
	msg->type = type;
}

/*
 * Get message index.
 */
int
route_msg_get_index(struct route_msg *msg)
{
	return (msg->index);
}

/*
 * Set message index.
 */
void
route_msg_set_index(struct route_msg *msg, int index)
{
	msg->index = index;
}

/*
 * Get message flags.
 */
int
route_msg_get_flags(struct route_msg *msg)
{
	return (msg->flags);
}

/*
 * Set message flags.
 */
void
route_msg_set_flags(struct route_msg *msg, int flags)
{
	msg->flags = flags;
}

/*
 * Get message error code.
 */
int
route_msg_get_error(struct route_msg *msg)
{
	return (msg->error);
}

/*
 * Get message process ID.
 */
pid_t
route_msg_get_pid(struct route_msg *msg)
{
	return (msg->pid);
}

/*
 * Get message sequence number.
 */
int
route_msg_get_seq(struct route_msg *msg)
{
	return (msg->seq);
}

/*
 * Get message destination.
 */
const struct sockaddr *
route_msg_get_dest(struct route_msg *msg)
{
	return (msg->dest);
}

/*
 * Set message destination.
 */
int
route_msg_set_dest(struct route_msg *msg, const struct sockaddr *dest)
{
	return (route_msg_set_sa(msg, "dest", dest));
}

/*
 * Get message gateway.
 */
const struct sockaddr *
route_msg_get_gateway(struct route_msg *msg)
{
	return (msg->gateway);
}

/*
 * Set message gateway.
 */
int
route_msg_set_gateway(struct route_msg *msg, const struct sockaddr *gateway)
{
	return (route_msg_set_sa(msg, "gateway", gateway));
}

/*
 * Get message netmask.
 */
const struct sockaddr *
route_msg_get_netmask(struct route_msg *msg)
{
	return (msg->netmask);
}

/*
 * Set message netmask.
 */
int
route_msg_set_netmask(struct route_msg *msg, const struct sockaddr *netmask)
{
	return (route_msg_set_sa(msg, "netmask", netmask));
}

/*
 * Get message cloning netmask.
 */
const struct sockaddr *
route_msg_get_genmask(struct route_msg *msg)
{
	return (msg->genmask);
}

/*
 * Set message cloning netmask.
 */
int
route_msg_set_genmask(struct route_msg *msg, const struct sockaddr *genmask)
{
	return (route_msg_set_sa(msg, "genmask", genmask));
}

/*
 * Get message interface name.
 */
const struct sockaddr *
route_msg_get_ifp(struct route_msg *msg)
{
	return (msg->ifp);
}

/*
 * Set message interface name.
 */
int
route_msg_set_ifp(struct route_msg *msg, const struct sockaddr *ifp)
{
	return (route_msg_set_sa(msg, "ifp", ifp));
}

/*
 * Get message interface address.
 */
const struct sockaddr *
route_msg_get_ifa(struct route_msg *msg)
{
	return (msg->ifa);
}

/*
 * Set message interface address.
 */
int
route_msg_set_ifa(struct route_msg *msg, const struct sockaddr *ifa)
{
	return (route_msg_set_sa(msg, "ifa", ifa));
}

/*
 * Get message author of the redirect message.
 */
const struct sockaddr *
route_msg_get_author(struct route_msg *msg)
{
	return (msg->author);
}

/*
 * Set message author of the redirect message.
 */
int
route_msg_set_author(struct route_msg *msg, const struct sockaddr *author)
{
	return (route_msg_set_sa(msg, "author", author));
}

/*
 * Get message broadcast or point-to-point destination address.
 */
const struct sockaddr *
route_msg_get_brd(struct route_msg *msg)
{
	return (msg->brd);
}

/*
 * Set message broadcast or point-to-point destination address.
 */
int
route_msg_set_brd(struct route_msg *msg, const struct sockaddr *brd)
{
	return (route_msg_set_sa(msg, "brd", brd));
}

/*
 * Set one of the struct sockaddr fields.
 */
static int
route_msg_set_sa(struct route_msg *msg,
	const char *name, const struct sockaddr *sa)
{
	struct sockaddr *copy;
	struct sockaddr **sp;
	char buf[32];
	int i;

	for (i = 0; msg_socks[i].name != NULL
	    && strcmp(name, msg_socks[i].name) != 0; i++);
	assert(msg_socks[i].name != NULL);
	sp = (struct sockaddr **)(void *)((u_char *)msg + msg_socks[i].offset);
	snprintf(buf, sizeof(buf), "route_msg.%s", name);
	if (sa != NULL) {
		if ((copy = MALLOC(buf, sa->sa_len)) == NULL)
			return (-1);
		memcpy(copy, sa, sa->sa_len);
	} else
		copy = NULL;
	FREE(buf, *sp);
	*sp = copy;
	return (0);
}

/*
 * Decode one or more routing messages from raw socket data.
 *
 * Returns the number of decoded messages, or -1 for error.
 * The array of struct route_msg pointers is returned in *listp,
 * in an array allocated with memory type 'mtype'.
 */
int
route_msg_decode(const u_char *data, size_t dlen,
	struct route_msg ***listp, const char *mtype)
{
	struct route_msg **list;
	struct rt_msghdr rtm;
	int posn;
	int num;
	int i;

	/* Count number of messages */
	for (num = posn = 0; posn < dlen; num++, posn += rtm.rtm_msglen) {
		if (dlen - posn < sizeof(rtm)) {
			errno = EINVAL;
			return (-1);
		}
		memcpy(&rtm, data + posn, sizeof(rtm.rtm_msglen));
		if (rtm.rtm_msglen < sizeof(rtm)) {
			errno = EINVAL;
			return (-1);
		}
	}

	/* Allocate list */
	if ((list = MALLOC(mtype, num * sizeof(*list))) == NULL)
		return (-1);
	memset(list, 0, num * sizeof(*list));

	/* Decode each message */
	for (i = posn = 0; i < num; i++, posn += rtm.rtm_msglen) {
		int posn2;
		int j;

		/* Create new route_msg object */
		if ((list[i] = route_msg_create()) == NULL)
			goto fail;

		/* Copy fields from route message header */
		memcpy(&rtm, data + posn, sizeof(rtm));
		list[i]->type = rtm.rtm_type;
		list[i]->index = rtm.rtm_index;
		list[i]->flags = rtm.rtm_flags;
		list[i]->pid = rtm.rtm_pid;
		list[i]->seq = rtm.rtm_seq;
		list[i]->error = rtm.rtm_errno;

		/* Create struct sockaddr's from each appended sockaddr */
		for (posn2 = posn + sizeof(rtm), j = 0;
		    msg_socks[j].name != NULL; j++) {
			const struct sockaddr *const sa
			    = (struct sockaddr *)(data + posn2);

			if ((rtm.rtm_addrs & (1 << j)) == 0)
				continue;
			if (sa->sa_len != 0) {
				if (route_msg_set_sa(list[i],
				    msg_socks[j].name, sa) == -1)
					goto fail;
			}
			ADVANCE(posn2, sa);
		}
	}

	/* Done */
	*listp = list;
	return (num);

fail:
	/* Cleanup after failure */
	for (i = 0; i < num; i++)
		route_msg_destroy(&list[i]);
	FREE(mtype, list);
	return (-1);
}

/*
 * Encode a route message into raw binary form.
 *
 * Returns the total encoded length, or -1 for error.
 */
int
route_msg_encode(const struct route_msg *msg, u_char *data, size_t dlen)
{
	struct rt_msghdr rtm;
	int totalLen;
	int i;

	/* Prepare header */
	if ((totalLen = sizeof(rtm)) > dlen) {
		errno = EMSGSIZE;
		return (-1);
	}
	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = msg->type;
	rtm.rtm_index = msg->index;
	rtm.rtm_flags = msg->flags;
	rtm.rtm_pid = msg->pid;
	rtm.rtm_seq = msg->seq;
	rtm.rtm_errno = msg->error;

	/* Append struct sockaddr's */
	for (i = 0; msg_socks[i].name != NULL; i++) {
		struct sockaddr *const sa = *((struct sockaddr **)(void *)
		    ((u_char *)msg + msg_socks[i].offset));
		int len;

		if (sa == NULL)
			continue;
		rtm.rtm_addrs |= (1 << i);
		len = ROUNDUP(sa->sa_len);
		if (totalLen + len > dlen) {
			errno = EMSGSIZE;
			return (-1);
		}
		memcpy(data + totalLen, sa, sa->sa_len);
		memset(data + totalLen + sa->sa_len, 0, len - sa->sa_len);
		totalLen += len;
	}

	/* Copy in completed header */
	rtm.rtm_msglen = totalLen;
	memcpy(data, &rtm, sizeof(rtm));

	/* Done */
	return (totalLen);
}

/*
 * Send a route message to a socket.
 */
int
route_msg_send(struct route_msg *msg, int sock)
{
	u_char buf[512];
	int count;
	int elen;
	int r;

	if ((elen = route_msg_encode(msg, buf, sizeof(buf))) == -1)
		return (-1);
	for (count = 0; count < 32; count++) {
		if ((r = send(sock, buf, elen, 0)) >= 0)
			return (0);
		switch (errno) {
		case ENETUNREACH:
		case ESRCH:
			break;
		default:
			return (-1);
		}
	}
	return (-1);
}

/*
 * Receive route messages from a socket.
 */
int
route_msg_recv(struct route_msg ***listp, int sock, const char *mtype)
{
	u_char buf[512];
	int r;

	/* Read next raw message array */
	if ((r = recv(sock, buf, sizeof(buf), 0)) < 0)
		return (-1);

	/* Decode it */
	return (route_msg_decode(buf, r, listp, mtype));
}

/*
 * Print route message as a string.
 */
void
route_msg_print(struct route_msg *msg, FILE *fp)
{
	const char *typestr = NULL;

	fprintf(fp, "type=");
	switch (msg->type) {
	case RTM_ADD:		typestr = "ADD"; break;
	case RTM_DELETE:	typestr = "DELETE"; break;
	case RTM_CHANGE:	typestr = "CHANGE"; break;
	case RTM_GET:		typestr = "GET"; break;
	case RTM_LOSING:	typestr = "LOSING"; break;
	case RTM_REDIRECT:	typestr = "REDIRECT"; break;
	case RTM_MISS:		typestr = "MISS"; break;
	case RTM_LOCK:		typestr = "LOCK"; break;
	case RTM_OLDADD:	typestr = "OLDADD"; break;
	case RTM_OLDDEL:	typestr = "OLDDEL"; break;
	case RTM_RESOLVE:	typestr = "RESOLVE"; break;
	case RTM_NEWADDR:	typestr = "NEWADDR"; break;
	case RTM_DELADDR:	typestr = "DELADDR"; break;
	case RTM_IFINFO:	typestr = "IFINFO"; break;
	case RTM_NEWMADDR:	typestr = "NEWMADDR"; break;
	case RTM_DELMADDR:	typestr = "DELMADDR"; break;
	default:
		fprintf(fp, "%d", msg->type);
		break;
	}
	if (typestr != NULL)
		fprintf(fp, "%s", typestr);
	fprintf(fp, " index=%d flags=0x%x pid=%lu seq=%d errno=%d",
	    msg->index, msg->flags, (u_long)msg->pid, msg->seq, msg->error);
	if (msg->dest != NULL)
		fprintf(fp, " dest=%p", msg->dest);
	if (msg->gateway != NULL)
		fprintf(fp, " gateway=%p", msg->gateway);
	if (msg->netmask != NULL)
		fprintf(fp, " netmask=%p", msg->netmask);
	if (msg->genmask != NULL)
		fprintf(fp, " genmask=%p", msg->genmask);
	if (msg->ifp != NULL)
		fprintf(fp, " ifp=%p", msg->ifp);
	if (msg->ifa != NULL)
		fprintf(fp, " ifa=%p", msg->ifa);
	if (msg->author != NULL)
		fprintf(fp, " author=%p", msg->author);
	if (msg->brd != NULL)
		fprintf(fp, " brd=%p", msg->brd);
	fflush(fp);
}

