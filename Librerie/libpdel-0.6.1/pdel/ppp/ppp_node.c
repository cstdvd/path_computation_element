
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_node.h"

#include <sys/queue.h>
#include <netgraph/ng_socket.h>

/*
 * This manages an ng_ppp(4) netgraph node.
 */

#define NODE_MTYPE		"ppp_node"
#define PKTBUFLEN		2000
#define NODE_HOOK		"ppp"

/* One recipient for incoming control messages */
struct ppp_node_recvmsg {
	u_int32_t			cookie;
	u_int32_t			cmd;
	ppp_node_recvmsg_t		*recvmsg;
	void				*arg;
	TAILQ_ENTRY(ppp_node_recvmsg)	next;
};

/* PPP node structure */
struct ppp_node {
	int			csock;		/* ng_socket ctrl socket */
	int			dsock;		/* ng_socket data socket */
	struct pevent_ctx	*ev_ctx;	/* event context */
	pthread_mutex_t		*mutex;		/* mutex */
	struct pevent		*cevent;	/* incoming ctrl msg event */
	struct pevent		*devent;	/* incoming packet event */
	struct ppp_log		*log;		/* log */
	ppp_node_recv_t		*recv;		/* handler for packets */
	void			*rarg;		/* recv() function arg */
	struct ng_ppp_node_conf	conf;		/* ng_ppp(4) node config */
	char			path[32];	/* netgraph path to node */
	u_char			got_conf;	/* "conf" is valid */
	u_char			connected[NG_PPP_MAX_LINKS];
	TAILQ_HEAD(,ppp_node_recvmsg) rmlist;	/* ng_mesg recipient list */
};

/* Internal functions */
static pevent_handler_t	ppp_node_read_packet;
static pevent_handler_t	ppp_node_read_message;

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(node->log, sev, fmt , ## args)

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/*
 * Create a new PPP node from a newly created link.
 *
 * The "log" is consumed.
 */
struct ppp_node *
ppp_node_create(struct pevent_ctx *ev_ctx,
	pthread_mutex_t *mutex, struct ppp_log *log)
{
	union {
	    u_char buf[sizeof(struct ng_mesg) + sizeof(struct nodeinfo)];
	    struct ng_mesg reply;
	} repbuf;
	struct ng_mesg *reply = &repbuf.reply;
	struct nodeinfo ninfo;
	struct ngm_mkpeer mkpeer;
	struct ppp_node *node;

	/* Create new node structure */
	if ((node = MALLOC(NODE_MTYPE, sizeof(*node))) == NULL)
		return (NULL);
	memset(node, 0, sizeof(*node));
	node->ev_ctx = ev_ctx;
	node->mutex = mutex;
	node->log = log;
	node->csock = -1;
	node->dsock = -1;
	TAILQ_INIT(&node->rmlist);

	/* Create netgraph socket node */
	if (NgMkSockNode(NULL, &node->csock, &node->dsock) == -1) {
		LOG(LOG_ERR, "%s: %m", "creating socket node");
		goto fail;
	}
	(void)fcntl(node->csock, F_SETFD, 1);
	(void)fcntl(node->dsock, F_SETFD, 1);

	/* Attach a new ng_ppp(4) node to the socket node via "bypass" hook */
	memset(&mkpeer, 0, sizeof(mkpeer));
	strlcpy(mkpeer.type, NG_PPP_NODE_TYPE, sizeof(mkpeer.type));
	strlcpy(mkpeer.ourhook, NODE_HOOK, sizeof(mkpeer.ourhook));
	strlcpy(mkpeer.peerhook, NG_PPP_HOOK_BYPASS, sizeof(mkpeer.peerhook));
	if (NgSendMsg(node->csock, ".",
	    NGM_GENERIC_COOKIE, NGM_MKPEER, &mkpeer, sizeof(mkpeer)) == -1) {
		LOG(LOG_ERR, "%s: %m", "attaching ppp node");
		goto fail;
	}

	/* Get node info including 'id' and create absolute path for node */
	if (NgSendMsg(node->csock, NODE_HOOK,
	    NGM_GENERIC_COOKIE, NGM_NODEINFO, NULL, 0) == -1) {
		LOG(LOG_ERR, "can't get node info: %m");
		goto fail;
	}
	memset(&repbuf, 0, sizeof(repbuf));
	if (NgRecvMsg(node->csock, reply, sizeof(repbuf), NULL) == -1) {
		LOG(LOG_ERR, "can't read node info: %m");
		goto fail;
	}
	memcpy(&ninfo, reply->data, sizeof(ninfo));
	snprintf(node->path, sizeof(node->path), "[%lx]:", (long)ninfo.id);

	/* Register event for reading incoming packets from bypass hook */
	if (pevent_register(node->ev_ctx, &node->devent, PEVENT_RECURRING,
	    node->mutex, ppp_node_read_packet, node, PEVENT_READ,
	    node->dsock) == -1) {
		LOG(LOG_ERR, "%s: %m", "adding read event");
		goto fail;
	}

	/* Register event for reading incoming control messages */
	if (pevent_register(node->ev_ctx, &node->cevent, PEVENT_RECURRING,
	    node->mutex, ppp_node_read_message, node, PEVENT_READ,
	    node->csock) == -1) {
		LOG(LOG_ERR, "%s: %m", "adding read event");
		goto fail;
	}

	/* Done */
	return (node);

fail:
	/* Clean up */
	if (node->csock != -1) {
		(void)NgSendMsg(node->csock, NODE_HOOK,
		    NGM_GENERIC_COOKIE, NGM_SHUTDOWN, NULL, 0);
		(void)close(node->csock);
		(void)close(node->dsock);
	}
	FREE(NODE_MTYPE, node);
	return (NULL);
}

/*
 * Destroy a node.
 */
void
ppp_node_destroy(struct ppp_node **nodep)
{
	struct ppp_node *const node = *nodep;
	const int esave = errno;

	if (node == NULL)
		return;
	*nodep = NULL;
	while (!TAILQ_EMPTY(&node->rmlist)) {
		struct ppp_node_recvmsg *const rm = TAILQ_FIRST(&node->rmlist);

		ppp_node_set_recvmsg(node, rm->cookie, rm->cmd, NULL, NULL);
	}
	(void)NgSendMsg(node->csock, NODE_HOOK,
	    NGM_GENERIC_COOKIE, NGM_SHUTDOWN, NULL, 0);
	(void)close(node->csock);
	(void)close(node->dsock);
	pevent_unregister(&node->devent);
	pevent_unregister(&node->cevent);
	ppp_log_close(&node->log);
	FREE(NODE_MTYPE, node);
	errno = esave;
}

/*
 * Write to bypass hook.
 */
int
ppp_node_write(struct ppp_node *node, u_int link_num,
	u_int16_t proto, const void *data, size_t len)
{
	struct sockaddr_ng sag;
	u_int16_t hdr[2];
	u_char *buf;

	/* Build ng_ppp(4) bypass header */
	hdr[0] = htons(link_num);
	hdr[1] = htons(proto);

	/* Set destination hook */
	memset(&sag, 0, sizeof(sag));
	sag.sg_len = 2 + sizeof(NODE_HOOK);
	sag.sg_family = AF_NETGRAPH;
	strlcpy(sag.sg_data, NODE_HOOK, sizeof(sag.sg_data));

	/* Write packet */
	if ((buf = MALLOC(TYPED_MEM_TEMP, len + 4)) == NULL)
		return (-1);
	memcpy(buf, hdr, 4);
	memcpy(buf + 4, data, len);
	if (sendto(node->dsock, buf, len + 4, 0,
	      (struct sockaddr *)&sag, sag.sg_len) == -1
	    && errno != ENOBUFS) {
		FREE(TYPED_MEM_TEMP, buf);
		return (-1);
	}
	FREE(TYPED_MEM_TEMP, buf);

	/* Done */
	return (len);
}

/*
 * Connect a link.
 */
int
ppp_node_connect(struct ppp_node *node, u_int link_num,
	const char *path, const char *hook)
{
	struct ngm_connect connect;

	/* Sanity checks */
	if (link_num >= NG_PPP_MAX_LINKS) {
		errno = EINVAL;
		return (-1);
	}
	if (node->connected[link_num])
		ppp_node_disconnect(node, link_num);

	/* Connect ng_ppp(4) device hook */
	memset(&connect, 0, sizeof(connect));
	strlcpy(connect.path, path, sizeof(connect.path));
	snprintf(connect.ourhook, sizeof(connect.ourhook),
	    "%s%u", NG_PPP_HOOK_LINK_PREFIX, link_num);
	strlcpy(connect.peerhook, hook, sizeof(connect.peerhook));
	if (NgSendMsg(node->csock, NODE_HOOK,
	    NGM_GENERIC_COOKIE, NGM_CONNECT, &connect, sizeof(connect)) == -1) {
		LOG(LOG_ERR, "connecting %s%u: %m",
		    NG_PPP_HOOK_LINK_PREFIX, link_num);
		return (-1);
	}

	/* Done */
	node->connected[link_num] = 1;
	return (0);
}

/*
 * Disonnect a link.
 */
int
ppp_node_disconnect(struct ppp_node *node, u_int link_num)
{
	struct ngm_rmhook rmhook;

	/* Sanity check */
	if (link_num >= NG_PPP_MAX_LINKS) {
		errno = EINVAL;
		return (-1);
	}

	/* Remove hook */
	memset(&rmhook, 0, sizeof(rmhook));
	snprintf(rmhook.ourhook, sizeof(rmhook.ourhook),
	    "%s%u", NG_PPP_HOOK_LINK_PREFIX, link_num);
	if (NgSendMsg(node->csock, NODE_HOOK, NGM_GENERIC_COOKIE,
	      NGM_RMHOOK, &rmhook, sizeof(rmhook)) == -1
	    && errno != ENOENT) {
		LOG(LOG_ERR, "disconnecting %s%u: %m",
		    NG_PPP_HOOK_LINK_PREFIX, link_num);
		return (-1);
	}

	/* Done */
	node->connected[link_num] = 0;
	return (0);
}

/*
 * Get absolute path to node.
 */
const char *
ppp_node_get_path(struct ppp_node *node)
{
	return (node->path);
}

/***********************************************************************
		    CONTROL MESSAGE FUNCTIONS
***********************************************************************/

/*
 * Get node configuration.
 */
int
ppp_node_get_config(struct ppp_node *node, struct ng_ppp_node_conf *conf)
{
	union {
	    u_char buf[sizeof(struct ng_mesg) + sizeof(*conf)];
	    struct ng_mesg reply;
	} buf;
	struct ng_mesg *const reply = &buf.reply;

	if (!node->got_conf) {
		memset(&buf, 0, sizeof(buf));
		if (NgSendMsg(node->csock, NODE_HOOK,
		    NGM_PPP_COOKIE, NGM_PPP_GET_CONFIG, NULL, 0) < 0) {
			LOG(LOG_ERR, "%s: %m", "getting node configuration");
			return (-1);
		}
		if (NgRecvMsg(node->csock, reply, sizeof(buf), NULL) == -1) {
			LOG(LOG_ERR, "%s: %m", "receiving node configuration");
			return (-1);
		}
		memcpy(&node->conf, reply->data, sizeof(node->conf));
		node->got_conf = 1;
	}
	*conf = node->conf;
	return (0);
}

/*
 * Set node configuration.
 */
int
ppp_node_set_config(struct ppp_node *node, const struct ng_ppp_node_conf *conf)
{
	if (NgSendMsg(node->csock, NODE_HOOK, NGM_PPP_COOKIE,
	    NGM_PPP_SET_CONFIG, conf, sizeof(*conf)) < 0) {
		LOG(LOG_ERR, "%s: %m", "configuring node");
		return (-1);
	}
	node->got_conf = 1;
	node->conf = *conf;
	return (0);
}

/*
 * Set incoming packet recipient.
 */
void
ppp_node_set_recv(struct ppp_node *node, ppp_node_recv_t *recv, void *arg)
{
	node->recv = recv;
	node->rarg = arg;
}

/*
 * Set incoming message recipient.
 */
int
ppp_node_set_recvmsg(struct ppp_node *node, u_int32_t cookie,
	u_int32_t cmd, ppp_node_recvmsg_t *recvmsg, void *arg)
{
	struct ppp_node_recvmsg *rm;
	int found = 0;

	/* Check if already exists */
	TAILQ_FOREACH(rm, &node->rmlist, next) {
		if (rm->cookie == cookie && rm->cmd == cmd) {
			found = 1;
			break;
		}
	}

	/* Removing it? */
	if (recvmsg == NULL) {
		if (found) {
			TAILQ_REMOVE(&node->rmlist, rm, next);
			FREE(NODE_MTYPE, rm);
		}
		return (0);
	} else if (found) {
		errno = EALREADY;
		return (-1);
	}

	/* Add new message receiver */
	if ((rm = MALLOC(NODE_MTYPE, sizeof(*rm))) == NULL)
		return (-1);
	memset(rm, 0, sizeof(*rm));
	rm->cookie = cookie;
	rm->cmd = cmd;
	rm->recvmsg = recvmsg;
	rm->arg = arg;
	TAILQ_INSERT_TAIL(&node->rmlist, rm, next);
	return (0);
}

int
ppp_node_send_msg(struct ppp_node *node, const char *relpath,
	u_int32_t cookie, u_int32_t cmd, const void *payload, size_t plen)
{
	char path[NG_PATHLEN + 1];

	if (relpath == NULL)
		strlcpy(path, NODE_HOOK, sizeof(path));
	else
		snprintf(path, sizeof(path), "%s.%s", NODE_HOOK, relpath);
	if (NgSendMsg(node->csock, path, cookie, cmd, payload, plen) == -1)
		return (-1);
	return (0);
}

int
ppp_node_recv_msg(struct ppp_node *node,
	struct ng_mesg *msg, size_t mlen, char *raddr)
{
	return (NgRecvMsg(node->csock, msg, mlen, raddr));
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

/*
 * Handle incoming data on the ng_ppp(4) bypass hook.
 */
static void
ppp_node_read_packet(void *arg)
{
	struct ppp_node *const node = arg;
	u_char buf[4 + PKTBUFLEN] __attribute__((aligned));
	u_int16_t link_num;
	u_int16_t proto;
	int len;

	/* Read packet */
	if ((len = read(node->dsock, buf, sizeof(buf))) == -1) {
		LOG(LOG_ERR, "error reading bypass hook: %m");
		return;
	}

	/* Extract protocol and link number */
	if (len < 4) {
		LOG(LOG_ERR, "read len %d bypass packet", len);
		return;
	}
	memcpy(&link_num, buf, 2);
	link_num = ntohs(link_num);
	memcpy(&proto, buf + 2, 2);
	proto = ntohs(proto);

	/* Shift packet contents down so they are properly aligned */
	memmove(buf, buf + 4, len - 4);
	len -= 4;

	/* Deliver packet */
	if (node->recv != NULL)
		(*node->recv)(node->rarg, link_num, proto, buf, len);
}

/*
 * Handle incoming control message on the netgraph socket.
 */
static void
ppp_node_read_message(void *arg)
{
	struct ppp_node *const node = arg;
	const size_t max_msglen = 4096;
	char raddr[NG_PATHLEN + 1];
	struct ppp_node_recvmsg *rm;
	struct ng_mesg *msg;
	int found = 0;
	int len;

	/* Allocate buffer */
	if ((msg = MALLOC(TYPED_MEM_TEMP, max_msglen)) == NULL) {
		LOG(LOG_ERR, "malloc: %m");
		return;
	}

	/* Read incoming message */
	if ((len = NgRecvMsg(node->csock, msg, max_msglen, raddr)) == -1) {
		LOG(LOG_ERR, "reading ctrl message: %m");
		goto done;
	}

	/* Search for handler */
	TAILQ_FOREACH(rm, &node->rmlist, next) {
		if (rm->cookie == msg->header.typecookie
		    && rm->cmd == msg->header.cmd) {
			found = 1;
			break;
		}
	}
	if (!found)
		goto done;

	/* Invoke handler */
	(*rm->recvmsg)(rm->arg, msg);

done:
	/* Clean up */
	FREE(TYPED_MEM_TEMP, msg);
}

