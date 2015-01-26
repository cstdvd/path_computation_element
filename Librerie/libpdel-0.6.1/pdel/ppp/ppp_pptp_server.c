
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
#include "ppp/ppp_pptp_server.h"
#include "ppp/ppp_pptp_ctrl.h"
#include "ppp/ppp_pptp_ctrl_defs.h"

#include <net/ethernet.h>
#include <netgraph/ng_pptpgre.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#define PPTP_MTYPE		"ppp_pptp"
#define PPTP_OUTPUT_MTYPE	"ppp_pptp.output"
#define PPTP_DEVICE_MTYPE	"ppp_pptp.device"
#define PPTP_PEER_MTYPE		"ppp_pptp.peer"

#if 0			/* win2k seems to require at least 1500 */
#define PPTP_MRU							\
    (ETHER_MAX_LEN		/* standard ethernet frame */		\
	- ETHER_CRC_LEN		/* ethernet crc */			\
	- ETHER_HDR_LEN		/* ethernet header */			\
	- sizeof(struct ip)	/* ip header */				\
	- 16			/* gre header */			\
	- 2)			/* ppp address & control fields */
#else
#define PPTP_MRU		LCP_DEFAULT_MRU
#endif

#define PPTP_MRRU		LCP_DEFAULT_MRRU

#define PPTPGRE_ALWAYS_ACK	1

/* PPTP server info */
struct pptp_server {
	struct ppp_pptp_server_info	info;	/* client info */
	struct ppp_log			*log;	/* log */
	struct ppp_engine		*engine;/* associated ppp engine */
	struct pptp_engine		*pptp;	/* pptp control engine */
	struct pevent_ctx		*ev_ctx;/* event context */
	pthread_mutex_t			*mutex;	/* mutex */
	struct in_addr			ip;	/* my ip address */
	u_int16_t			port;	/* my port */
	u_char				shutdown;/* server is shutdown */
	u_int				npeers;	/* number of extant devices */
};

/* Remote peer info */
struct ppp_pptp_peer {
	struct pptp_server		*s;	/* back pointer to server */
	struct ppp_channel		*chan;	/* back pointer to channel */
	struct pptpctrlinfo		cinfo;	/* pptp control channel info */
	struct nodeinfo			ninfo;	/* ng_pptpgre(4) node info */
	struct pevent			*answer;	/* pptp answer event */
	char				path[32];	/* pptpgre node path */
	char				logname[32];	/* peer logname */
	void				*carg;	/* client callback arg */
	struct in_addr			ip;	/* peer remote ip address */
	u_int16_t			port;	/* peer remote port */
	u_char				closed;	/* closed by client side */
};

/* PPTP control callbacks */
static PptpCheckNewConn_t	ppp_pptp_server_check_new_conn;
static PptpGetInLink_t		ppp_pptp_server_get_in_link;
static PptpGetOutLink_t		ppp_pptp_server_get_out_link;

/* Device methods */
static ppp_channel_open_t		ppp_pptp_server_device_open;
static ppp_channel_close_t		ppp_pptp_server_device_close;
static ppp_channel_destroy_t		ppp_pptp_server_device_destroy;
static ppp_channel_free_output_t	ppp_pptp_server_device_free_output;
static ppp_channel_set_link_info_t	ppp_pptp_server_device_set_link_info;
static ppp_channel_get_origination_t	ppp_pptp_server_device_get_origination;
static ppp_channel_get_node_t		ppp_pptp_server_device_get_node;
static ppp_channel_get_hook_t		ppp_pptp_server_device_get_hook;
static ppp_channel_is_async_t		ppp_pptp_server_device_is_async;
static ppp_channel_get_mtu_t		ppp_pptp_server_device_get_mtu;
static ppp_channel_get_acfcomp_t	ppp_pptp_server_device_get_acfcomp;
static ppp_channel_get_pfcomp_t		ppp_pptp_server_device_get_pfcomp;

static struct ppp_channel_meth ppp_pptp_server_device_meth = {
	ppp_pptp_server_device_open,
	ppp_pptp_server_device_close,
	ppp_pptp_server_device_destroy,
	ppp_pptp_server_device_free_output,
	ppp_pptp_server_device_set_link_info,
	ppp_pptp_server_device_get_origination,
	ppp_pptp_server_device_get_node,
	ppp_pptp_server_device_get_hook,
	ppp_pptp_server_device_is_async,
	ppp_pptp_server_device_get_mtu,
	ppp_pptp_server_device_get_acfcomp,
	ppp_pptp_server_device_get_pfcomp,
};

/* Other internal functions */
static struct	ppp_pptp_peer *ppp_pptp_server_new_peer(struct pptp_server *s,
			struct in_addr ip, u_int16_t port,
			const struct pptpctrlinfo *cinfo);
static void	ppp_pptp_server_device_output(struct ppp_pptp_peer *peer,
			enum ppp_channeloutput type, ...);

static void	ppp_pptp_server_cancel(void *cookie);
static void	ppp_pptp_server_result(void *cookie, const char *errmsg);

static pevent_handler_t	ppp_pptp_server_answer;

/* Macro for logging */
#define LOG(sev, fmt, args...)	PPP_LOG(s->log, sev, fmt , ## args)

/***********************************************************************
			PUBLIC FUNCTIONS
***********************************************************************/

/*
 * Start the PPTP server associated with a ppp engine.
 */
int
ppp_pptp_server_start(struct ppp_engine *engine,
	const struct ppp_pptp_server_info *info,
	struct in_addr ip, u_int16_t port, u_int max_conn)
{
	struct ppp_log *const elog = ppp_engine_get_log(engine);
	struct pptp_server *s;

	/* Sanity */
	if (engine == NULL || info->arg == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* See if server already exists */
	if ((s = ppp_engine_get_pptp_server(engine)) != NULL) {
		errno = EALREADY;
		return (-1);
	}

	/* Create new server */
	if ((s = MALLOC(PPTP_MTYPE, sizeof(*s))) == NULL)
		return (-1);
	memset(s, 0, sizeof(*s));
	s->engine = engine;
	s->ev_ctx = ppp_engine_get_ev_ctx(engine);
	s->mutex = ppp_engine_get_mutex(engine);
	s->log = ppp_log_dup(elog);
	s->info = *info;
	s->ip = ip;
	s->port = port;

	/* Start PPTP */
	if ((s->pptp = PptpCtrlInit(s, s->ev_ctx, s->mutex,
	    ppp_pptp_server_check_new_conn, ppp_pptp_server_get_in_link,
	    ppp_pptp_server_get_out_link, ip, port, info->vendor,
	    ppp_log_dup(elog), 1)) == NULL) {
		ppp_log_put(elog, LOG_ERR, "failed to initialize pptp");
		FREE(PPTP_MTYPE, s);
		return (-1);
	}

	/* Enable incoming connections */
	if (PptpCtrlListen(s->pptp, 1) == -1) {
		ppp_log_put(elog, LOG_ERR, "failed start pptp server");
		PptpCtrlShutdown(&s->pptp);
		FREE(PPTP_MTYPE, s);
		return (-1);
	}

	/* Done */
	ppp_engine_set_pptp_server(engine, s);
	return (0);
}

/*
 * Stop the PPTP server associated with a ppp engine.
 *
 * We can't completely destroy it, because there may be PPTP devices
 * in use by ppp_link's that still exist. The ppp_link's are responsible
 * for destroying their devices, not us.
 */
void
ppp_pptp_server_stop(struct ppp_engine *engine)
{
	struct pptp_server *s;

	if ((s = ppp_engine_get_pptp_server(engine)) == NULL)
		return;
	ppp_engine_set_pptp_server(s->engine, NULL);
	PptpCtrlShutdown(&s->pptp);
	if (s->npeers == 0) {
		ppp_log_close(&s->log);
		FREE(PPTP_MTYPE, s);
		return;
	}

	/* Wait for all devices to be destroyed */
	s->shutdown = 1;
}

/*
 * Close a PPTP connection.
 */
void
ppp_pptp_server_close(struct ppp_engine *engine, struct ppp_pptp_peer **peerp)
{
	struct ppp_pptp_peer *const peer = *peerp;

	if (peer == NULL)
		return;
	*peerp = NULL;
	peer->carg = NULL;		/* don't call client 'destroy' method */
	ppp_pptp_server_device_close(peer->chan);
}

/*
 * Get the client handle for the PPTP channel associated with a device.
 */
void *
ppp_pptp_server_get_client_info(struct ppp_channel *chan)
{
	struct ppp_pptp_peer *const peer = chan->priv;

	if (chan->meth != &ppp_pptp_server_device_meth) {
		errno = EINVAL;
		return (NULL);
	}
	if (peer->carg == NULL)
		errno = ENXIO;
	return (peer->carg);
}

/***********************************************************************
			PPTP CONTROL CALLBACKS
***********************************************************************/

static int
ppp_pptp_server_check_new_conn(void *arg, struct in_addr ip,
	u_int16_t port, char *logname, size_t max)
{
	struct pptp_server *const s = arg;

	if (s->info.getlogname != NULL)
		(*s->info.getlogname)(s->info.arg, ip, port, logname, max);
	return (0);
}

static struct pptplinkinfo
ppp_pptp_server_get_in_link(void *arg, struct pptpctrlinfo cinfo,
	struct in_addr ip, u_int16_t port, int bearType,
	const char *callingNum, const char *calledNum, const char *subAddress)
{
	struct pptp_server *const s = arg;
	struct pptplinkinfo info;
	struct ppp_pptp_peer *peer;

	/* Create new peer */
	memset(&info, 0, sizeof(info));
	if ((peer = ppp_pptp_server_new_peer(s, ip, port, &cinfo)) == NULL)
		return (info);

	/* Fill in response */
	info.cookie = peer;
	info.cancel = ppp_pptp_server_cancel;
	info.result = ppp_pptp_server_result;

	/* Output 'up' message */
	ppp_pptp_server_device_output(peer, PPP_CHANNEL_OUTPUT_UP);

	/* Done */
	return (info);
}

static struct pptplinkinfo
ppp_pptp_server_get_out_link(void *arg, struct pptpctrlinfo cinfo,
	struct in_addr ip, u_int16_t port, int bearType, int frameType,
	int minBps, int maxBps, const char *calledNum, const char *subAddress)
{
	struct pptp_server *const s = arg;
	struct pptplinkinfo info;
	struct ppp_pptp_peer *peer;

	/* Create new peer */
	memset(&info, 0, sizeof(info));
	if ((peer = ppp_pptp_server_new_peer(s, ip, port, &cinfo)) == NULL)
		return (info);

	/* Set up to 'answer' the outgoing call (but not reentrantly) */
	pevent_unregister(&peer->answer);
	if (pevent_register(s->ev_ctx, &peer->answer, 0, s->mutex,
	    ppp_pptp_server_answer, peer, PEVENT_TIME, 0) == -1) {
		LOG(LOG_ERR, "pevent_register: %m");
		return (info);
	}

	/* Fill in response */
	info.cookie = peer;
	info.cancel = ppp_pptp_server_cancel;
	info.result = ppp_pptp_server_result;

	/* Done */
	return (info);
}

static void
ppp_pptp_server_result(void *cookie, const char *errmsg)
{
	struct ppp_pptp_peer *const peer = cookie;
	struct pptp_server *const s = peer->s;

	LOG(LOG_INFO, "call from %s terminated: %s", peer->logname, errmsg);
	peer->cinfo.cookie = NULL;	/* don't call cinfo close() method */
	ppp_pptp_server_device_close(peer->chan);
}

static void
ppp_pptp_server_cancel(void *cookie)
{
	struct ppp_pptp_peer *const peer = cookie;

	(void)peer;
	assert(0);
}

/***********************************************************************
			INTERNAL FUNCTIONS
***********************************************************************/

/*
 * Create a new PPTP peer object corresponding to a PPTP channel
 * and start up a new PPP link/bundle.
 */
static struct ppp_pptp_peer *
ppp_pptp_server_new_peer(struct pptp_server *s, struct in_addr ip,
	u_int16_t port, const struct pptpctrlinfo *cinfo)
{
	union {
	    u_char repbuf[sizeof(struct ng_mesg) + sizeof(struct nodeinfo)];
	    struct ng_mesg reply;
	} repbuf;
	struct ng_mesg *reply = &repbuf.reply;
	struct ppp_link_config link_config;
	struct ng_pptpgre_conf greconf;
	struct ppp_pptp_peer *peer = NULL;
	struct ppp_auth_config auth;
	struct ppp_log *log = NULL;
	struct in_addr pptp_ip[2];
	struct ngm_mkpeer mkpeer;
	struct ngm_rmhook rmhook;
	int csock = -1;
	int esave;

	/* Create peer info structure */
	if ((peer = MALLOC(PPTP_PEER_MTYPE, sizeof(*peer))) == NULL) {
		LOG(LOG_ERR, "can't allocate new device: %m");
		goto fail;
	}
	memset(peer, 0, sizeof(*peer));
	peer->s = s;
	peer->cinfo = *cinfo;
	peer->ip = ip;
	peer->port = port;

	/* Create ng_pptpgre(4) node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1) {
		LOG(LOG_ERR, "can't create socket node: %m");
		goto fail;
	}
	memset(&mkpeer, 0, sizeof(mkpeer));
	strlcpy(mkpeer.type, NG_PPTPGRE_NODE_TYPE, sizeof(mkpeer.type));
	strlcpy(mkpeer.ourhook, NG_PPTPGRE_HOOK_UPPER, sizeof(mkpeer.ourhook));
	strlcpy(mkpeer.peerhook,
	    NG_PPTPGRE_HOOK_UPPER, sizeof(mkpeer.peerhook));
	if (NgSendMsg(csock, ".",
	    NGM_GENERIC_COOKIE, NGM_MKPEER, &mkpeer, sizeof(mkpeer)) == -1) {
		LOG(LOG_ERR, "can't create pptpgre node: %m");
		goto fail;
	}
	snprintf(peer->path, sizeof(peer->path), "%s", NG_PPTPGRE_HOOK_UPPER);

	/* Get node info including 'id' and create absolute path for node */
	if (NgSendMsg(csock, NG_PPTPGRE_HOOK_UPPER,
	    NGM_GENERIC_COOKIE, NGM_NODEINFO, NULL, 0) == -1) {
		LOG(LOG_ERR, "can't get node info: %m");
		goto fail;
	}
	memset(&repbuf, 0, sizeof(repbuf));
	if (NgRecvMsg(csock, reply, sizeof(repbuf), NULL) == -1) {
		LOG(LOG_ERR, "can't read node info: %m");
		goto fail;
	}
	memcpy(&peer->ninfo, reply->data, sizeof(peer->ninfo));
	snprintf(peer->path, sizeof(peer->path),
	    "[%lx]:", (long)peer->ninfo.id);

	/* Check with client library */
	strlcpy(peer->logname, inet_ntoa(ip), sizeof(peer->logname));
	memset(&auth, 0, sizeof(auth));
	if ((peer->carg = (*s->info.admit)(s->info.arg, peer, ip, port,
	    &auth, peer->logname, sizeof(peer->logname))) == NULL)
		goto fail;

	/* Get PPTP session info */
	memset(&greconf, 0, sizeof(greconf));
	if (PptpCtrlGetSessionInfo(cinfo, &pptp_ip[PPP_SELF],
	    &pptp_ip[PPP_PEER], &greconf.cid, &greconf.peerCid,
	    &greconf.recvWin, &greconf.peerPpd) == -1) {
		LOG(LOG_ERR, "can't get pptp session info: %m");
		goto fail;
	}

	/* Configure pptpgre node */
	greconf.enabled = 1;
	greconf.enableDelayedAck = 1;
#if PPTPGRE_ALWAYS_ACK
	greconf.enableAlwaysAck = 1;
#endif
	if (NgSendMsg(csock, peer->path, NGM_PPTPGRE_COOKIE,
	    NGM_PPTPGRE_SET_CONFIG, &greconf, sizeof(greconf)) == -1) {
		LOG(LOG_ERR, "can't configure pptpgre node: %m");
		goto fail;
	}

	/* Plumb the 'lower' side of the ng_pptpgre(4) node */
	if ((*s->info.plumb)(s->info.arg,
	    peer->carg, peer->path, NG_PPTPGRE_HOOK_LOWER, pptp_ip) == -1) {
		LOG(LOG_ERR, "error plumbing node: %m");
		goto fail;
	}

	/* Disconnect from the node so the link can connect to it */
	memset(&rmhook, 0, sizeof(rmhook));
	strlcpy(rmhook.ourhook, NG_PPTPGRE_HOOK_UPPER, sizeof(rmhook.ourhook));
	if (NgSendMsg(csock, ".", NGM_GENERIC_COOKIE,
	      NGM_RMHOOK, &rmhook, sizeof(rmhook)) == -1) {
		LOG(LOG_ERR, "can't unhook from node: %m");
		goto fail;
	}

	/* Create a new PPP device for this pptp connection */
	if ((peer->chan = MALLOC(PPTP_DEVICE_MTYPE,
	    sizeof(*peer->chan))) == NULL) {
		LOG(LOG_ERR, "can't allocate new channel: %m");
		goto fail;
	}
	memset(peer->chan, 0, sizeof(*peer->chan));
	peer->chan->meth = &ppp_pptp_server_device_meth;
	peer->chan->priv = peer;

	/* Create device output message port */
	if ((peer->chan->outport
	    = mesg_port_create("ppp_pptp_server")) == NULL) {
		LOG(LOG_ERR, "can't create mesg_port: %m");
		goto fail;
	}

	/* Create log for the new link by prefixing the engine's log */
	if ((log = ppp_engine_get_log(s->engine)) != NULL
	    && (log = ppp_log_prefix(log, "%s: ", peer->logname)) == NULL) {
		LOG(LOG_ERR, "can't create link log: %m");
		goto fail;
	}

	/* Configure new link */
	memset(&link_config, 0, sizeof(link_config));
	link_config.auth = auth;
	link_config.max_self_mru = PPTP_MRU;
	link_config.max_self_mrru = PPTP_MRRU;
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

	/* Done */
	(void)close(csock);
	s->npeers++;
	return (peer);

fail:
	/* Clean up after failure */
	esave = errno;
	ppp_log_close(&log);
	if (peer != NULL) {
		if (peer->carg != NULL)
			(*s->info.destroy)(s->info.arg, peer->carg, peer->path);
		if (peer->chan != NULL) {
			if (peer->chan->outport != NULL)
				mesg_port_destroy(&peer->chan->outport);
			FREE(PPTP_DEVICE_MTYPE, peer->chan);
		}
		if (csock != -1) {
			(void)NgSendMsg(csock, peer->path,
			    NGM_GENERIC_COOKIE, NGM_SHUTDOWN, NULL, 0);
		}
		FREE(PPTP_PEER_MTYPE, peer);
	}
	if (csock != -1)
		(void)close(csock);
	errno = esave;
	return (NULL);
}

/*
 * "Answer" peer's outgoing call.
 */
static void
ppp_pptp_server_answer(void *arg)
{
	struct ppp_pptp_peer *const peer = arg;

	pevent_unregister(&peer->answer);
	(*peer->cinfo.answer)(peer->cinfo.cookie, PPTP_OCR_RESL_OK,
	    0, 0, 10000000 /* XXX */);
	ppp_pptp_server_device_output(peer, PPP_CHANNEL_OUTPUT_UP);
}

/*
 * Output indication from the device.
 */
static void
ppp_pptp_server_device_output(struct ppp_pptp_peer *peer,
	enum ppp_channeloutput type, ...)
{
	struct pptp_server *const s = peer->s;
	struct ppp_channel_output *output;

	/* Get output object */
	if ((output = MALLOC(PPTP_OUTPUT_MTYPE, sizeof(*output))) == NULL) {
		LOG(LOG_ERR, "can't create pptp output: %m");
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
		if ((output->info = STRDUP(PPTP_OUTPUT_MTYPE, msg)) == NULL) {
			LOG(LOG_ERR, "can't create pptp output: %m");
			FREE(PPTP_OUTPUT_MTYPE, output);
			return;
		}
		break;
	    }
	case PPP_CHANNEL_OUTPUT_UP:
		break;
	}

	/* Send message */
	if (mesg_port_put(peer->chan->outport, output) == -1) {
		LOG(LOG_ERR, "can't send pptp output: %m");
		ppp_pptp_server_device_free_output(peer->chan, output);
		return;
	}
}

/***********************************************************************
			PPTP DEVICE METHODS
***********************************************************************/

static void
ppp_pptp_server_device_open(struct ppp_channel *chan)
{
	return;
}

static void
ppp_pptp_server_device_close(struct ppp_channel *chan)
{
	struct ppp_pptp_peer *const peer = chan->priv;
	struct pptp_server *const s = peer->s;
	struct ng_pptpgre_conf greconf;
	int csock;

	/* Logging */
	if (!peer->closed) {
		LOG(LOG_INFO, "closing PPTP connection with %s", peer->logname);
		peer->closed = 1;
	}

	/* Disable the pptpgre device */
	if (NgMkSockNode(NULL, &csock, NULL) == -1)
		LOG(LOG_ERR, "can't create socket node: %m");
	else {
		memset(&greconf, 0, sizeof(greconf));
		(void)NgSendMsg(csock, peer->path, NGM_PPTPGRE_COOKIE,
		    NGM_PPTPGRE_SET_CONFIG, &greconf, sizeof(greconf));
		(void)close(csock);
	}

	/* Output 'down' message from the device */
	ppp_pptp_server_device_output(peer,
	    PPP_CHANNEL_OUTPUT_DOWN_FATAL, "administratively closed");
}

static void
ppp_pptp_server_device_destroy(struct ppp_channel **chanp)
{
	struct ppp_channel *const chan = *chanp;
	struct ppp_channel_output *output;
	struct ppp_pptp_peer *peer;
	struct pptp_server *s;
	int csock;

	/* Sanity */
	if (chan == NULL)
		return;
	*chanp = NULL;
	peer = chan->priv;
	s = peer->s;

	/* Close client code's side of the device */
	if (peer->carg != NULL) {
		(*s->info.destroy)(s->info.arg, peer->carg, peer->path);
		peer->carg = NULL;
	}

	/* Close the PPTP channel */
	pevent_unregister(&peer->answer);
	if (peer->cinfo.cookie != NULL) {
		(*peer->cinfo.close)(peer->cinfo.cookie,
		    PPTP_CDN_RESL_ADMIN, 0, 0);
		peer->cinfo.cookie = NULL;
	}

	/* Destroy the ng_pptpgre(4) node */
	if (NgMkSockNode(NULL, &csock, NULL) == -1)
		LOG(LOG_ERR, "can't create socket node: %m");
	else {
		(void)NgSendMsg(csock, peer->path,
		    NGM_GENERIC_COOKIE, NGM_SHUTDOWN, NULL, 0);
		(void)close(csock);
	}

	/* Destroy the 'peer' object */
	FREE(PPTP_PEER_MTYPE, peer);

	/* Destroy the device object */
	while ((output = mesg_port_get(chan->outport, 0)) != NULL)
		ppp_pptp_server_device_free_output(chan, output);
	mesg_port_destroy(&chan->outport);
	FREE(PPTP_DEVICE_MTYPE, chan);
	s->npeers--;

	/* Check if shutting down PPTP server */
	if (s->npeers == 0 && s->shutdown) {
		ppp_log_close(&s->log);
		FREE(PPTP_MTYPE, s);
	}
}

static void
ppp_pptp_server_device_free_output(struct ppp_channel *chan,
	struct ppp_channel_output *output)
{
	FREE(PPTP_OUTPUT_MTYPE, output->info);
	FREE(PPTP_OUTPUT_MTYPE, output);
}

static void
ppp_pptp_server_device_set_link_info(struct ppp_channel *chan, u_int32_t accm)
{
	/* XXX implement me? */
}

static int
ppp_pptp_server_device_get_origination(struct ppp_channel *chan)
{
	return (PPP_PEER);	/* we don't initiate any calls ourself */
}

static const char *
ppp_pptp_server_device_get_node(struct ppp_channel *chan)
{
	struct ppp_pptp_peer *const peer = chan->priv;

	return (peer->path);
}

static const char *
ppp_pptp_server_device_get_hook(struct ppp_channel *chan)
{
	return (NG_PPTPGRE_HOOK_UPPER);
}

static int
ppp_pptp_server_device_is_async(struct ppp_channel *chan)
{
	return (0);
}

static u_int
ppp_pptp_server_device_get_mtu(struct ppp_channel *chan)
{
	return (PPTP_MRU);
}

static int
ppp_pptp_server_device_get_acfcomp(struct ppp_channel *chan)
{
	return (1);
}

static int
ppp_pptp_server_device_get_pfcomp(struct ppp_channel *chan)
{
	return (1);
}

