
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_engine.h"
#include "ppp/ppp_fsm_option.h"
#include "ppp/ppp_auth.h"
#include "ppp/ppp_lcp.h"
#include "ppp/ppp_link.h"
#include "ppp/ppp_channel.h"
#include "ppp/ppp_l2tp_avp.h"
#include "ppp/ppp_l2tp_server.h"
#include "ppp/ppp_l2tp_ctrl.h"

#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <netgraph/ng_ksocket.h>

#define L2TP_MTYPE		"ppp_l2tp"
#define L2TP_OUTPUT_MTYPE	"ppp_l2tp.output"
#define L2TP_DEVICE_MTYPE	"ppp_l2tp.device"
#define L2TP_PEER_MTYPE		"ppp_l2tp.peer"

/* Win2k is too stupid to handle changing the UDP port */
#define L2TP_CHANGE_PORT	0

#if 0				/* win2k seems to require at least 1500 */
#define L2TP_MRU							\
    (ETHER_MAX_LEN		/* standard ethernet frame */		\
	- ETHER_CRC_LEN		/* ethernet crc */			\
	- ETHER_HDR_LEN		/* ethernet header */			\
	- sizeof(struct ip)	/* ip header */				\
	- sizeof(struct udp)	/* udp header */			\
	- 10			/* l2tp header */
	- 2)			/* ppp protocol field */
#else
#define L2TP_MRU		LCP_DEFAULT_MRU
#endif

#define L2TP_MRRU		LCP_DEFAULT_MRRU

/* L2TP server info */
struct ppp_l2tp_server {
	struct ppp_l2tp_server_info	info;		/* client info */
	struct ppp_log			*log;		/* ppp log */
	struct ppp_engine		*engine;	/* ppp engine */
	struct ghash			*peers;		/* active peers */
	struct pevent_ctx		*ev_ctx;	/* event context */
	pthread_mutex_t			*mutex;		/* mutex */
	struct in_addr			ip;		/* server ip address */
	u_int16_t			port;		/* server udp port */
	int				sock;		/* udp listen socket */
	struct pevent			*sock_event;	/* event context */
};

/* We keep one of these for each control connection */
struct ppp_l2tp_peer {
	struct ppp_l2tp_server		*s;		/* pointer to server */
	struct ppp_l2tp_ctrl		*ctrl;		/* ctrl connection */
	void				*carg;		/* client callbck arg */
	struct ppp_l2tp_sess		*sess;		/* l2tp session */
	struct ppp_channel		*chan;		/* pointer to channel */
	struct ppp_auth_config		auth;		/* auth config */
	char				node[32];		/* node path */
	char				hook[NG_HOOKLEN + 1];	/* node hook */
	char				logname[32];	/* peer logname */
	struct in_addr			ip;		/* peer ip address */
	u_int16_t			port;		/* peer port */
	u_char				closed;		/* closed by client */
};

/* L2TP control callbacks */
static ppp_l2tp_ctrl_terminated_t	ppp_l2tp_server_ctrl_terminated;
static ppp_l2tp_initiated_t		ppp_l2tp_server_initiated;
static ppp_l2tp_connected_t		ppp_l2tp_server_connected;
static ppp_l2tp_terminated_t		ppp_l2tp_server_terminated;

static const struct ppp_l2tp_ctrl_cb ppp_l2tp_server_ctrl_cb = {
	ppp_l2tp_server_ctrl_terminated,
	ppp_l2tp_server_initiated,
	ppp_l2tp_server_connected,
	ppp_l2tp_server_terminated,
	NULL,
	NULL,
};

/* Device methods */
static ppp_channel_open_t		ppp_l2tp_server_device_open;
static ppp_channel_close_t		ppp_l2tp_server_device_close;
static ppp_channel_destroy_t		ppp_l2tp_server_device_destroy;
static ppp_channel_free_output_t	ppp_l2tp_server_device_free_output;
static ppp_channel_set_link_info_t	ppp_l2tp_server_device_set_link_info;
static ppp_channel_get_origination_t	ppp_l2tp_server_device_get_origination;
static ppp_channel_get_node_t		ppp_l2tp_server_device_get_node;
static ppp_channel_get_hook_t		ppp_l2tp_server_device_get_hook;
static ppp_channel_is_async_t		ppp_l2tp_server_device_is_async;
static ppp_channel_get_mtu_t		ppp_l2tp_server_device_get_mtu;
static ppp_channel_get_acfcomp_t	ppp_l2tp_server_device_get_acfcomp;
static ppp_channel_get_pfcomp_t		ppp_l2tp_server_device_get_pfcomp;

static struct ppp_channel_meth ppp_l2tp_server_device_meth = {
	ppp_l2tp_server_device_open,
	ppp_l2tp_server_device_close,
	ppp_l2tp_server_device_destroy,
	ppp_l2tp_server_device_free_output,
	ppp_l2tp_server_device_set_link_info,
	ppp_l2tp_server_device_get_origination,
	ppp_l2tp_server_device_get_node,
	ppp_l2tp_server_device_get_hook,
	ppp_l2tp_server_device_is_async,
	ppp_l2tp_server_device_get_mtu,
	ppp_l2tp_server_device_get_acfcomp,
	ppp_l2tp_server_device_get_pfcomp,
};

/* Other internal functions */
static int	ppp_l2tp_server_new_sess(struct ppp_l2tp_peer *peer,
			struct ppp_l2tp_sess *sess);
static void	ppp_l2tp_server_destroy(struct ppp_l2tp_server **sp);
static void	ppp_l2tp_server_peer_destroy(struct ppp_l2tp_peer **peerp);
static void	ppp_l2tp_server_device_output(struct ppp_l2tp_peer *peer,
			enum ppp_channeloutput type, ...);
static void	ppp_l2tp_server_close_client(struct ppp_l2tp_peer *peer);

static pevent_handler_t	ppp_l2tp_server_sock_event;

static const	int one = 1;

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(s->log, sev, fmt , ## args)

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/*
 * Start the L2TP server associated with a ppp engine.
 */
int
ppp_l2tp_server_start(struct ppp_engine *engine,
	const struct ppp_l2tp_server_info *info,
	struct in_addr ip, u_int16_t port, u_int max_conn)
{
	struct ppp_log *const elog = ppp_engine_get_log(engine);
	struct sockaddr_in sin;
	struct ppp_l2tp_server *s;

	/* Sanity */
	if (engine == NULL || info->arg == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* See if server already exists */
	if ((s = ppp_engine_get_l2tp_server(engine)) != NULL) {
		errno = EALREADY;
		return (-1);
	}

	/* Create new server */
	if ((s = MALLOC(L2TP_MTYPE, sizeof(*s))) == NULL)
		return (-1);
	memset(s, 0, sizeof(*s));
	s->engine = engine;
	s->ev_ctx = ppp_engine_get_ev_ctx(engine);
	s->mutex = ppp_engine_get_mutex(engine);
	s->sock = -1;
	s->log = ppp_log_dup(elog);
	s->info = *info;
	s->ip = ip;
	s->port = (port != 0) ? port : L2TP_PORT;

	/* Create control connection hash table */
	if ((s->peers = ghash_create(s, 0, 0,
	    L2TP_MTYPE, NULL, NULL, NULL, NULL)) == NULL) {
		ppp_log_put(elog, LOG_ERR, "ghash_create: %m");
		goto fail;
	}

	/* Setup UDP socket that listens for new connections */
	if ((s->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ppp_log_put(elog, LOG_ERR, "socket: %m");
		goto fail;
	}
	if (setsockopt(s->sock, SOL_SOCKET,
	    SO_REUSEADDR, &one, sizeof(one)) == -1) {
		ppp_log_put(elog, LOG_ERR, "setsockopt: %m");
		goto fail;
	}
	if (setsockopt(s->sock, SOL_SOCKET,
	    SO_REUSEPORT, &one, sizeof(one)) == -1) {
		ppp_log_put(elog, LOG_ERR, "setsockopt: %m");
		goto fail;
	}
	memset(&sin, 0, sizeof(sin));
#ifndef __linux__
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_addr = ip;
	sin.sin_port = htons(s->port);
	if (bind(s->sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		ppp_log_put(elog, LOG_ERR, "bind: %m");
		goto fail;
	}
	if (pevent_register(s->ev_ctx, &s->sock_event, PEVENT_RECURRING,
	    s->mutex, ppp_l2tp_server_sock_event, s, PEVENT_READ,
	    s->sock) == -1) {
		ppp_log_put(elog, LOG_ERR, "pevent_register: %m");
		goto fail;
	}

	/* Done */
	ppp_engine_set_l2tp_server(engine, s);
	return (0);

fail:
	/* Clean up after failure */
	pevent_unregister(&s->sock_event);
	if (s->sock != -1)
		close(s->sock);
	ghash_destroy(&s->peers);
	FREE(L2TP_MTYPE, s);
	return (-1);
}

/*
 * Stop the L2TP server associated with a ppp engine.
 *
 * We can't completely destroy it, because there may be L2TP devices
 * in use by ppp_link's that still exist. The ppp_link's are responsible
 * for destroying their devices, not us.
 */
void
ppp_l2tp_server_stop(struct ppp_engine *engine)
{
	struct ppp_l2tp_server *s;

	/* Get and clear L2TP server */
	if ((s = ppp_engine_get_l2tp_server(engine)) == NULL)
		return;
	ppp_engine_set_l2tp_server(s->engine, NULL);

	/* Stop accepting new connections */
	pevent_unregister(&s->sock_event);
	(void)close(s->sock);
	s->sock = -1;

	/* If there are no control connections, clean up now */
	if (ghash_size(s->peers) == 0) {
		ppp_l2tp_server_destroy(&s);
		return;
	}

	/* Destroy all control connections */
	while (1) {
		struct ppp_l2tp_peer *peer;
		struct ghash_walk walk;

		ghash_walk_init(s->peers, &walk);
		while ((peer = ghash_walk_next(s->peers, &walk)) != NULL) {

			/*
			 * Destroy control connection and session (if any).
			 * We do a 'close' before the 'destroy' to cause at
			 * least one StopCCN packet to be generated, in an
			 * attempt to be a little nicer to the peer.
			 */
			if (peer->ctrl != NULL) {
				ppp_l2tp_ctrl_shutdown(peer->ctrl,
				    L2TP_RESULT_SHUTDOWN, 0, NULL);
				ppp_l2tp_ctrl_destroy(&peer->ctrl);
				peer->sess = NULL;
			}

			/* Notify client side peer is gone */
			ppp_l2tp_server_close_client(peer);

			/* Destroy peer if it has no more references */
			if (peer->chan == NULL) {
				ppp_l2tp_server_peer_destroy(&peer);
				break;		/* restart; walk is invalid */
			}
		}
		if (peer == NULL)
			break;
	}
}

/*
 * Destroy the L2TP server object.
 */
static void
ppp_l2tp_server_destroy(struct ppp_l2tp_server **sp)
{
	struct ppp_l2tp_server *const s = *sp;

	/* Sanity */
	if (s == NULL)
		return;
	*sp = NULL;

	/* Deallocate */
	assert(ghash_size(s->peers) == 0);
	pevent_unregister(&s->sock_event);
	(void)close(s->sock);
	ghash_destroy(&s->peers);
	ppp_log_close(&s->log);
	FREE(L2TP_MTYPE, s);
}

/*
 * Close a L2TP connection.
 */
void
ppp_l2tp_server_close(struct ppp_engine *engine, struct ppp_l2tp_peer **peerp)
{
	struct ppp_l2tp_peer *const peer = *peerp;

	if (peer == NULL)
		return;
	*peerp = NULL;
	peer->carg = NULL;		/* don't call client 'destroy' method */
	if (peer->chan != NULL)
		ppp_l2tp_server_device_close(peer->chan);
}

/*
 * Get the client handle for the L2TP channel associated with a device.
 */
void *
ppp_l2tp_server_get_client_info(struct ppp_channel *chan)
{
	struct ppp_l2tp_peer *const peer = chan->priv;

	if (chan->meth != &ppp_l2tp_server_device_meth) {
		errno = EINVAL;
		return (NULL);
	}
	if (peer->carg == NULL)
		errno = ENXIO;
	return (peer->carg);
}

/***********************************************************************
			L2TP CONTROL CALLBACKS
***********************************************************************/

/*
 * This is called when a control connection is terminated for any reason
 * other than a call ppp_l2tp_ctrl_destroy().
 */
static void
ppp_l2tp_server_ctrl_terminated(struct ppp_l2tp_ctrl *ctrl,
	u_int16_t result, u_int16_t error, const char *errmsg)
{
	struct ppp_l2tp_peer *peer = ppp_l2tp_ctrl_get_cookie(ctrl);
	struct ppp_l2tp_server *const s = peer->s;

	/* Debugging */
	LOG(LOG_DEBUG, "%s: invoked ctrl=%p errmsg=\"%s\"",
	    __FUNCTION__, ctrl, errmsg);

	/* Notify client side peer is gone */
	ppp_l2tp_server_close_client(peer);

	/* There should be no session */
	assert(peer->sess == NULL);
	peer->ctrl = NULL;

	/* Destroy peer if it has no more references */
	if (peer->chan == NULL)
		ppp_l2tp_server_peer_destroy(&peer);
}

/*
 * This callback is used to report the peer's initiating a new incoming
 * or outgoing call.
 */
static void
ppp_l2tp_server_initiated(struct ppp_l2tp_ctrl *ctrl,
	struct ppp_l2tp_sess *sess, int out,
	const struct ppp_l2tp_avp_list *avps)
{
	struct ppp_l2tp_peer *const peer = ppp_l2tp_ctrl_get_cookie(ctrl);
	struct ppp_l2tp_server *const s = peer->s;

	/* Debugging */
	LOG(LOG_DEBUG, "%s: invoked ctrl=%p sess=%p out=%d",
	    __FUNCTION__, ctrl, sess, out);

	/* If call is incoming, wait for peer to reply */
	if (!out) {
		ppp_l2tp_sess_set_cookie(sess, peer);
		return;
	}

	/* Accept call */
	if (ppp_l2tp_server_new_sess(peer, sess) == -1) {
		ppp_l2tp_terminate(sess, L2TP_RESULT_ERROR,
		    L2TP_ERROR_GENERIC, strerror(errno));
		return;
	}

	/* Notify control code */
	ppp_l2tp_connected(sess, NULL);
}

/*
 * This callback is used to report successful connection of a remotely
 * initiated incoming call (see ppp_l2tp_initiated_t) or a locally initiated
 * outgoing call (see ppp_l2tp_initiate()).
 */
static void
ppp_l2tp_server_connected(struct ppp_l2tp_sess *sess,
	const struct ppp_l2tp_avp_list *avps)
{
	struct ppp_l2tp_peer *const peer = ppp_l2tp_sess_get_cookie(sess);
	struct ppp_l2tp_server *const s = peer->s;

	/* Debugging */
	LOG(LOG_DEBUG, "%s: invoked sess=%p", __FUNCTION__, sess);

	/* Accept call */
	if (ppp_l2tp_server_new_sess(peer, sess) == -1) {
		ppp_l2tp_terminate(sess, L2TP_RESULT_ERROR,
		    L2TP_ERROR_GENERIC, strerror(errno));
		return;
	}
}

/*
 * This callback is called when any call, whether successfully connected
 * or not, is terminated for any reason other than explict termination
 * from the link side (via a call to either ppp_l2tp_terminate() or
 * ppp_l2tp_ctrl_destroy()).
 */
static void
ppp_l2tp_server_terminated(struct ppp_l2tp_sess *sess,
	u_int16_t result, u_int16_t error, const char *errmsg)
{
	struct ppp_l2tp_peer *const peer = ppp_l2tp_sess_get_cookie(sess);
	struct ppp_l2tp_server *const s = peer->s;
	char buf[128];

	/* Debugging */
	LOG(LOG_DEBUG, "%s: invoked sess=%p", __FUNCTION__, sess);

	/* Sanity */
	assert(peer->sess == NULL || peer->sess == sess);
	assert(peer->ctrl != NULL);

	/* Control side is notifying us session is down */
	peer->sess = NULL;
	snprintf(buf, sizeof(buf), "result=%u error=%u errmsg=\"%s\"",
	    result, error, (errmsg != NULL) ? errmsg : "");
	LOG(LOG_INFO, "call from %s terminated: %s", peer->logname, buf);

	/* Notify client side peer is gone */
	ppp_l2tp_server_close_client(peer);

	/* Notify PPP stack session is down */
	if (peer->chan != NULL) {
		ppp_l2tp_server_device_output(peer,
		    PPP_CHANNEL_OUTPUT_DOWN_FATAL,
		    (errmsg != NULL) ?  errmsg : "administratively closed");
	}
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

/*
 * Read an incoming packet that might be a new L2TP connection.
 */
static void
ppp_l2tp_server_sock_event(void *arg)
{
	struct ppp_l2tp_server *const s = arg;
	struct ppp_l2tp_avp_list *avps = NULL;
	struct ppp_l2tp_peer *peer = NULL;
#if !L2TP_CHANGE_PORT
	union {
	    u_char buf[sizeof(struct ng_ksocket_sockopt) + sizeof(int)];
	    struct ng_ksocket_sockopt sockopt;
	} sockopt_buf;
	struct ng_ksocket_sockopt *const sockopt = &sockopt_buf.sockopt;
#endif
	struct ppp_log *log = NULL;
	struct ngm_connect connect;
	struct ngm_rmhook rmhook;
	struct ngm_mkpeer mkpeer;
	struct sockaddr_in peer_sin;
	struct sockaddr_in sin;
	const size_t bufsize = 8192;
	u_int16_t *buf = NULL;
	char hook[NG_HOOKLEN + 1];
	socklen_t sin_len;
	char namebuf[64];
	ng_ID_t node_id;
	int csock = -1;
	int dsock = -1;
	int len;

	/* Allocate buffer */
	if ((buf = MALLOC(TYPED_MEM_TEMP, bufsize)) == NULL) {
		LOG(LOG_ERR, "malloc: %m");
		goto fail;
	}

	/* Read packet */
	sin_len = sizeof(peer_sin);
	if ((len = recvfrom(s->sock, buf, bufsize, 0,
	    (struct sockaddr *)&peer_sin, &sin_len)) == -1) {
		LOG(LOG_ERR, "recvfrom: %m");
		goto fail;
	}

	/* Drop it if it's not an initial L2TP packet */
	if (len < 12)
		goto fail;
	if ((ntohs(buf[0]) & 0xcb0f) != 0xc802 || ntohs(buf[1]) < 12
	    || buf[2] != 0 || buf[3] != 0 || buf[4] != 0 || buf[5] != 0)
		goto fail;

	/* Create a new peer */
	if ((peer = MALLOC(L2TP_PEER_MTYPE, sizeof(*peer))) == NULL) {
		LOG(LOG_ERR, "malloc: %m");
		return;
	}
	memset(peer, 0, sizeof(*peer));
	peer->s = s;
	peer->ip = peer_sin.sin_addr;
	peer->port = ntohs(peer_sin.sin_port);

	/* Check with client library */
	snprintf(peer->logname, sizeof(peer->logname), "%s:%u",
	    inet_ntoa(peer_sin.sin_addr), ntohs(peer_sin.sin_port));
	if ((peer->carg = (*s->info.admit)(s->info.arg, peer,
	    peer->ip, peer->port, &peer->auth, peer->logname,
	    sizeof(peer->logname))) == NULL)
		goto fail;

	/* Create vendor name AVP */
	if (s->info.vendor != NULL) {
		if ((avps = ppp_l2tp_avp_list_create()) == NULL) {
			LOG(LOG_ERR, "%s: %m", "ppp_l2tp_avp_list_create");
			goto fail;
		}
		if (ppp_l2tp_avp_list_append(avps, 1, 0, AVP_VENDOR_NAME,
		    s->info.vendor, strlen(s->info.vendor)) == -1) {
			LOG(LOG_ERR, "%s: %m", "ppp_l2tp_avp_list_append");
			goto fail;
		}
	}

	/* Create a log for the control connection */
	if ((log = ppp_log_prefix(s->log, "%s: ", peer->logname)) == NULL) {
		LOG(LOG_ERR, "%s: %m", "ppp_log_prefix");
		goto fail;
	}

	/* Create a new control connection */
	if ((peer->ctrl = ppp_l2tp_ctrl_create(s->ev_ctx, s->mutex,
	    &ppp_l2tp_server_ctrl_cb, log, 0, ntohl(peer_sin.sin_addr.s_addr),
	    &node_id, hook, avps, NULL, 0)) == NULL) {
		LOG(LOG_ERR, "%s: %m", "ppp_l2tp_ctrl_create");
		goto fail;
	}
	ppp_l2tp_ctrl_set_cookie(peer->ctrl, peer);
	log = NULL;		/* log is consumed by control connection */

	/* Get a temporary netgraph socket node */
	if (NgMkSockNode(NULL, &csock, &dsock) == -1) {
		LOG(LOG_ERR, "%s: %m", "NgMkSockNode");
		goto fail;
	}

	/* Connect to l2tp netgraph node "lower" hook */
	snprintf(namebuf, sizeof(namebuf), "[%lx]:", (u_long)node_id);
	memset(&connect, 0, sizeof(connect));
	strlcpy(connect.path, namebuf, sizeof(connect.path));
	strlcpy(connect.ourhook, hook, sizeof(connect.ourhook));
	strlcpy(connect.peerhook, hook, sizeof(connect.peerhook));
	if (NgSendMsg(csock, ".", NGM_GENERIC_COOKIE,
	    NGM_CONNECT, &connect, sizeof(connect)) == -1) {
		LOG(LOG_ERR, "%s: %m", "connect");
		goto fail;
	}

	/* Write the received packet to the node */
	if (NgSendData(dsock, hook, (u_char *)buf, len) == -1) {
		LOG(LOG_ERR, "%s: %m", "NgSendData");
		goto fail;
	}

	/* Disconnect from netgraph node "lower" hook */
	memset(&rmhook, 0, sizeof(rmhook));
	strlcpy(rmhook.ourhook, hook, sizeof(rmhook.ourhook));
	if (NgSendMsg(csock, ".", NGM_GENERIC_COOKIE,
	    NGM_RMHOOK, &rmhook, sizeof(rmhook)) == -1) {
		LOG(LOG_ERR, "%s: %m", "rmhook");
		goto fail;
	}

	/* Attach a new UDP socket to "lower" hook */
	memset(&mkpeer, 0, sizeof(mkpeer));
	strlcpy(mkpeer.type, NG_KSOCKET_NODE_TYPE, sizeof(mkpeer.type));
	strlcpy(mkpeer.ourhook, hook, sizeof(mkpeer.ourhook));
	strlcpy(mkpeer.peerhook, "inet/dgram/udp", sizeof(mkpeer.peerhook));
	if (NgSendMsg(csock, namebuf, NGM_GENERIC_COOKIE,
	    NGM_MKPEER, &mkpeer, sizeof(mkpeer)) == -1) {
		LOG(LOG_ERR, "%s: %m", "mkpeer");
		goto fail;
	}

	/* Point name at ksocket node */
	strlcat(namebuf, hook, sizeof(namebuf));

#if !L2TP_CHANGE_PORT
	/* Make UDP port reusable */
	memset(&sockopt_buf, 0, sizeof(sockopt_buf));
	sockopt->level = SOL_SOCKET;
	sockopt->name = SO_REUSEADDR;
	memcpy(sockopt->value, &one, sizeof(int));
	if (NgSendMsg(csock, namebuf, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_SETOPT, sockopt, sizeof(sockopt_buf)) == -1) {
		LOG(LOG_ERR, "%s: %m", "setsockopt");
		goto fail;
	}
	sockopt->name = SO_REUSEPORT;
	if (NgSendMsg(csock, namebuf, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_SETOPT, sockopt, sizeof(sockopt_buf)) == -1) {
		LOG(LOG_ERR, "%s: %m", "setsockopt");
		goto fail;
	}
#endif

	/* Bind socket to a new port */
	memset(&sin, 0, sizeof(sin));
#ifndef __linux__
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_addr = s->ip;
#if L2TP_CHANGE_PORT
	sin.sin_port = 0;			/* "choose any free port" */
#else
	sin.sin_port = htons(s->port);
#endif
	if (NgSendMsg(csock, namebuf, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_BIND, &sin, sizeof(sin)) == -1) {
		LOG(LOG_ERR, "%s: %m", "bind");
		goto fail;
	}

#if L2TP_CHANGE_PORT
	/* Set up reverse NAT mapping for the new port */
	if (s->info.natmap != NULL) {
		struct ng_mesg *const reply = (struct ng_mesg *)buf;
		struct sockaddr_in *const self_sin
		    = (struct sockaddr_in *)reply->data;

		/* Get kernel-assigned UDP port number */
		if (NgSendMsg(csock, namebuf, NGM_KSOCKET_COOKIE,
		    NGM_KSOCKET_GETNAME, NULL, 0) == -1) {
			LOG(LOG_ERR, "%s: %m", "getsockname");
			goto fail;
		}
		if (NgRecvMsg(csock, reply, bufsize, NULL) == -1) {
			LOG(LOG_ERR, "%s: %m", "recvmsg");
			goto fail;
		}
		if ((*s->info.natmap)(s->info.arg, self_sin->sin_addr,
		    ntohs(self_sin->sin_port), s->port, peer_sin.sin_addr,
		    ntohs(peer_sin.sin_port)) == -1) {
			LOG(LOG_ERR, "%s: %m", "can't reverse NAT map");
			goto fail;
		}
	}
#endif

	/* Connect socket to remote peer's IP and port */
	if (NgSendMsg(csock, namebuf, NGM_KSOCKET_COOKIE,
	      NGM_KSOCKET_CONNECT, &peer_sin, sizeof(peer_sin)) == -1
	    && errno != EINPROGRESS) {
		LOG(LOG_ERR, "%s: %m", "connect");
		goto fail;
	}

	/* Add peer to our hash table */
	if (ghash_put(s->peers, peer) == -1) {
		LOG(LOG_ERR, "%s: %m", "ghash_put");
		goto fail;
	}

	/* Clean up and return */
	ppp_l2tp_avp_list_destroy(&avps);
	(void)close(csock);
	(void)close(dsock);
	FREE(TYPED_MEM_TEMP, buf);
	return;

fail:
	/* Clean up after failure */
	if (csock != -1)
		(void)close(csock);
	if (dsock != -1)
		(void)close(dsock);
	if (peer != NULL) {
		ppp_l2tp_server_close_client(peer);
		ppp_l2tp_ctrl_destroy(&peer->ctrl);
		FREE(L2TP_PEER_MTYPE, peer);
	}
	ppp_l2tp_avp_list_destroy(&avps);
	ppp_log_close(&log);
	FREE(TYPED_MEM_TEMP, buf);
}

/*
 * Create a new L2TP peer object corresponding to a L2TP channel
 * and start up a new PPP link/bundle.
 */
static int
ppp_l2tp_server_new_sess(struct ppp_l2tp_peer *peer, struct ppp_l2tp_sess *sess)
{
	struct ppp_l2tp_server *const s = peer->s;
	struct ppp_link_config link_config;
	struct ppp_log *log = NULL;
	const char *hook;
	ng_ID_t node_id;
	int esave;

	/* We allow only one session per control connection */
	if (peer->sess != NULL) {
		errno = EALREADY;
		return (-1);
	}

	/* Get this link's node and hook */
	ppp_l2tp_sess_get_hook(sess, &node_id, &hook);
	snprintf(peer->node, sizeof(peer->node), "[%lx]:", (u_long)node_id);
	strlcpy(peer->hook, hook, sizeof(peer->hook));

	/* Create a new PPP device for this session */
	if ((peer->chan = MALLOC(L2TP_DEVICE_MTYPE,
	    sizeof(*peer->chan))) == NULL) {
		LOG(LOG_ERR, "can't allocate new channel: %m");
		goto fail;
	}
	memset(peer->chan, 0, sizeof(*peer->chan));
	peer->chan->meth = &ppp_l2tp_server_device_meth;
	peer->chan->priv = peer;

	/* Create device output message port */
	if ((peer->chan->outport
	    = mesg_port_create("ppp_l2tp_server")) == NULL) {
		LOG(LOG_ERR, "can't create mesg_port: %m");
		goto fail;
	}

	/* Create log for the new PPP link by prefixing the engine's log */
	if ((log = ppp_engine_get_log(s->engine)) != NULL
	    && (log = ppp_log_prefix(log, "%s: ", peer->logname)) == NULL) {
		LOG(LOG_ERR, "can't create link log: %m");
		goto fail;
	}

	/* Configure the new PPP link */
	memset(&link_config, 0, sizeof(link_config));
	link_config.auth = peer->auth;
	link_config.max_self_mru = L2TP_MRU;
	link_config.max_self_mrru = L2TP_MRRU;
	link_config.multilink = 1;
	link_config.eid.class = PPP_EID_CLASS_IP;
	link_config.eid.length = sizeof(s->ip);
	memcpy(link_config.eid.value, &s->ip, sizeof(s->ip));

	/* Add new link to the PPP engine */
	if (ppp_link_create(s->engine, peer->chan, &link_config, log) == -1) {
		LOG(LOG_ERR, "can't create link: %m");
		goto fail;
	}
	log = NULL;

	/* Notify PPP engine link is up */
	ppp_l2tp_server_device_output(peer, PPP_CHANNEL_OUTPUT_UP);

	/* Done */
	peer->sess = sess;
	return (0);

fail:
	/* Clean up after failure */
	esave = errno;
	ppp_log_close(&log);
	if (peer->chan != NULL) {
		if (peer->chan->outport != NULL)
			mesg_port_destroy(&peer->chan->outport);
		FREE(L2TP_DEVICE_MTYPE, peer->chan);
	}
	errno = esave;
	return (-1);
}

/*
 * Output indication from the device.
 */
static void
ppp_l2tp_server_device_output(struct ppp_l2tp_peer *peer,
	enum ppp_channeloutput type, ...)
{
	struct ppp_l2tp_server *const s = peer->s;
	struct ppp_channel_output *output;

	/* Get output object */
	if ((output = MALLOC(L2TP_OUTPUT_MTYPE, sizeof(*output))) == NULL) {
		LOG(LOG_ERR, "can't create l2tp output: %m");
		return;
	}
	memset(output, 0, sizeof(*output));
	output->type = type;

	/* Get extra args */
	switch (output->type) {
	case PPP_CHANNEL_OUTPUT_DOWN_FATAL:
	case PPP_CHANNEL_OUTPUT_DOWN_NONFATAL:
	    {
		const char *msg;
		va_list args;

		/* Get string message */
		va_start(args, type);
		msg = va_arg(args, const char *);
		va_end(args);
		if ((output->info = STRDUP(L2TP_OUTPUT_MTYPE, msg)) == NULL) {
			LOG(LOG_ERR, "can't create l2tp output: %m");
			FREE(L2TP_OUTPUT_MTYPE, output);
			return;
		}
		break;
	    }
	case PPP_CHANNEL_OUTPUT_UP:
		break;
	}

	/* Send message */
	if (mesg_port_put(peer->chan->outport, output) == -1) {
		LOG(LOG_ERR, "can't send l2tp output: %m");
		ppp_l2tp_server_device_free_output(peer->chan, output);
		return;
	}
}

/*
 * Notify client code that connection is gone.
 * Make sure that we only do this once however.
 */
static void
ppp_l2tp_server_close_client(struct ppp_l2tp_peer *peer)
{
	struct ppp_l2tp_server *const s = peer->s;
	void *const peer_carg = peer->carg;

	if (peer_carg == NULL)
		return;
	peer->carg = NULL;
	(*s->info.destroy)(s->info.arg, peer_carg);
}

/*
 * Destroy a peer.
 */
static void
ppp_l2tp_server_peer_destroy(struct ppp_l2tp_peer **peerp)
{
	struct ppp_l2tp_peer *peer = *peerp;
	struct ppp_l2tp_server *s;

	/* Sanity checks */
	if (peer == NULL)
		return;
	*peerp = NULL;
	assert(peer->ctrl == NULL);
	assert(peer->sess == NULL);
	assert(peer->chan == NULL);

	/* Destroy peer */
	s = peer->s;
	ghash_remove(s->peers, peer);
	FREE(L2TP_PEER_MTYPE, peer);

	/* Destroy server if shutting down and no more peers left */
	if (s->sock == -1 && ghash_size(s->peers) == 0)
		ppp_l2tp_server_destroy(&s);
}

/***********************************************************************
			L2TP DEVICE METHODS
***********************************************************************/

static void
ppp_l2tp_server_device_open(struct ppp_channel *chan)
{
	return;
}

static void
ppp_l2tp_server_device_close(struct ppp_channel *chan)
{
	struct ppp_l2tp_peer *const peer = chan->priv;
	struct ppp_l2tp_server *const s = peer->s;

	/* Logging */
	if (!peer->closed) {
		LOG(LOG_INFO, "closing L2TP connection with %s", peer->logname);
		peer->closed = 1;
	}

	/* Terminate the L2TP channel */
	if (peer->sess != NULL) {
		ppp_l2tp_terminate(peer->sess, L2TP_RESULT_ADMIN, 0, NULL);
		peer->sess = NULL;
	}

	/* Notify upper layers link is down */
	ppp_l2tp_server_device_output(peer,
	    PPP_CHANNEL_OUTPUT_DOWN_FATAL, "administratively closed");
}

static void
ppp_l2tp_server_device_destroy(struct ppp_channel **chanp)
{
	struct ppp_channel *const chan = *chanp;
	struct ppp_channel_output *output;
	struct ppp_l2tp_peer *peer;
	struct ppp_l2tp_server *s;

	/* Sanity */
	if (chan == NULL)
		return;
	*chanp = NULL;
	peer = chan->priv;
	assert(peer->chan == chan);
	s = peer->s;

	/* Close client code's side of the device */
	ppp_l2tp_server_close_client(peer);

	/* Terminate the L2TP channel */
	if (peer->sess != NULL) {
		ppp_l2tp_terminate(peer->sess, L2TP_RESULT_ADMIN, 0, NULL);
		peer->sess = NULL;
	}

	/* Destroy the device object */
	while ((output = mesg_port_get(chan->outport, 0)) != NULL)
		ppp_l2tp_server_device_free_output(chan, output);
	mesg_port_destroy(&chan->outport);
	FREE(L2TP_DEVICE_MTYPE, chan);
	peer->chan = NULL;

	/* Destroy peer if it has no more references */
	if (peer->ctrl == NULL)
		ppp_l2tp_server_peer_destroy(&peer);
}

static void
ppp_l2tp_server_device_free_output(struct ppp_channel *chan,
	struct ppp_channel_output *output)
{
	FREE(L2TP_OUTPUT_MTYPE, output->info);
	FREE(L2TP_OUTPUT_MTYPE, output);
}

static void
ppp_l2tp_server_device_set_link_info(struct ppp_channel *chan, u_int32_t accm)
{
	/* XXX implement me? */
}

static int
ppp_l2tp_server_device_get_origination(struct ppp_channel *chan)
{
	return (PPP_PEER);	/* we don't initiate any calls ourself */
}

static const char *
ppp_l2tp_server_device_get_node(struct ppp_channel *chan)
{
	struct ppp_l2tp_peer *const peer = chan->priv;

	return (peer->node);
}

static const char *
ppp_l2tp_server_device_get_hook(struct ppp_channel *chan)
{
	struct ppp_l2tp_peer *const peer = chan->priv;

	return (peer->hook);
}

static int
ppp_l2tp_server_device_is_async(struct ppp_channel *chan)
{
	return (0);
}

static u_int
ppp_l2tp_server_device_get_mtu(struct ppp_channel *chan)
{
	return (L2TP_MRU);
}

static int
ppp_l2tp_server_device_get_acfcomp(struct ppp_channel *chan)
{
	return (1);
}

static int
ppp_l2tp_server_device_get_pfcomp(struct ppp_channel *chan)
{
	return (1);
}

