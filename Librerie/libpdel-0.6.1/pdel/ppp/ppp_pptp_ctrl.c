
/*
 * Copyright (c) 1995-1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty,
 * use and redistribution of this software, in source or object code
 * forms, with or without modifications are expressly permitted by
 * Whistle Communications; provided, however, that:   (i) any and
 * all reproductions of the source or object code must include the
 * copyright notice above and the following disclaimer of warranties;
 * and (ii) no rights are granted, in any manner or form, to use
 * Whistle Communications, Inc. trademarks, including the mark "WHISTLE
 * COMMUNICATIONS" on advertising, endorsements, or otherwise except
 * as such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS",
 * AND TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS
 * MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED,
 * REGARDING THIS SOFTWARE, INCLUDING WITHOUT LIMITATION, ANY AND
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT.  WHISTLE COMMUNICATIONS DOES NOT
 * WARRANT, GUARANTEE, OR MAKE ANY REPRESENTATIONS REGARDING THE USE
 * OF, OR THE RESULTS OF THE USE OF THIS SOFTWARE IN TERMS OF ITS
 * CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.  IN NO EVENT
 * SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES RESULTING
 * FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED
 * AND UNDER ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS
 * IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: <archie@freebsd.org>
 */

#include "ppp/ppp_defs.h"
#include "ppp/ppp_log.h"
#include "ppp/ppp_pptp_ctrl.h"
#include "ppp/ppp_pptp_ctrl_defs.h"
#include <netinet/tcp.h>

/*
 * DEFINITIONS
 */

  #define RANDOMIZE_CID			1
  #define CHECK_RESERVED_FIELDS		0
  #define LOGNAME_MAX			32

#ifndef FALSE
  #define FALSE				0
  #define TRUE				1
#endif

  #define PPTP_MTYPE			"ppp_pptp_ctrl"
  #define PPTP_CTRL_MTYPE		"ppp_pptp_ctrl.ctrl"
  #define PPTP_CHAN_MTYPE		"ppp_pptp_ctrl.chan"
  #define PPTP_PREP_MTYPE		"ppp_pptp_ctrl.prep"

  #define PPTP_FIRMWARE_REV		0x0101

  #define PPTP_STR_INTERNAL_CALLING	"Internally originated VPN call"

  /* Limits on how long we wait for replies to things */
  #define PPTP_DFL_REPLY_TIME		PPTP_IDLE_TIMEOUT
  #define PPTP_OUTCALLREQ_REPLY_TIME	60
  #define PPTP_INCALLREP_REPLY_TIME	60
  #define PPTP_STOPCCR_REPLY_TIME	3

  /* Logging */
  #define PLOG(sev, fmt, args...)	\
	      ppp_log_put(pptp->log, sev, fmt , ## args)
  #define CLOG(sev, fmt, args...)	\
	      ppp_log_put(c->log, sev, fmt , ## args)
  #define CHLOG(sev, fmt, args...)	\
	      ppp_log_put(ch->log, sev, fmt , ## args)

  struct pptp_engine;

  /* This describes how/if a reply is required */
  struct pptpreqrep {
    u_char	reply;			/* required reply (or zero) */
    u_char	killCtrl;		/* fatal to ctrl or just to channel */
    u_short	timeout;		/* max time to wait for reply */
  };
  typedef struct pptpreqrep	*PptpReqRep;

  /* This represents a pending reply we're waiting for */
  struct pptppendrep {
    const struct pptpmsginfo	*request;	/* original message info */
    struct pptpctrl		*ctrl;		/* control channel */
    struct pptpchan		*chan;		/* channel (NULL if none) */
    struct pevent		*timer;		/* reply timeout timer */
    struct pptppendrep		*next;		/* next in list */
  };
  typedef struct pptppendrep	*PptpPendRep;

  /* This describes how to match a message to the corresponding channel */
  struct pptpchanid {
    u_char		findIn;		/* how to find channel (incoming) */
    u_char		findOut;	/* how to find channel (outgoing) */
    const char		*inField;	/* field used to find channel (in) */
    const char		*outField;	/* field used to find channel (out) */
  };
  typedef struct pptpchanid	*PptpChanId;

  #define PPTP_FIND_CHAN_MY_CID		1	/* match field vs. my cid */
  #define PPTP_FIND_CHAN_PEER_CID	2	/* match field vs. peer cid */
  #define PPTP_FIND_CHAN_PNS_CID	3	/* match field vs. PNS cid */
  #define PPTP_FIND_CHAN_PAC_CID	4	/* match field vs. PAC cid */

  /* Message handler function type */
  typedef void pptpmsghandler_t(void *, void *);

  /* Total info about a message type (except field layout) */
  struct pptpmsginfo {
    const char		*name;		/* name for this message type */
    pptpmsghandler_t	*handler;	/* message handler function */
    u_char		isReply;	/* this is always a reply message */
    u_char		length;		/* length of message (sans header) */
    u_short		states;		/* states which admit this message */
    struct pptpchanid	match;		/* how to find corresponding channel */
    struct pptpreqrep	reqrep;		/* what kind of reply we expect */
  };
  typedef const struct pptpmsginfo	*PptpMsgInfo;

  /* Receive window size XXX */
  #define PPTP_RECV_WIN			16

  /* Packet processing delay XXX */
  #define PPTP_PPD			1

  /* Channel state */
  struct pptpchan {
    u_char		state;		/* channel state */
    u_char		id;		/* channel index */
    u_char		orig:1;		/* call originated from us */
    u_char		incoming:1;	/* call is incoming, not outgoing */
    u_char		killing:1;	/* channel is being killed */
    u_int16_t		cid;		/* my call id */
    u_int16_t		serno;		/* call serial number */
    u_int16_t		peerCid;	/* peer call id */
    u_int16_t		peerPpd;	/* peer's packet processing delay */
    u_int16_t		recvWin;	/* peer's recv window size */
    u_int32_t		recvSeq;	/* last seq # we rcv'd */
    u_int32_t		xmitSeq;	/* last seq # we sent */
    u_int32_t		recvAck;	/* last seq # peer acknowledged */
    u_int32_t		xmitAck;	/* last seq # we acknowledged */
    u_int32_t		bearType;	/* call bearer type */
    u_int32_t		frameType;	/* call framing type */
    u_int32_t		minBps;		/* minimum acceptable speed */
    u_int32_t		maxBps;		/* maximum acceptable speed */
    struct pptplinkinfo	linfo;		/* info about corresponding link */
    struct pptpctrl	*ctrl;		/* my control channel */
    struct ppp_log	*log;		/* log */
    char		callingNum[PPTP_PHONE_LEN + 1];	/* calling number */
    char		calledNum[PPTP_PHONE_LEN + 1];	/* called number */
    char		subAddress[PPTP_SUBADDR_LEN + 1];/* sub-address */
  };
  typedef struct pptpchan	*PptpChan;

  #define PPTP_CHAN_IS_PNS(ch)		(!(ch)->orig ^ !(ch)->incoming)

  /* Control channel state */
  struct pptpctrl {
    u_char		frame[PPTP_CTRL_MAX_FRAME];
    u_char		state;		/* state */
    u_char		id;		/* channel index */
    u_char		orig:1;		/* we originated connection */
    u_char		killing:1;	/* connection is being killed */
    u_int16_t		flen;		/* length of partial frame */
    int			csock;		/* peer control messages */
    struct in_addr	self_addr;	/* local IP address */
    struct in_addr	peer_addr;	/* peer we're talking to */
    u_int16_t		self_port;
    u_int16_t		peer_port;
    struct pptp_engine	*pptp;		/* engine that owns me */
    struct pevent	*connEvent;	/* connection event */
    struct pevent	*ctrlEvent;	/* control connection input */
    struct pevent	*killEvent;	/* kill this ctrl in separate thread */
    struct pevent	*idleTimer;	/* idle timer */
    struct ppp_log	*log;		/* log */
    u_int32_t		echoId;		/* last echo id # sent */
    PptpPendRep		reps;		/* pending replies to msgs */
    PptpChan		*channels;	/* array of channels */
    int			numChannels;	/* length of channels array */
    char		logname[LOGNAME_MAX];	/* name for logging */
  };
  typedef struct pptpctrl	*PptpCtrl;

  #define MAX_IOVEC	32

  /* Our physical channel ID */
  #define PHYS_CHAN(ch)		(((ch)->ctrl->id << 16) | (ch)->id)

/*
 * INTERNAL FUNCTIONS
 */

  /* Methods for each control message type */
  static void	PptpStartCtrlConnRequest(PptpCtrl c,
			struct pptpStartCtrlConnRequest *m);
  static void	PptpStartCtrlConnReply(PptpCtrl c,
			struct pptpStartCtrlConnReply *m);
  static void	PptpStopCtrlConnRequest(PptpCtrl c,
			struct pptpStopCtrlConnRequest *m);
  static void	PptpStopCtrlConnReply(PptpCtrl c,
			struct pptpStopCtrlConnReply *m);
  static void	PptpEchoRequest(PptpCtrl c, struct pptpEchoRequest *m);
  static void	PptpEchoReply(PptpCtrl c, struct pptpEchoReply *m);
  static void	PptpOutCallRequest(PptpCtrl c, struct pptpOutCallRequest *m);
  static void	PptpOutCallReply(PptpChan ch, struct pptpOutCallReply *m);
  static void	PptpInCallRequest(PptpCtrl c, struct pptpInCallRequest *m);
  static void	PptpInCallReply(PptpChan ch, struct pptpInCallReply *m);
  static void	PptpInCallConn(PptpChan ch, struct pptpInCallConn *m);
  static void	PptpCallClearRequest(PptpChan ch,
			struct pptpCallClearRequest *m);
  static void	PptpCallDiscNotify(PptpChan ch, struct pptpCallDiscNotify *m);
  static void	PptpWanErrorNotify(PptpChan ch, struct pptpWanErrorNotify *m);
  static void	PptpSetLinkInfo(PptpChan ch, struct pptpSetLinkInfo *m);

  /* Link layer callbacks */
  static void	PptpCtrlCloseChan(PptpChan ch,
		  int result, int error, int cause);
  static void	PptpCtrlKillChan(PptpChan ch, const char *errmsg);
  static void	PptpCtrlDialResult(void *cookie,
		  int result, int error, int cause, int speed);

  /* Internal event handlers */
  static pevent_handler_t	PptpCtrlListenEvent;
  static pevent_handler_t	PptpCtrlConnEvent;
  static pevent_handler_t	PptpCtrlReadCtrl;

  /* Shutdown routines */
  static void	PptpCtrlCloseCtrl(PptpCtrl c);
  static void	PptpCtrlKillCtrl(PptpCtrl c);

  /* Timer routines */
  static void	PptpCtrlResetIdleTimer(PptpCtrl c);
  static void	PptpCtrlIdleTimeout(void *arg);
  static void	PptpCtrlReplyTimeout(void *arg);

  /* Misc routines */
  static void	PptpCtrlInitCtrl(PptpCtrl c, int orig);
  static void	PptpCtrlMsg(PptpCtrl c, int type, void *msg);
  static void	PptpCtrlWriteMsg(PptpCtrl c, int type, void *msg);
  static int	PptpCtrlGetSocket(struct in_addr ip,
		  u_int16_t port, char *ebuf, size_t elen);

  static void	PptpCtrlSwap(int type, void *buf);
  static void	PptpCtrlDump(int sev, PptpCtrl c, int type, void *msg);
  static void	PptpCtrlDumpBuf(int sev, PptpCtrl c,
		  const void *data, size_t len, const char *fmt, ...);
  static int	PptpCtrlFindField(int type, const char *name, u_int *offset);
  static void	PptpCtrlInitCinfo(PptpChan ch, PptpCtrlInfo ci);

  static void	PptpCtrlNewCtrlState(PptpCtrl c, int new);
  static void	PptpCtrlNewChanState(PptpChan ch, int new);

  static int	PptpCtrlExtendArray(const char *mtype,
			void *arrayp, int esize, int *alenp);

  static struct pptpctrlinfo
			PptpCtrlOrigCall(struct pptp_engine *pptp, int incmg,
			  struct pptplinkinfo linfo, struct in_addr ip,
			  u_int16_t port, const char *logname,
			  int bearType, int frameType, int minBps, int maxBps,
			  const char *callingNum, const char *calledNum,
			  const char *subAddress);

  static PptpCtrl	PptpCtrlGetCtrl(struct pptp_engine *pptp,
			  int orig, struct in_addr peer_addr,
			  u_int16_t peer_port, const char *logname,
			  char *buf, int bsiz);
  static PptpChan	PptpCtrlGetChan(PptpCtrl c, int chanState, int orig,
			  int incoming, int bearType, int frameType, int minBps,
			  int maxBps, const char *callingNum,
			  const char *calledNum, const char *subAddress);
  static PptpChan	PptpCtrlFindChan(PptpCtrl c, int type,
			  void *msg, int incoming);
  static void		PptpCtrlCheckConn(PptpCtrl c);

/*
 * PPTP ENGINE STATE
 */

  /* PPTP engine structure */
  struct pptp_engine {
	  int			listen_sock;	/* listening socket */
	  struct in_addr	listen_addr;	/* listen ip address */
	  u_int16_t		listen_port;	/* listen port */
	  u_char		nocd;		/* no collision detection */
	  struct pevent_ctx	*ev_ctx;	/* event context */
	  pthread_mutex_t	*mutex;		/* mutex */
	  struct pevent		*listen_event;	/* incoming connection event */
	  PptpCheckNewConn_t	*check_new_conn;/* callback for new connectn. */
	  PptpGetInLink_t	*get_in_link;	/* callback for incoming call */
	  PptpGetOutLink_t	*get_out_link;	/* callback for outgoing call */
	  struct ppp_log	*log;		/* log */
	  void			*arg;		/* client arg */
	  u_int16_t		last_call_id;	/* last used call id */
	  PptpCtrl		*ctrl;		/* array of control channels */
	  int			nctrl;		/* length of ctrl array */
	  char			vendor[PPTP_VENDOR_LEN];    /* vendor name */
  };

/*
 * INTERNAL VARIABLES
 */

  /* Control message field layout */
  static const struct pptpfield
    gPptpMsgLayout[PPTP_MAX_CTRL_TYPE][PPTP_CTRL_MAX_FIELDS] =
  {
#define _WANT_PPTP_FIELDS
#include "ppp/ppp_pptp_ctrl_defs.h"
  };

  /* Control channel and call state names */
  static const char		*gPptpCtrlStates[] = {
#define PPTP_CTRL_ST_FREE		0
		    "FREE",
#define PPTP_CTRL_ST_IDLE		1
		    "IDLE",
#define PPTP_CTRL_ST_WAIT_CTL_REPLY	2
		    "WAIT_CTL_REPLY",
#define PPTP_CTRL_ST_WAIT_STOP_REPLY	3
		    "WAIT_STOP_REPLY",
#define PPTP_CTRL_ST_ESTABLISHED	4
		    "ESTABLISHED",
  };

  static const char		*gPptpChanStates[] = {
#define PPTP_CHAN_ST_FREE		0
		    "FREE",
#define PPTP_CHAN_ST_WAIT_IN_REPLY	1
		    "WAIT_IN_REPLY",
#define PPTP_CHAN_ST_WAIT_OUT_REPLY	2
		    "WAIT_OUT_REPLY",
#define PPTP_CHAN_ST_WAIT_CONNECT	3
		    "WAIT_CONNECT",
#define PPTP_CHAN_ST_WAIT_DISCONNECT	4
		    "WAIT_DISCONNECT",
#define PPTP_CHAN_ST_WAIT_ANSWER	5
		    "WAIT_ANSWER",
#define PPTP_CHAN_ST_ESTABLISHED	6
		    "ESTABLISHED",
#define PPTP_CHAN_ST_WAIT_CTRL		7
		    "WAIT_CTRL",
  };

  /* Control message descriptors */
#define CL(s)	(1 << (PPTP_CTRL_ST_ ## s))
#define CH(s)	((1 << (PPTP_CHAN_ST_ ## s)) | 0x8000)

  static const struct pptpmsginfo	gPptpMsgInfo[PPTP_MAX_CTRL_TYPE] = {
    { "PptpMsgHead", NULL,			/* placeholder */
      FALSE, sizeof(struct pptpMsgHead),
    },
    { "StartCtrlConnRequest", (pptpmsghandler_t *)PptpStartCtrlConnRequest,
      FALSE, sizeof(struct pptpStartCtrlConnRequest),
      CL(IDLE),
      { 0, 0 },					/* no associated channel */
      { PPTP_StartCtrlConnReply, TRUE, PPTP_DFL_REPLY_TIME },
    },
    { "StartCtrlConnReply", (pptpmsghandler_t *)PptpStartCtrlConnReply,
      TRUE, sizeof(struct pptpStartCtrlConnReply),
      CL(WAIT_CTL_REPLY),
      { 0, 0 },					/* no associated channel */
      { 0 },					/* no reply expected */
    },
    { "StopCtrlConnRequest", (pptpmsghandler_t *)PptpStopCtrlConnRequest,
      FALSE, sizeof(struct pptpStopCtrlConnRequest),
      CL(WAIT_CTL_REPLY)|CL(WAIT_STOP_REPLY)|CL(ESTABLISHED),
      { 0, 0 },					/* no associated channel */
      { PPTP_StopCtrlConnReply, TRUE, PPTP_STOPCCR_REPLY_TIME },
    },
    { "StopCtrlConnReply", (pptpmsghandler_t *)PptpStopCtrlConnReply,
      TRUE, sizeof(struct pptpStopCtrlConnReply),
      CL(WAIT_STOP_REPLY),
      { 0, 0 },					/* no associated channel */
      { 0 },					/* no reply expected */
    },
    { "EchoRequest", (pptpmsghandler_t *)PptpEchoRequest,
      FALSE, sizeof(struct pptpEchoRequest),
      CL(ESTABLISHED),
      { 0, 0 },					/* no associated channel */
      { PPTP_EchoReply, TRUE, PPTP_DFL_REPLY_TIME },
    },
    { "EchoReply", (pptpmsghandler_t *)PptpEchoReply,
      TRUE, sizeof(struct pptpEchoReply),
      CL(ESTABLISHED),
      { 0, 0 },					/* no associated channel */
      { 0 },					/* no reply expected */
    },
    { "OutCallRequest", (pptpmsghandler_t *)PptpOutCallRequest,
      FALSE, sizeof(struct pptpOutCallRequest),
      CL(ESTABLISHED),
      { 0, PPTP_FIND_CHAN_MY_CID, NULL, "cid" },
      { PPTP_OutCallReply, TRUE, PPTP_OUTCALLREQ_REPLY_TIME },
    },
    { "OutCallReply", (pptpmsghandler_t *)PptpOutCallReply,
      TRUE, sizeof(struct pptpOutCallReply),
      CH(WAIT_OUT_REPLY),
      { PPTP_FIND_CHAN_MY_CID, PPTP_FIND_CHAN_MY_CID, "peerCid", "cid" },
      { 0 },					/* no reply expected */
    },
    { "InCallRequest", (pptpmsghandler_t *)PptpInCallRequest,
      FALSE, sizeof(struct pptpInCallRequest),
      CL(ESTABLISHED),
      { 0, PPTP_FIND_CHAN_MY_CID, NULL, "cid" },
      { PPTP_InCallReply, FALSE, PPTP_DFL_REPLY_TIME },
    },
    { "InCallReply", (pptpmsghandler_t *)PptpInCallReply,
      TRUE, sizeof(struct pptpInCallReply),
      CH(WAIT_IN_REPLY),
      { PPTP_FIND_CHAN_MY_CID, PPTP_FIND_CHAN_MY_CID, "peerCid", "cid" },
      { PPTP_InCallConn, FALSE, PPTP_INCALLREP_REPLY_TIME },
    },
    { "InCallConn", (pptpmsghandler_t *)PptpInCallConn,
      TRUE, sizeof(struct pptpInCallConn),
      CH(WAIT_CONNECT),
      { PPTP_FIND_CHAN_MY_CID, PPTP_FIND_CHAN_PEER_CID, "peerCid", "peerCid" },
      { 0 },					/* no reply expected */
    },
    { "CallClearRequest", (pptpmsghandler_t *)PptpCallClearRequest,
      FALSE, sizeof(struct pptpCallClearRequest),
      CH(WAIT_IN_REPLY)|CH(WAIT_ANSWER)|CH(ESTABLISHED),
      { PPTP_FIND_CHAN_PNS_CID, PPTP_FIND_CHAN_PNS_CID, "cid", "cid" },
      { PPTP_CallDiscNotify, TRUE, PPTP_DFL_REPLY_TIME },
    },
    { "CallDiscNotify", (pptpmsghandler_t *)PptpCallDiscNotify,
      FALSE, sizeof(struct pptpCallDiscNotify),
      CH(WAIT_OUT_REPLY)|CH(WAIT_CONNECT)|CH(WAIT_DISCONNECT)|CH(ESTABLISHED),
      { PPTP_FIND_CHAN_PAC_CID, PPTP_FIND_CHAN_PAC_CID, "cid", "cid" },
      { 0 },					/* no reply expected */
    },
    { "WanErrorNotify", (pptpmsghandler_t *)PptpWanErrorNotify,
      FALSE, sizeof(struct pptpWanErrorNotify),
      CH(ESTABLISHED),
      { PPTP_FIND_CHAN_PNS_CID, PPTP_FIND_CHAN_PNS_CID, "cid", "cid" },
      { 0 },					/* no reply expected */
    },
    { "SetLinkInfo", (pptpmsghandler_t *)PptpSetLinkInfo,
      FALSE, sizeof(struct pptpSetLinkInfo),
      CH(ESTABLISHED),
      { PPTP_FIND_CHAN_PAC_CID, PPTP_FIND_CHAN_PAC_CID, "cid", "cid" },
      { 0 },					/* no reply expected */
    },
  };

#undef CL
#undef CH

  /* Error code to string converters */
  #define DECODE(a, n)	((u_int)(n) < (sizeof(a) / sizeof(*(a))) ? \
			    (a)[(u_int)(n)] : "[out of range]")

  static const char	*const gPptpErrorCodes[] = {
    "none",
    "not connected",
    "bad format",
    "bad value",
    "no resource",
    "bad call ID",
    "pac error",
  };
  #define PPTP_ERROR_CODE(n)		DECODE(gPptpErrorCodes, (n))

  static const char	*const gPptpSccrReslCodes[] = {
    "zero?",
    "OK",
    "general error",
    "channel exists",
    "not authorized",
    "bad protocol version",
  };
  #define PPTP_SCCR_RESL_CODE(n)	DECODE(gPptpSccrReslCodes, (n))

  static const char	*const gPptpSccrReasCodes[] = {
    "zero?",
    "none",
    "bad protocol version",
    "local shutdown",
  };
  #define PPTP_SCCR_REAS_CODE(n)	DECODE(gPptpSccrReasCodes, (n))

  static const char	*const gPptpEchoReslCodes[] = {
    "zero?",
    "OK",
    "general error",
  };
  #define PPTP_ECHO_RESL_CODE(n)	DECODE(gPptpEchoReslCodes, (n))

  static const char	*const gPptpOcrReslCodes[] = {
    "zero?",
    "OK",
    "general error",
    "no carrier",
    "busy",
    "no dialtone",
    "timed out",
    "admin prohib",
  };
  #define PPTP_OCR_RESL_CODE(n)		DECODE(gPptpOcrReslCodes, (n))

  static const char	*const gPptpIcrReslCodes[] = {
    "zero?",
    "OK",
    "general error",
    "not accepted",
  };
  #define PPTP_ICR_RESL_CODE(n)		DECODE(gPptpIcrReslCodes, (n))

  static const char	*const gPptpCdnReslCodes[] = {
    "zero?",
    "lost carrier",
    "general error",
    "admin action",
    "disconnect request",
  };
  #define PPTP_CDN_RESL_CODE(n)		DECODE(gPptpCdnReslCodes, (n))

/*************************************************************************
			EXPORTED FUNCTIONS
*************************************************************************/

/*
 * PptpCtrlInit()
 *
 * Initialize PPTP state and set up callbacks. This must be called
 * first, and any calls after the first will ignore the ip parameter.
 * Returns 0 if successful, -1 otherwise.
 *
 * Parameters:
 *   arg	Client opaque argument.
 *   getInLink	Function to call when a peer has requested to establish
 *		an incoming call. If returned cookie is NULL, call failed.
 *		This pointer may be NULL to deny all incoming calls.
 *   getOutLink	Function to call when a peer has requested to establish
 *		an outgoming call. If returned cookie is NULL, call failed.
 *		This pointer may be NULL to deny all outgoing calls.
 *   ip		The IP address for my server to use (cannot be zero).
 *   port	The TCP port for my server to use (zero for default).
 *   log	The log to use. Note: the log is consumed.
 */

struct pptp_engine *
PptpCtrlInit(void *arg, struct pevent_ctx *ev_ctx, pthread_mutex_t *mutex,
  PptpCheckNewConn_t *checkNewConn, PptpGetInLink_t *getInLink,
  PptpGetOutLink_t *getOutLink, struct in_addr ip, u_int16_t port,
  const char *vendor, struct ppp_log *log, int nocd)
{
  struct pptp_engine	*pptp;
  int			type;

  /* Sanity check structure lengths and valid state bits */
  for (type = 0; type < PPTP_MAX_CTRL_TYPE; type++) {
    PptpField	field = gPptpMsgLayout[type];
    int		total;

    assert((gPptpMsgInfo[type].match.inField != NULL)
      ^ !(gPptpMsgInfo[type].states & 0x8000));
    for (total = 0; field->name; field++)
      total += field->length;
    assert(total == gPptpMsgInfo[type].length);
  }

  /* Create new pptp object */
  if ((pptp = MALLOC(PPTP_MTYPE, sizeof(*pptp))) == NULL)
    return(NULL);
  memset(pptp, 0, sizeof(*pptp));
  pptp->ev_ctx = ev_ctx;
  pptp->mutex = mutex;
  pptp->log = log;
  pptp->nocd = nocd;
  pptp->arg = arg;
  pptp->listen_sock = -1;
  pptp->check_new_conn = checkNewConn;
  pptp->get_in_link = getInLink;
  pptp->get_out_link = getOutLink;
  pptp->listen_addr = ip;
  pptp->listen_port = port ? port : PPTP_PORT;
  if (vendor != NULL)
	strlcpy(pptp->vendor, vendor, sizeof(pptp->vendor));
#if RANDOMIZE_CID
  pptp->last_call_id = time(NULL) ^ (getpid() << 5);
#endif

  /* Done */
  return(pptp);
}

/*
 * PptpCtrlShutdown()
 *
 * Destroy a PPTP engine
 */

void
PptpCtrlShutdown(struct pptp_engine **enginep)
{
	struct pptp_engine *const pptp = *enginep;
	int i;

	if (pptp == NULL)
		return;
	*enginep = NULL;
	if (pptp->listen_sock != -1)
	    (void)close(pptp->listen_sock);
	for (i = 0; i < pptp->nctrl; i++) {
		if (pptp->ctrl[i] == NULL)
			continue;
		PptpCtrlKillCtrl(pptp->ctrl[i]);
	}
	pevent_unregister(&pptp->listen_event);
	ppp_log_close(&pptp->log);
	FREE(PPTP_MTYPE, pptp->ctrl);
	FREE(PPTP_MTYPE, pptp);
}

/*
 * PptpCtrlListen()
 *
 * Enable or disable incoming PPTP TCP connections.
 * Returns 0 if successful, -1 otherwise.
 */

int
PptpCtrlListen(struct pptp_engine *pptp, int enable)
{
  char ebuf[128];

  /* Enable or disable */
  if (enable) {

    /* Already enabled? */
    if (pptp->listen_sock != -1)
      return(0);

    /* We must have a callback for incoming connections */
    if (pptp->check_new_conn == NULL) {
      errno = ENXIO;
      return(-1);
    }

    /* Create listening socket */
    if ((pptp->listen_sock = PptpCtrlGetSocket(pptp->listen_addr,
	pptp->listen_port, ebuf, sizeof(ebuf))) == -1) {
      PLOG(LOG_ERR, "pptp: can't get listen socket: %s", ebuf);
      return(-1);
    }

    /* Listen for connections */
    if (listen(pptp->listen_sock, 10) == -1) {
	PLOG(LOG_ERR, "pptp: %s: %s", "listen", strerror(errno));
	goto sock_fail;
    }

    /* Wait for incoming connections */
    if (pevent_register(pptp->ev_ctx, &pptp->listen_event, PEVENT_RECURRING,
	pptp->mutex, PptpCtrlListenEvent, pptp, PEVENT_READ,
	pptp->listen_sock) == -1) {
      PLOG(LOG_ERR, "pptp: %s: %s", "pevent_register", strerror(errno));
sock_fail:
      (void)close(pptp->listen_sock);
      pptp->listen_sock = -1;
      return(-1);
    }
  } else {

    /* Already disabled? */
    if (pptp->listen_sock == -1)
      return(0);

    /* Stop listening */
    (void)close(pptp->listen_sock);
    pptp->listen_sock = -1;
    pevent_unregister(&pptp->listen_event);
  }

  /* Done */
  return(0);
}

/*
 * PptpCtrlInCall()
 *
 * Initiate an incoming call
 */

struct pptpctrlinfo
PptpCtrlInCall(struct pptp_engine *engine,
	struct pptplinkinfo linfo, struct in_addr ip, u_int16_t port,
	const char *logname, int bearType, int frameType, int minBps,
	int maxBps, const char *callingNum, const char *calledNum,
	const char *subAddress)
{
  return(PptpCtrlOrigCall(engine, TRUE, linfo, ip, port, logname,
    bearType, frameType, minBps, maxBps, callingNum, calledNum, subAddress));
}

/*
 * PptpCtrlOutCall()
 *
 * Initiate an outgoing call
 */

struct pptpctrlinfo
PptpCtrlOutCall(struct pptp_engine *engine,
	struct pptplinkinfo linfo,
	struct in_addr ip, u_int16_t port, const char *logname,
	int bearType, int frameType, int minBps, int maxBps,
	const char *calledNum, const char *subAddress)
{
  return(PptpCtrlOrigCall(engine, FALSE, linfo, ip, port, logname,
    bearType, frameType, minBps, maxBps, PPTP_STR_INTERNAL_CALLING,
    calledNum, subAddress));
}

/*
 * PptpCtrlOrigCall()
 *
 * Request from the PPTP peer at ip:port the establishment of an
 * incoming or outgoing call (as viewed by the peer). The "result"
 * callback will be called when the connection has been established
 * or failed to do so. This initiates a TCP control connection if
 * needed; otherwise it uses the existing connection. If port is
 * zero, then use the normal PPTP port.
 */

static struct pptpctrlinfo
PptpCtrlOrigCall(struct pptp_engine *pptp, int incoming,
	struct pptplinkinfo linfo, struct in_addr ip, u_int16_t port,
	const char *logname, int bearType, int frameType, int minBps,
	int maxBps, const char *callingNum, const char *calledNum,
	const char *subAddress)
{
  PptpCtrl		c;
  PptpChan		ch;
  struct pptpctrlinfo	cinfo;
  char			ebuf[100];

  /* Init */
  port = port ? port : PPTP_PORT;
  memset(&cinfo, 0, sizeof(cinfo));

  /* Sanity check */
  if (linfo.result == NULL) {
    PLOG(LOG_ERR, "pptp: can't originate call without 'result' callback");
    errno = EINVAL;
    return(cinfo);
  }

  /* Find/create control block */
  if ((c = PptpCtrlGetCtrl(pptp, TRUE, ip, port,
      logname, ebuf, sizeof(ebuf))) == NULL) {
    PLOG(LOG_INFO, "pptp: %s", ebuf);
    return(cinfo);
  }

  /* Get new channel */
  if ((ch = PptpCtrlGetChan(c, PPTP_CHAN_ST_WAIT_CTRL, TRUE, incoming,
      bearType, frameType, minBps, maxBps,
      callingNum, calledNum, subAddress)) == NULL) {
    PptpCtrlKillCtrl(c);
    return(cinfo);
  }
  ch->linfo = linfo;

  /* Control channel may be ready already; start channel if so */
  PptpCtrlCheckConn(c);

  /* Return OK */
  PptpCtrlInitCinfo(ch, &cinfo);
  return(cinfo);
}

/*
 * PptpCtrlGetSessionInfo()
 *
 * Returns information associated with a call.
 */

int
PptpCtrlGetSessionInfo(const struct pptpctrlinfo *cp,
	struct in_addr *selfAddr, struct in_addr *peerAddr,
	u_int16_t *selfCid, u_int16_t *peerCid,
	u_int16_t *peerWin, u_int16_t *peerPpd)
{
  PptpChan	const ch = (PptpChan)cp->cookie;

  switch (ch->state) {
    case PPTP_CHAN_ST_WAIT_IN_REPLY:
    case PPTP_CHAN_ST_WAIT_OUT_REPLY:
    case PPTP_CHAN_ST_WAIT_CONNECT:
    case PPTP_CHAN_ST_WAIT_DISCONNECT:
    case PPTP_CHAN_ST_WAIT_ANSWER:
    case PPTP_CHAN_ST_ESTABLISHED:
    case PPTP_CHAN_ST_WAIT_CTRL:
      {
	PptpCtrl	const c = ch->ctrl;

	if (selfAddr != NULL)
	  *selfAddr = c->self_addr;
	if (peerAddr != NULL)
	  *peerAddr = c->peer_addr;
	if (selfCid != NULL)
	  *selfCid = ch->cid;
	if (peerCid != NULL)
	  *peerCid = ch->peerCid;
	if (peerWin != NULL)
	  *peerWin = ch->recvWin;
	if (peerPpd != NULL)
	  *peerPpd = ch->peerPpd;
	return(0);
     }
    case PPTP_CHAN_ST_FREE:
      errno = ENXIO;
      return(-1);
      break;
    default:
      assert(0);
  }
  return(-1);	/* NOTREACHED */
}

/*************************************************************************
			CONTROL CONNECTION SETUP
*************************************************************************/

/*
 * PptpCtrlListenEvent()
 *
 * Someone has connected to our TCP socket on which we were listening.
 */

static void
PptpCtrlListenEvent(void *cookie)
{
  struct pptp_engine	*const pptp = cookie;
  struct sockaddr_in	peer;
  int			psize = sizeof(peer);
  char			logname[LOGNAME_MAX];
  char			ebuf[100];
  PptpCtrl		c;
  int			sock;

  /* Accept connection */
  if ((sock = accept(pptp->listen_sock,
      (struct sockaddr *)&peer, &psize)) == -1)
    return;
  (void)fcntl(sock, F_SETFD, 1);

  /* See if accepting connection is ok */
  snprintf(logname, sizeof(logname), "%s:%u",
    inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
  if ((*pptp->check_new_conn)(pptp->arg, peer.sin_addr,
      ntohs(peer.sin_port), logname, sizeof(logname)) != 0) {
    PLOG(LOG_INFO, "pptp connection from %s rejected", logname);
    (void)close(sock);
    return;
  }

  /* Initialize a new control block */
  PLOG(LOG_INFO, "pptp connection from %s", logname);
  if ((c = PptpCtrlGetCtrl(pptp, FALSE, peer.sin_addr,
      ntohs(peer.sin_port), logname, ebuf, sizeof(ebuf))) == NULL) {
    PLOG(LOG_ERR, "pptp connection failed: %s", ebuf);
    (void)close(sock);
    return;
  }
  c->csock = sock;

  /* Initialize the session */
  PptpCtrlInitCtrl(c, FALSE);
}

/*
 * PptpCtrlConnEvent()
 *
 * We are trying to make a TCP connection to the peer. When this
 * either succeeds or fails, we jump to here.
 */

static void
PptpCtrlConnEvent(void *cookie)
{
  PptpCtrl		const c = (PptpCtrl) cookie;
  struct sockaddr_in	addr;
  int			addrLen = sizeof(addr);

  /* Unregister event */
  assert(c->state == PPTP_CTRL_ST_IDLE);
  pevent_unregister(&c->connEvent);

  /* Check whether the connection was successful or not */
  if (getpeername(c->csock, (struct sockaddr *) &addr, &addrLen) < 0) {
    CLOG(LOG_INFO, "connection to peer failed");
    PptpCtrlKillCtrl(c);
    return;
  }

  /* Initialize the session */
  CLOG(LOG_INFO, "successfully connected to peer");
  PptpCtrlInitCtrl(c, TRUE);
}

/*
 * PptpCtrlInitCtrl()
 *
 * A TCP connection has just been established. Initialize the
 * control block for this connection and initiate the session.
 */

static void
PptpCtrlInitCtrl(PptpCtrl c, int orig)
{
  struct pptp_engine	*const pptp = c->pptp;
  struct sockaddr_in	self, peer;
  static const int	one = 1;
  int			k, addrLen;

  /* Good time for a sanity check */
  assert(c->state == PPTP_CTRL_ST_IDLE);
  assert(c->connEvent == NULL);
  assert(c->ctrlEvent == NULL);
  assert(c->reps == NULL);
  for (k = 0; k < c->numChannels; k++) {
    assert(c->channels[k] == NULL
      || c->channels[k]->state == PPTP_CHAN_ST_WAIT_CTRL);
  }

  /* Initialize control state */
  c->orig = orig;
  c->echoId = 0;
  c->flen = 0;

  /* Get local IP address */
  addrLen = sizeof(self);
  if (getsockname(c->csock, (struct sockaddr *) &self, &addrLen) < 0) {
    CLOG(LOG_ERR, "%s: %s", "getsockname", strerror(errno));
abort:
    PptpCtrlKillCtrl(c);
    return;
  }
  c->self_addr = self.sin_addr;
  c->self_port = ntohs(self.sin_port);

  /* Get remote IP address */
  addrLen = sizeof(peer);
  if (getpeername(c->csock, (struct sockaddr *) &peer, &addrLen) < 0) {
    CLOG(LOG_ERR, "%s: %s", "getpeername", strerror(errno));
    goto abort;
  }
  c->peer_addr = peer.sin_addr;
  c->peer_port = ntohs(peer.sin_port);

  /* Turn of Nagle algorithm on the TCP socket, since we are going to
     be writing complete control frames one at a time */
  if (setsockopt(c->csock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
    CLOG(LOG_ERR, "%s: %s", "setsockopt", strerror(errno));

  /* Register for events on control and data sockets */
  if (pevent_register(pptp->ev_ctx, &c->ctrlEvent, PEVENT_RECURRING,
      pptp->mutex, PptpCtrlReadCtrl, c, PEVENT_READ, c->csock) == -1) {
    CLOG(LOG_ERR, "%s: %s", "pevent_register", strerror(errno));
    goto abort;
  }

  /* Start echo keep-alive timer */
  PptpCtrlResetIdleTimer(c);

  /* If we originated the call, we start the conversation */
  if (c->orig) {
    struct pptpStartCtrlConnRequest	msg;

    memset(&msg, 0, sizeof(msg));
    msg.vers = PPTP_PROTO_VERS;
    msg.frameCap = PPTP_FRAMECAP_SYNC;
    msg.bearCap = PPTP_BEARCAP_ANY;
    msg.firmware = PPTP_FIRMWARE_REV;
    gethostname(msg.host, sizeof(msg.host));
    strncpy(msg.vendor, pptp->vendor, sizeof(msg.vendor));
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_WAIT_CTL_REPLY);
    PptpCtrlWriteMsg(c, PPTP_StartCtrlConnRequest, &msg);
  }
}

/*************************************************************************
		CONTROL CONNECTION MESSAGE HANDLING
*************************************************************************/

/*
 * PptpCtrlReadCtrl()
 */

static void
PptpCtrlReadCtrl(void *cookie)
{
  PptpCtrl	const c = (PptpCtrl) cookie;
  PptpMsgHead	const hdr = (PptpMsgHead) c->frame;
  int		nread;

  /* Figure how much to read and read it */
  nread = (c->flen < sizeof(*hdr) ? sizeof(*hdr) : hdr->length) - c->flen;
  if ((nread = read(c->csock, c->frame + c->flen, nread)) <= 0) {
    if (nread < 0)
      CLOG(LOG_ERR, "%s: %s", "read", strerror(errno));
    else
      CLOG(LOG_INFO, "control connection closed by peer");
    goto abort;
  }
  PptpCtrlDumpBuf(LOG_DEBUG + 1, c,
    c->frame + c->flen, nread, "read ctrl data");
  c->flen += nread;

  /* Do whatever with what we got */
  if (c->flen < sizeof(*hdr))			/* incomplete header */
    return;
  if (c->flen == sizeof(*hdr)) {		/* complete header */
    PptpCtrlSwap(0, hdr);		/* byte swap header */
    CLOG(LOG_DEBUG + 1, "rec'd hdr");
    PptpCtrlDump(LOG_DEBUG + 1, c, 0, hdr);
    if (hdr->msgType != PPTP_CTRL_MSG_TYPE) {
      CLOG(LOG_NOTICE, "rec'd invalid msg type %d", hdr->msgType);
      goto abort;
    }
    if (hdr->magic != PPTP_MAGIC) {
      CLOG(LOG_NOTICE, "rec'd invalid magic 0x%08x", hdr->type);
      goto abort;
    }
    if (!PPTP_VALID_CTRL_TYPE(hdr->type)) {
      CLOG(LOG_NOTICE, "rec'd invalid ctrl type %d", hdr->type);
      goto abort;
    }
#if CHECK_RESERVED_FIELDS
    if (hdr->resv0 != 0) {
      CLOG(LOG_NOTICE, "rec'd non-zero reserved field in header");
      goto abort;
    }
#endif
    if (hdr->length != sizeof(*hdr) + gPptpMsgInfo[hdr->type].length) {
      CLOG(LOG_NOTICE, "rec'd invalid length %u for type %s",
	hdr->length, gPptpMsgInfo[hdr->type].name);
abort:
      PptpCtrlKillCtrl(c);
      return;
    }
    return;
  }
  if (c->flen == hdr->length) {			/* complete message */
    void	*const msg = ((u_char *) hdr) + sizeof(*hdr);

    PptpCtrlSwap(hdr->type, msg);		/* byte swap message */
    CLOG(LOG_DEBUG, "recv %s", gPptpMsgInfo[hdr->type].name);
    PptpCtrlDump(LOG_DEBUG, c, hdr->type, msg);
    c->flen = 0;
    PptpCtrlResetIdleTimer(c);
    PptpCtrlMsg(c, hdr->type, msg);
  }
}

/*
 * PptpCtrlMsg()
 *
 * We read a complete control message. Sanity check it and handle it.
 */

static void
PptpCtrlMsg(PptpCtrl c, int type, void *msg)
{
  PptpMsgInfo		const mi = &gPptpMsgInfo[type];
  PptpChan		ch = NULL;
  PptpPendRep		*pp;

#if CHECK_RESERVED_FIELDS
  {
    PptpField		field = gPptpMsgLayout[type];
    u_int		off;
    static u_char	zeros[4];

    /* Make sure all reserved fields are zero */
    for (off = 0; field->name; off += field->length, field++) {
      if (!strncmp(field->name, PPTP_RESV_PREF, strlen(PPTP_RESV_PREF))
	  && memcmp((u_char *) msg + off, zeros, field->length)) {
	CLOG(LOG_INFO, "rec'd non-zero reserved field %s in %s",
	  field->name, mi->name);
	PptpCtrlKillCtrl(c);
	return;
      }
    }
  }
#endif

  /* Find channel this message corresponds to (if any) */
  if (mi->match.inField && !(ch = PptpCtrlFindChan(c, type, msg, TRUE)))
    return;

  /* See if this message qualifies as the reply to a previously sent message */
  for (pp = &c->reps; *pp; pp = &(*pp)->next) {
    if ((*pp)->request->reqrep.reply == type && (*pp)->chan == ch)
      break;
  }

  /* If not, and this message is *always* a reply, ignore it */
  if (*pp == NULL && mi->isReply) {
    CLOG(LOG_NOTICE, "rec'd spurious %s", mi->name);
    return;
  }

  /* If so, cancel the matching pending reply */
  if (*pp) {
    PptpPendRep	const prep = *pp;

    pevent_unregister(&prep->timer);
    *pp = prep->next;
    FREE(PPTP_PREP_MTYPE, prep);
  }

  /* Check for invalid message and call or control state combinations */
  if (!ch && ((1 << c->state) & mi->states) == 0) {
    CLOG(LOG_NOTICE, "rec'd %s in %s state %s (not valid)",
      gPptpMsgInfo[type].name, "control channel", gPptpCtrlStates[c->state]);
    PptpCtrlKillCtrl(c);
    return;
  }
  if (ch && ((1 << ch->state) & mi->states) == 0) {
    CHLOG(LOG_NOTICE, "rec'd %s in %s state %s (not valid)",
      gPptpMsgInfo[type].name, "channel", gPptpChanStates[ch->state]);
    PptpCtrlKillCtrl(c);
    return;
  }

  /* Things look OK; process message */
  (*mi->handler)(ch ? (void *) ch : (void *) c, msg);
}

/*
 * PptpCtrlWriteMsg()
 *
 * Write out a control message. If we should expect a reply,
 * register a matching pending reply for it.
 */

static void
PptpCtrlWriteMsg(PptpCtrl c, int type, void *msg)
{
  struct pptp_engine	*const pptp = c->pptp;
  PptpMsgInfo		const mi = &gPptpMsgInfo[type];
  u_char		buf[PPTP_CTRL_MAX_FRAME];
  PptpMsgHead		const hdr = (PptpMsgHead) buf;
  u_char		*const payload = (u_char *) (hdr + 1);
  const int		totlen = sizeof(*hdr) + gPptpMsgInfo[type].length;
  int			nwrote;

  /* Build message */
  assert(PPTP_VALID_CTRL_TYPE(type));
  memset(hdr, 0, sizeof(*hdr));
  hdr->length = totlen;
  hdr->msgType = PPTP_CTRL_MSG_TYPE;
  hdr->magic = PPTP_MAGIC;
  hdr->type = type;
  memcpy(payload, msg, gPptpMsgInfo[type].length);
  CLOG(LOG_DEBUG, "send %s msg", gPptpMsgInfo[hdr->type].name);
  PptpCtrlDump(LOG_DEBUG, c, 0, hdr);
  PptpCtrlDump(LOG_DEBUG, c, type, msg);

  /* Byte swap it */
  PptpCtrlSwap(0, hdr);
  PptpCtrlSwap(type, payload);

  /* Send it; if TCP buffer is full, we abort the connection */
  if ((nwrote = write(c->csock, buf, totlen)) != totlen) {
    if (nwrote < 0)
      CLOG(LOG_ERR, "%s: %s", "write", strerror(errno));
    else
      CLOG(LOG_ERR, "only wrote %d/%d", nwrote, totlen);
    goto kill;
  }
  PptpCtrlDumpBuf(LOG_DEBUG + 1, c, buf, totlen, "wrote ctrl data");

  /* If we expect a reply to this message, start expecting it now */
  if (PPTP_VALID_CTRL_TYPE(mi->reqrep.reply)) {
    PptpPendRep	prep;

    if ((prep = MALLOC(PPTP_PREP_MTYPE, sizeof(*prep))) == NULL) {
      CLOG(LOG_ERR, "%s: %s", "malloc", strerror(errno));
      goto kill;
    }
    memset(prep, 0, sizeof(*prep));
    prep->ctrl = c;
    prep->chan = PptpCtrlFindChan(c, type, msg, FALSE);
    prep->request = mi;
    if (pevent_register(pptp->ev_ctx, &prep->timer, 0,
	pptp->mutex, PptpCtrlReplyTimeout, prep, PEVENT_TIME,
	mi->reqrep.timeout * 1000) == -1) {
      CLOG(LOG_ERR, "%s: %s", "pevent_register", strerror(errno));
      FREE(PPTP_PREP_MTYPE, prep);
      goto kill;
    }
    prep->next = c->reps;
    c->reps = prep;
  }

  /* Done */
  return;

kill:
  /* There was an error, kill connection (later) */
  pevent_unregister(&c->killEvent);
  if (pevent_register(pptp->ev_ctx, &c->killEvent, 0, pptp->mutex,
      (pevent_handler_t *)PptpCtrlKillCtrl, c, PEVENT_TIME, 0) == -1)
    CLOG(LOG_ERR, "pevent_register: %m");
}

/*************************************************************************
		CONTROL AND CHANNEL ALLOCATION FUNCTIONS
*************************************************************************/

/*
 * PptpCtrlGetCtrl()
 *
 * Get existing or create new control bock for given peer and return it.
 * Returns NULL if there was some problem, and puts an error message
 * into the buffer.
 *
 * If "orig" is TRUE, and we currently have no TCP connection to the peer,
 * then initiate one. Otherwise, make sure we don't already have one,
 * because that would mean we'd have two connections to the same peer.
 */

static PptpCtrl
PptpCtrlGetCtrl(struct pptp_engine *pptp, int orig, struct in_addr peer_addr,
	u_int16_t peer_port, const char *logname, char *buf, int bsiz)
{
  PptpCtrl			c;
  struct sockaddr_in		peer;
  static const struct in_addr	any = { 0 };
  int				flags;
  int				k;

  /* See if we're already have a control block matching this address and port */
  for (k = 0; k < pptp->nctrl; k++) {
    PptpCtrl	const c = pptp->ctrl[k];

    if (c != NULL
	&& c->peer_addr.s_addr == peer_addr.s_addr
	&& c->peer_port == peer_port) {
      if (orig)
	return(c);
      snprintf(buf, bsiz, "connection to %s already exists", logname);
      return(NULL);
    }
  }

  /* Find/create a free one */
  for (k = 0; k < pptp->nctrl && pptp->ctrl[k] != NULL; k++);
  if (k == pptp->nctrl) {
    if (PptpCtrlExtendArray(PPTP_MTYPE, &pptp->ctrl,
	sizeof(*pptp->ctrl), &pptp->nctrl) == -1) {
      snprintf(buf, bsiz, "%s: %s", "malloc", strerror(errno));
      return(NULL);
    }
  }
  if ((c = MALLOC(PPTP_CTRL_MTYPE, sizeof(*c))) == NULL) {
    snprintf(buf, bsiz, "%s: %s", "malloc", strerror(errno));
    return(NULL);
  }
  memset(c, 0, sizeof(*c));
  pptp->ctrl[k] = c;

  /* Initialize it */
  c->id = k;
  c->pptp = pptp;
  c->orig = orig;
  c->csock = -1;
  c->peer_addr = peer_addr;
  c->peer_port = peer_port;
  strlcpy(c->logname, logname, sizeof(c->logname));
  if ((c->log = ppp_log_prefix(pptp->log, "%s: ", c->logname)) == NULL) {
    snprintf(buf, bsiz, "%s: %s", "ppp_log_prefix", strerror(errno));
    pptp->ctrl[k] = NULL;
    FREE(PPTP_CTRL_MTYPE, c);
    return(NULL);
  }

  /* Go to the idle state */
  PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_IDLE);

  /* If not doing the connecting, return here */
  if (!c->orig)
    return(c);

  /* Get socket */
  if ((c->csock = PptpCtrlGetSocket(any, 0, buf, bsiz)) < 0) {
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_FREE);
    return(NULL);
  }

  /* Put socket in non-blocking mode */
  if (fcntl(c->csock, F_GETFL, &flags) == -1
      || fcntl(c->csock, F_SETFL, flags | O_NONBLOCK) == -1) {
    snprintf(buf, bsiz, "pptp: connect to %s failed: %s: %s",
      logname, "fcntl", strerror(errno));
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_FREE);
    return(NULL);
  }

  /* Initiate connection to peer */
  memset(&peer, 0, sizeof(peer));
  peer.sin_family = AF_INET;
  peer.sin_addr = c->peer_addr;
  peer.sin_port = htons(c->peer_port);
  if (connect(c->csock, (struct sockaddr *) &peer, sizeof(peer)) < 0
      && errno != EINPROGRESS) {
    (void) close(c->csock);
    c->csock = -1;
    snprintf(buf, bsiz, "pptp: connect to %s failed: %s",
      logname, strerror(errno));
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_FREE);
    return(NULL);
  }

  /* Put socket back in blocking mode */
  if (fcntl(c->csock, F_SETFL, flags) == -1) {
    (void) close(c->csock);
    c->csock = -1;
    snprintf(buf, bsiz, "pptp: connect to %s failed: %s: %s",
      logname, "fcntl", strerror(errno));
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_FREE);
    return(NULL);
  }

  /* Wait for it to go through */
  if (pevent_register(pptp->ev_ctx, &c->connEvent, PEVENT_RECURRING,
      pptp->mutex, PptpCtrlConnEvent, c, PEVENT_READ, c->csock) == -1) {
    CLOG(LOG_ERR, "%s: %s", "pevent_register", strerror(errno));
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_FREE);
    return(NULL);
  }

  /* Done */
  CLOG(LOG_INFO, "intiating connection to peer");
  return(c);
}

/*
 * PptpCtrlGetChan()
 *
 * Find a free data channel and attach it to the control channel.
 */

static PptpChan
PptpCtrlGetChan(PptpCtrl c, int chanState, int orig, int incoming,
	int bearType, int frameType, int minBps, int maxBps,
	const char *callingNum, const char *calledNum, const char *subAddress)
{
  struct pptp_engine	*const pptp = c->pptp;
  PptpChan		ch;
  int			k;

  /* Get a free data channel */
  for (k = 0; k < c->numChannels && c->channels[k] != NULL; k++);
  if (k == c->numChannels) {
    if (PptpCtrlExtendArray(PPTP_CTRL_MTYPE, &c->channels,
	sizeof(*c->channels), &c->numChannels) == -1) {
      CLOG(LOG_ERR, "%s: %s", "malloc", strerror(errno));
      return(NULL);
    }
  }
  if ((ch = MALLOC(PPTP_CHAN_MTYPE, sizeof(*ch))) == NULL) {
    CLOG(LOG_ERR, "%s: %s", "malloc", strerror(errno));
    return(NULL);
  }
  memset(ch, 0, sizeof(*ch));
  c->channels[k] = ch;
  ch->id = k;
  ch->cid = ++pptp->last_call_id;
  ch->ctrl = c;
  ch->orig = orig;
  ch->incoming = incoming;
  ch->minBps = minBps;
  ch->maxBps = maxBps;
  ch->bearType = bearType;
  ch->frameType = frameType;
  snprintf(ch->calledNum, sizeof(ch->calledNum), "%s", calledNum);
  snprintf(ch->callingNum, sizeof(ch->callingNum), "%s", callingNum);
  snprintf(ch->subAddress, sizeof(ch->subAddress), "%s", subAddress);

  /* Get log for this channel */
  if ((ch->log = ppp_log_prefix(pptp->log,
      "%s[%d]: ", c->logname, ch->id)) == NULL) {
    CLOG(LOG_ERR, "%s: %s", "ppp_log_prefix", strerror(errno));
    pptp->last_call_id--;
    c->channels[k] = NULL;
    FREE(PPTP_CHAN_MTYPE, ch);
    return(NULL);
  }

  /* Go to starting state */
  PptpCtrlNewChanState(ch, chanState);
  return(ch);
}

/*
 * PptpCtrlDialResult()
 *
 * Link layer calls this to let us know whether an outgoing call
 * has been successfully completed or has failed.
 */

static void
PptpCtrlDialResult(void *cookie, int result, int error, int cause, int speed)
{
  PptpChan			const ch = (PptpChan) cookie;
  PptpCtrl			const c = ch->ctrl;
  struct pptpOutCallReply	rep;

  memset(&rep, 0, sizeof(rep));
  rep.cid = ch->cid;
  rep.peerCid = ch->peerCid;
  rep.result = result;
  if (rep.result == PPTP_OCR_RESL_ERR)
    rep.err = error;
  rep.cause = cause;
  rep.speed = speed;
  rep.ppd = PPTP_PPD;		/* XXX should get this value from link layer */
  rep.recvWin = PPTP_RECV_WIN;	/* XXX */
  rep.channel = PHYS_CHAN(ch);
  PptpCtrlWriteMsg(c, PPTP_OutCallReply, &rep);
  if (rep.result == PPTP_OCR_RESL_OK)
    PptpCtrlNewChanState(ch, PPTP_CHAN_ST_ESTABLISHED);
  else
    PptpCtrlKillChan(ch, "local outgoing call failed");
}

/*************************************************************************
		    SHUTDOWN FUNCTIONS
*************************************************************************/

/*
 * PptpCtrlCloseCtrl()
 */

static void
PptpCtrlCloseCtrl(PptpCtrl c)
{
  CLOG(LOG_INFO, "closing PPTP control connection");
  switch (c->state) {
    case PPTP_CTRL_ST_IDLE:
    case PPTP_CTRL_ST_WAIT_STOP_REPLY:
    case PPTP_CTRL_ST_WAIT_CTL_REPLY:
      PptpCtrlKillCtrl(c);
      return;
    case PPTP_CTRL_ST_ESTABLISHED:
      {
	struct pptpStopCtrlConnRequest	req;

	memset(&req, 0, sizeof(req));
	req.reason = PPTP_SCCR_REAS_LOCAL;
	PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_WAIT_STOP_REPLY);
	PptpCtrlWriteMsg(c, PPTP_StopCtrlConnRequest, &req);
	return;
      }
      break;
    default:
      assert(0);
  }
}

/*
 * PptpCtrlKillCtrl()
 */

static void
PptpCtrlKillCtrl(PptpCtrl c)
{
  PptpPendRep		prep, next;
  int			k;

  /* Cancel kill event */
  pevent_unregister(&c->killEvent);

  /* Don't recurse */
  assert(c);
  if (c->killing || c->state == PPTP_CTRL_ST_FREE)
    return;
  c->killing = 1;

  /* Do ungraceful shutdown */
  CLOG(LOG_DEBUG, "killing control connection");
  for (k = 0; k < c->numChannels; k++) {
    PptpChan	const ch = c->channels[k];

    if (ch != NULL)
      PptpCtrlKillChan(ch, "control channel shutdown");
  }
  if (c->csock >= 0) {
    close(c->csock);
    c->csock = -1;
  }
  pevent_unregister(&c->connEvent);
  pevent_unregister(&c->ctrlEvent);
  pevent_unregister(&c->idleTimer);
  for (prep = c->reps; prep; prep = next) {
    next = prep->next;
    pevent_unregister(&prep->timer);
    FREE(PPTP_PREP_MTYPE, prep);
  }
  c->reps = NULL;
  PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_FREE);
}

/*
 * PptpCtrlCloseChan()
 *
 * Gracefully clear a call.
 */

static void
PptpCtrlCloseChan(PptpChan ch, int result, int error, int cause)
{
  PptpCtrl	const c = ch->ctrl;

  /* Don't recurse */
  if (ch->killing)
    return;

  /* Check call state */
  switch (ch->state) {
    case PPTP_CHAN_ST_ESTABLISHED:
      if (PPTP_CHAN_IS_PNS(ch))
	goto pnsClear;
      else
	goto pacClear;
      break;
    case PPTP_CHAN_ST_WAIT_ANSWER:
      {
	struct pptpOutCallReply	reply;

	memset(&reply, 0, sizeof(reply));
	reply.peerCid = ch->peerCid;
	reply.result = PPTP_OCR_RESL_ADMIN;
	PptpCtrlWriteMsg(c, PPTP_OutCallReply, &reply);
	PptpCtrlKillChan(ch, "link layer shutdown");	/* XXX errmsg */
	return;
      }
      break;
    case PPTP_CHAN_ST_WAIT_IN_REPLY:		/* we are the PAC */
pacClear:
      {
	struct pptpCallDiscNotify	disc;

	CHLOG(LOG_INFO, "clearing call");
	memset(&disc, 0, sizeof(disc));
	disc.cid = ch->cid;
	disc.result = result;
	if (disc.result == PPTP_CDN_RESL_ERR)
	  disc.err = error;
	disc.cause = cause;
	/* XXX stats? */
	PptpCtrlWriteMsg(c, PPTP_CallDiscNotify, &disc);
	PptpCtrlKillChan(ch, "link layer shutdown");	/* XXX errmsg */
      }
      break;
    case PPTP_CHAN_ST_WAIT_OUT_REPLY:		/* we are the PNS */
    case PPTP_CHAN_ST_WAIT_CONNECT:		/* we are the PNS */
pnsClear:
      {
	struct pptpCallClearRequest	req;

	CHLOG(LOG_INFO, "clearing call");
	memset(&req, 0, sizeof(req));
	req.cid = ch->cid;
	PptpCtrlNewChanState(ch, PPTP_CHAN_ST_WAIT_DISCONNECT);
	PptpCtrlWriteMsg(c, PPTP_CallClearRequest, &req);
      }
      break;
    case PPTP_CHAN_ST_WAIT_DISCONNECT:		/* call was already cleared */
      return;
    case PPTP_CHAN_ST_WAIT_CTRL:
      PptpCtrlKillChan(ch, "link layer shutdown");
      return;
    default:
      assert(0);
  }
}

/*
 * PptpCtrlKillChan()
 */

static void
PptpCtrlKillChan(PptpChan ch, const char *errmsg)
{
  PptpCtrl		const c = ch->ctrl;
  PptpPendRep		*pp;
  int			k;

  /* Don't recurse */
  assert(ch);
  if (ch->killing)		/* should never happen anyway */
    return;
  ch->killing = 1;

  /* If link layer needs notification, tell it */
  CHLOG(LOG_DEBUG, "killing channel");
  switch (ch->state) {
    case PPTP_CHAN_ST_WAIT_IN_REPLY:
    case PPTP_CHAN_ST_WAIT_OUT_REPLY:
    case PPTP_CHAN_ST_WAIT_CONNECT:
    case PPTP_CHAN_ST_ESTABLISHED:
    case PPTP_CHAN_ST_WAIT_CTRL:
      (*ch->linfo.result)(ch->linfo.cookie, errmsg);
      break;
    case PPTP_CHAN_ST_WAIT_DISCONNECT:
      break;
    case PPTP_CHAN_ST_WAIT_ANSWER:
      (*ch->linfo.cancel)(ch->linfo.cookie);
      break;
    default:
      assert(0);
  }

  /* Nuke any pending replies pertaining to this channel */
  for (pp = &c->reps; *pp; ) {
    PptpPendRep	const prep = *pp;

    if (prep->chan == ch) {
      pevent_unregister(&prep->timer);
      *pp = prep->next;
      FREE(PPTP_PREP_MTYPE, prep);
    } else
      pp = &prep->next;
  }

  /* Free channel */
  PptpCtrlNewChanState(ch, PPTP_CHAN_ST_FREE);

  /* When the last channel is closed, close the control channel too,
     unless we're already in the process of killing it. */
  for (k = 0; k < c->numChannels; k++) {
    PptpChan	const ch2 = c->channels[k];

    if (ch2 != NULL && ch2->ctrl == c)
      break;
  }
  if (k == c->numChannels
      && c->state == PPTP_CTRL_ST_ESTABLISHED
      && !c->killing)
    PptpCtrlCloseCtrl(c);
}

/*************************************************************************
		    TIMER RELATED FUNCTIONS
*************************************************************************/

/*
 * PptpCtrlReplyTimeout()
 */

static void
PptpCtrlReplyTimeout(void *arg)
{
  PptpPendRep		const prep = (PptpPendRep) arg;
  PptpPendRep		*pp;
  PptpChan		const ch = prep->chan;
  PptpCtrl		const c = prep->ctrl;

  /* Cancel timer event */
  pevent_unregister(&prep->timer);

  /* Log it */
  if (ch) {
    CHLOG(LOG_NOTICE, "no reply to %s after %d sec",
      prep->request->name, prep->request->reqrep.timeout);
  } else if (prep->request - gPptpMsgInfo != PPTP_StopCtrlConnRequest) {
    CLOG(LOG_NOTICE, "no reply to %s after %d sec",
      prep->request->name, prep->request->reqrep.timeout);
  }

  /* Unlink pending reply */
  for (pp = &c->reps; *pp != prep; pp = &(*pp)->next);
  assert(*pp);
  *pp = prep->next;

  /* Either close this channel or kill entire control connection */
  if (prep->request->reqrep.killCtrl)
    PptpCtrlKillCtrl(c);
  else
    PptpCtrlCloseChan(ch, PPTP_CDN_RESL_ERR, PPTP_ERROR_PAC_ERROR, 0);

  /* Done */
  FREE(PPTP_PREP_MTYPE, prep);
}

/*
 * PptpCtrlIdleTimeout()
 *
 * We've heard PPTP_IDLE_TIMEOUT seconds of silence from the peer.
 * Send an echo request to make sure it's alive.
 */

static void
PptpCtrlIdleTimeout(void *arg)
{
  PptpCtrl			const c = (PptpCtrl) arg;
  struct pptpEchoRequest	msg;
  int				k;

  /* Cancel timer event */
  pevent_unregister(&c->idleTimer);

  /* If no channels are left on this control connection, shut it down */
  for (k = 0; k < c->numChannels && c->channels[k] == NULL; k++);
  if (k == c->numChannels) {
    PptpCtrlCloseCtrl(c);
    return;
  }

  /* Send echo request */
  memset(&msg, 0, sizeof(msg));
  msg.id = ++c->echoId;
  PptpCtrlWriteMsg(c, PPTP_EchoRequest, &msg);
}

/*
 * PptpCtrlResetIdleTimer()
 *
 * Reset the idle timer back up to PPTP_IDLE_TIMEOUT seconds.
 */

static void
PptpCtrlResetIdleTimer(PptpCtrl c)
{
  struct pptp_engine	*const pptp = c->pptp;

  pevent_unregister(&c->idleTimer);
  if (pevent_register(pptp->ev_ctx, &c->idleTimer, 0, pptp->mutex,
      PptpCtrlIdleTimeout, c, PEVENT_TIME, PPTP_IDLE_TIMEOUT * 1000) == -1)
    CLOG(LOG_ERR, "%s: %s", "pevent_register", strerror(errno));
}


/*************************************************************************
		    CHANNEL MAINTENANCE FUNCTIONS
*************************************************************************/

/*
 * PptpCtrlCheckConn()
 *
 * Check whether we have any pending connection requests, and whether
 * we can send them yet, or what.
 */

static void
PptpCtrlCheckConn(PptpCtrl c)
{
  int	k;

  switch (c->state) {
    case PPTP_CTRL_ST_IDLE:
    case PPTP_CTRL_ST_WAIT_CTL_REPLY:
    case PPTP_CTRL_ST_WAIT_STOP_REPLY:
      break;
    case PPTP_CTRL_ST_ESTABLISHED:
      for (k = 0; k < c->numChannels; k++) {
	PptpChan	const ch = c->channels[k];

	if (ch == NULL || ch->state != PPTP_CHAN_ST_WAIT_CTRL)
	  continue;
	if (ch->incoming) {
	  struct pptpInCallRequest	req;

	  memset(&req, 0, sizeof(req));
	  req.cid = ch->cid;
	  req.serno = req.cid;
	  req.bearType = PPTP_BEARCAP_DIGITAL;
	  req.channel = PHYS_CHAN(ch);
	  PptpCtrlNewChanState(ch, PPTP_CHAN_ST_WAIT_IN_REPLY);
	  PptpCtrlWriteMsg(c, PPTP_InCallRequest, &req);
	} else {
	  struct pptpOutCallRequest	req;

	  memset(&req, 0, sizeof(req));
	  req.cid = ch->cid;
	  req.serno = req.cid;
	  req.minBps = ch->minBps;
	  req.maxBps = ch->maxBps;
	  req.bearType = ch->bearType;
	  req.frameType = ch->frameType;
	  req.recvWin = PPTP_RECV_WIN;		/* XXX */
	  req.ppd = PPTP_PPD;			/* XXX */
	  req.numLen = strlen(ch->calledNum);
	  strncpy(req.phone, ch->calledNum, sizeof(req.phone));
	  strncpy(req.subaddr, ch->subAddress, sizeof(req.subaddr));
	  PptpCtrlNewChanState(ch, PPTP_CHAN_ST_WAIT_OUT_REPLY);
	  PptpCtrlWriteMsg(c, PPTP_OutCallRequest, &req);
	}
	return;
      }
      break;
    default:
      assert(0);
  }
}

/*
 * PptpCtrlFindChan()
 *
 * Find the channel identified by this message. Returns NULL if
 * the message is not channel specific, or the channel was not found.
 */

static PptpChan
PptpCtrlFindChan(PptpCtrl c, int type, void *msg, int incoming)
{
  PptpMsgInfo	const mi = &gPptpMsgInfo[type];
  const char	*fname = incoming ? mi->match.inField : mi->match.outField;
  const int	how = incoming ? mi->match.findIn : mi->match.findOut;
  u_int16_t	cid;
  int		k, off;

  /* Get the identifying CID field */
  if (!fname)
    return(NULL);
  (void) PptpCtrlFindField(type, fname, &off);		/* we know len == 2 */
  cid = *((u_int16_t *) ((u_char *) msg + off));

  /* Match the CID against our list of active channels */
  for (k = 0; k < c->numChannels; k++) {
    PptpChan	const ch = c->channels[k];
    u_int16_t	tryCid = 0;

    if (ch == NULL)
      continue;
    switch (how) {
      case PPTP_FIND_CHAN_MY_CID:
	tryCid = ch->cid;
	break;
      case PPTP_FIND_CHAN_PEER_CID:
	tryCid = ch->peerCid;
	break;
      case PPTP_FIND_CHAN_PNS_CID:
	tryCid = PPTP_CHAN_IS_PNS(ch) ? ch->cid : ch->peerCid;
	break;
      case PPTP_FIND_CHAN_PAC_CID:
	tryCid = !PPTP_CHAN_IS_PNS(ch) ? ch->cid : ch->peerCid;
	break;
      default:
	assert(0);
    }
    if (cid == tryCid)
      return(ch);
  }

  /* Not found */
  CLOG(LOG_INFO, "CID 0x%04x in %s not found", cid, mi->name);
  return(NULL);
}

/*************************************************************************
			  MISC FUNCTIONS
*************************************************************************/

/*
 * PptpCtrlNewCtrlState()
 */

static void
PptpCtrlNewCtrlState(PptpCtrl c, int new)
{
  struct pptp_engine	*const pptp = c->pptp;

  assert(c->state != new);
  CLOG(LOG_DEBUG, "ctrl state %s --> %s",
    gPptpCtrlStates[c->state], gPptpCtrlStates[new]);
  if (new == PPTP_CTRL_ST_FREE) {
    pptp->ctrl[c->id] = NULL;
    ppp_log_close(&c->log);
    FREE(PPTP_CTRL_MTYPE, c->channels);
    memset(c, 0, sizeof(*c));
    FREE(PPTP_CTRL_MTYPE, c);
    return;
  }
  c->state = new;
}

/*
 * PptpCtrlNewChanState()
 */

static void
PptpCtrlNewChanState(PptpChan ch, int new)
{
  PptpCtrl	const c = ch->ctrl;

  assert(ch->state != new);
  CHLOG(LOG_DEBUG, "chan state %s --> %s",
    gPptpChanStates[ch->state], gPptpChanStates[new]);
  switch (new) {
    case PPTP_CHAN_ST_FREE:
      c->channels[ch->id] = NULL;
      ppp_log_close(&ch->log);
      memset(ch, 0, sizeof(*ch));
      FREE(PPTP_CHAN_MTYPE, ch);
      return;
    case PPTP_CHAN_ST_WAIT_IN_REPLY:
    case PPTP_CHAN_ST_WAIT_OUT_REPLY:
    case PPTP_CHAN_ST_WAIT_CONNECT:
    case PPTP_CHAN_ST_WAIT_DISCONNECT:
    case PPTP_CHAN_ST_WAIT_ANSWER:
    case PPTP_CHAN_ST_ESTABLISHED:
    case PPTP_CHAN_ST_WAIT_CTRL:
      break;
  }
  ch->state = new;
}

/*
 * PptpCtrlSwap()
 *
 * Byteswap message between host order <--> network order. Note:
 * this relies on the fact that ntohs == htons and ntohl == htonl.
 */

static void
PptpCtrlSwap(int type, void *buf)
{
  PptpField	field = gPptpMsgLayout[type];
  int		off;

  for (off = 0; field->name; off += field->length, field++) {
    switch (field->length) {
      case 4:
	{
	  u_int32_t	*const valp = (u_int32_t *) ((u_char *) buf + off);

	  *valp = ntohl(*valp);
	}
	break;
      case 2:
	{
	  u_int16_t	*const valp = (u_int16_t *) ((u_char *) buf + off);

	  *valp = ntohs(*valp);
	}
	break;
    }
  }
}

/*
 * PptpCtrlDump()
 *
 * Debugging display of a control message.
 */

#define DUMP_DING	65
#define DUMP_MAX_DEC	100
#define DUMP_MAX_BUF	200

static void
PptpCtrlDump(int sev, PptpCtrl c, int type, void *msg)
{
  struct pptp_engine	*const pptp = c->pptp;
  PptpField		field = gPptpMsgLayout[type];
  char			line[DUMP_MAX_BUF];
  int			off;

  for (*line = off = 0; field->name; off += field->length, field++) {
    u_char	*data = (u_char *) msg + off;
    char	buf[DUMP_MAX_BUF];
    const char	*fmt;

    if (!strncmp(field->name, PPTP_RESV_PREF, strlen(PPTP_RESV_PREF)))
      continue;
    snprintf(buf, sizeof(buf), " %s=", field->name);
    switch (field->length) {
      case 4:
	fmt = (*((u_int16_t *) data) <= DUMP_MAX_DEC) ? "%d" : "0x%x";
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	  fmt, *((u_int32_t *) data));
	break;
      case 2:
	fmt = (*((u_int16_t *) data) <= DUMP_MAX_DEC) ? "%d" : "0x%x";
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	  fmt, *((u_int16_t *) data));
	break;
      case 1:
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	  "%d", *((u_int8_t *) data));
	break;
      default:
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	  "\"%s\"", (char *) data);
	break;
    }
    if (*line && strlen(line) + strlen(buf) > DUMP_DING) {
      PLOG(sev, " %s", line);
      *line = 0;
    }
    snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s", buf);
  }
  if (*line)
    PLOG(sev, " %s", line);
}

/*
 * PptpCtrlDumpBuf()
 *
 * Debugging display of a some bytes with a prefix line.
 */

static void
PptpCtrlDumpBuf(int sev, PptpCtrl c,
  const void *data, size_t len, const char *fmt, ...)
{
  struct pptp_engine	*const pptp = c->pptp;
  va_list args;

  va_start(args, fmt);
  ppp_log_vput(pptp->log, sev, fmt, args);
  va_end(args);
  ppp_log_dump(pptp->log, sev, data, len);
}

/*
 * PptpCtrlFindField()
 *
 * Locate a field in a structure, returning length and offset (*offset).
 * Die if field was not found.
 */

static int
PptpCtrlFindField(int type, const char *name, u_int *offp)
{
  PptpField	field = gPptpMsgLayout[type];
  u_int		off;

  for (off = 0; field->name; off += field->length, field++) {
    if (!strcmp(field->name, name)) {
      *offp = off;
      return(field->length);
    }
  }
  assert(0);
  return(0);
}

/*
 * PptpCtrlInitCinfo()
 */

static void
PptpCtrlInitCinfo(PptpChan ch, PptpCtrlInfo ci)
{
  PptpCtrl	const c = ch->ctrl;

  memset(ci, 0, sizeof(*ci));
  ci->cookie = ch;
  ci->peer_addr = c->peer_addr;
  ci->peer_port = c->peer_port;
  ci->close = (void (*)(void *, int, int, int))PptpCtrlCloseChan;
  ci->answer = PptpCtrlDialResult;
}

/*
 * PptpCtrlExtendArray()
 *
 * Extend an array
 */

static int
PptpCtrlExtendArray(const char *mtype, void *array, int esize, int *alenp)
{
  void **const arrayp = (void **)array;
  void *newa;

  if ((newa = REALLOC(mtype, *arrayp, (*alenp + 1) * esize)) == NULL)
    return(-1);
  *arrayp = newa;
  memset((u_char *)*arrayp + (*alenp * esize), 0, esize);
  (*alenp)++;
  return(0);
}

/*
 * PptpCtrlGetSocket()
 *
 * Get and (if port != 0) bind a TCP socket.
 */

static int
PptpCtrlGetSocket(struct in_addr ip, u_int16_t port, char *ebuf, size_t elen)
{
  static const int	one = 1;
  struct sockaddr_in	addr;
  int			sock;
  int			addr_size = sizeof(addr);

  /* Get socket */
  if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    snprintf(ebuf, elen, "socket: %s", strerror(errno));
    return(-1);
  }
  (void)fcntl(sock, F_SETFD, 1);

  /* Set reusable address and port */
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
    snprintf(ebuf, elen, "setsockopt: %s", strerror(errno));
    (void)close(sock);
    return(-1);
  }
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one))) {
    snprintf(ebuf, elen, "setsockopt: %s", strerror(errno));
    (void)close(sock);
    return(-1);
  }

  /* Bind socket */
  if (port != 0) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = ip;
    addr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *) &addr, addr_size) == -1) {
      snprintf(ebuf, elen, "bind: %s", strerror(errno));
      (void)close(sock);
      return(-1);
    }
  }

  /* Done */
  return(sock);
}

/*************************************************************************
		      CONTROL MESSAGE FUNCTIONS
*************************************************************************/

/*
 * PptpStartCtrlConnRequest()
 */

static void
PptpStartCtrlConnRequest(PptpCtrl c, struct pptpStartCtrlConnRequest *req)
{
  struct pptp_engine		*const pptp = c->pptp;
  struct pptpStartCtrlConnReply	reply;
  int				k;

  /* Check for a collision */
  if (!pptp->nocd) {
    for (k = 0; k < pptp->nctrl; k++) {
      PptpCtrl	const c2 = pptp->ctrl[k];
      int		iwin;

      if (c2 == NULL
	  || c2 == c
	  || c2->peer_addr.s_addr != c->peer_addr.s_addr)
	continue;
      iwin = (u_int32_t) ntohl(c->self_addr.s_addr)
	      > (u_int32_t) ntohl(c->peer_addr.s_addr);
      CLOG(LOG_INFO, "collision with peer! %s", iwin ? "i win" : "peer wins");
      if (iwin)
	goto abort;		/* Kill this peer-initiated connection */
      else
	PptpCtrlKillCtrl(c2);	/* Kill the connection that I initiated */
    }
  }

  /* Initialize reply */
  memset(&reply, 0, sizeof(reply));
  reply.vers = PPTP_PROTO_VERS;
  reply.frameCap = PPTP_FRAMECAP_SYNC;
  reply.bearCap = PPTP_BEARCAP_ANY;
  reply.firmware = PPTP_FIRMWARE_REV;
  reply.result = PPTP_SCCR_RESL_OK;
  gethostname(reply.host, sizeof(reply.host));
  strncpy(reply.vendor, pptp->vendor, sizeof(reply.vendor));

  /* Check protocol version */
  if (req->vers != PPTP_PROTO_VERS) {
    CLOG(LOG_ERR, "incompatible PPTP protocol version 0x%04x", req->vers);
    reply.result = PPTP_SCCR_RESL_VERS;
    PptpCtrlWriteMsg(c, PPTP_StartCtrlConnReply, &reply);
abort:
    PptpCtrlKillCtrl(c);
    return;
  }

  /* OK */
  PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_ESTABLISHED);
  PptpCtrlWriteMsg(c, PPTP_StartCtrlConnReply, &reply);
}

/*
 * PptpStartCtrlConnReply()
 */

static void
PptpStartCtrlConnReply(PptpCtrl c, struct pptpStartCtrlConnReply *rep)
{

  /* Is peer happy? */
  if (rep->result != PPTP_SCCR_RESL_OK) {
    CLOG(LOG_NOTICE, "my %s failed, result=%s err=%s",
      gPptpMsgInfo[PPTP_StartCtrlConnRequest].name,
      PPTP_SCCR_RESL_CODE(rep->result), PPTP_ERROR_CODE(rep->err));
    PptpCtrlKillCtrl(c);
    return;
  }

  /* Check peer's protocol version */
  if (rep->vers != PPTP_PROTO_VERS) {
    struct pptpStopCtrlConnRequest	req;

    CLOG(LOG_ERR, "incompatible PPTP protocol version 0x%04x", rep->vers);
    memset(&req, 0, sizeof(req));
    req.reason = PPTP_SCCR_REAS_PROTO;
    PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_WAIT_STOP_REPLY);
    PptpCtrlWriteMsg(c, PPTP_StopCtrlConnRequest, &req);
    return;
  }

  /* OK */
  PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_ESTABLISHED);
  PptpCtrlCheckConn(c);
}

/*
 * PptpStopCtrlConnRequest()
 */

static void
PptpStopCtrlConnRequest(PptpCtrl c, struct pptpStopCtrlConnRequest *req)
{
  struct pptpStopCtrlConnReply	rep;

  CLOG(LOG_INFO, "rec'd %s: reason=%s",
    gPptpMsgInfo[PPTP_StopCtrlConnRequest].name,
    PPTP_SCCR_REAS_CODE(req->reason));
  memset(&rep, 0, sizeof(rep));
  rep.result = PPTP_SCCR_RESL_OK;
  PptpCtrlNewCtrlState(c, PPTP_CTRL_ST_IDLE);
  PptpCtrlWriteMsg(c, PPTP_StopCtrlConnReply, &rep);
  PptpCtrlKillCtrl(c);
}

/*
 * PptpStopCtrlConnReply()
 */

static void
PptpStopCtrlConnReply(PptpCtrl c, struct pptpStopCtrlConnReply *rep)
{
  PptpCtrlKillCtrl(c);
}

/*
 * PptpEchoRequest()
 */

static void
PptpEchoRequest(PptpCtrl c, struct pptpEchoRequest *req)
{
  struct pptpEchoReply	reply;

  memset(&reply, 0, sizeof(reply));
  reply.id = req->id;
  reply.result = PPTP_ECHO_RESL_OK;
  PptpCtrlWriteMsg(c, PPTP_EchoReply, &reply);
}

/*
 * PptpEchoReply()
 */

static void
PptpEchoReply(PptpCtrl c, struct pptpEchoReply *rep)
{
  if (rep->result != PPTP_ECHO_RESL_OK) {
    CLOG(LOG_NOTICE, "echo reply failed: res=%s err=%s",
      PPTP_ECHO_RESL_CODE(rep->result), PPTP_ERROR_CODE(rep->err));
    PptpCtrlKillCtrl(c);
  } else if (rep->id != c->echoId) {
    CLOG(LOG_NOTICE, "bogus echo reply: %u != %u", rep->id, c->echoId);
    PptpCtrlKillCtrl(c);
  }
}

/*
 * PptpOutCallRequest()
 */

static void
PptpOutCallRequest(PptpCtrl c, struct pptpOutCallRequest *req)
{
  struct pptp_engine		*const pptp = c->pptp;
  struct pptpOutCallReply	reply;
  struct pptpctrlinfo		cinfo;
  struct pptplinkinfo		linfo;
  PptpChan			ch = NULL;
  char				calledNum[PPTP_PHONE_LEN + 1];
  char				subAddress[PPTP_SUBADDR_LEN + 1];

  /* Does link allow this? */
  if (pptp->get_out_link == NULL)
    goto denied;

  /* Copy out fields */
  strncpy(calledNum, req->phone, sizeof(calledNum) - 1);
  calledNum[sizeof(calledNum) - 1] = 0;
  strncpy(subAddress, req->subaddr, sizeof(subAddress) - 1);
  subAddress[sizeof(subAddress) - 1] = 0;

  /* Get a data channel */
  if ((ch = PptpCtrlGetChan(c, PPTP_CHAN_ST_WAIT_ANSWER, FALSE, FALSE,
      req->bearType, req->frameType, req->minBps, req->maxBps,
      PPTP_STR_INTERNAL_CALLING, calledNum, subAddress)) == NULL) {
    CLOG(LOG_ERR, "can't get channel for %s call", "outgoing");
    goto chFail;
  }

  /* Fill in details */
  ch->serno = req->serno;
  ch->peerCid = req->cid;
  ch->peerPpd = req->ppd;
  ch->recvWin = req->recvWin;

  /* Ask link layer about making the outgoing call */
  PptpCtrlInitCinfo(ch, &cinfo);
  linfo = (*pptp->get_out_link)(pptp->arg,
    cinfo, c->peer_addr, c->peer_port, req->bearType,
    req->frameType, req->minBps, req->maxBps, calledNum, subAddress);
  if (linfo.cookie == NULL || linfo.cancel == NULL || linfo.result == NULL)
    goto denied;

  /* Link layer says it's OK; wait for link layer to report back later */
  ch->linfo = linfo;
  return;

  /* Failed */
denied:
  CLOG(LOG_NOTICE, "peer's %s call request was denied", "outgoing");
  if (ch)
    PptpCtrlNewChanState(ch, PPTP_CHAN_ST_FREE);
chFail:
  memset(&reply, 0, sizeof(reply));
  reply.peerCid = req->cid;
  reply.result = PPTP_OCR_RESL_ADMIN;
  PptpCtrlWriteMsg(c, PPTP_OutCallReply, &reply);
}

/*
 * PptpOutCallReply()
 */

static void
PptpOutCallReply(PptpChan ch, struct pptpOutCallReply *reply)
{
  /* Did call go through? */
  if (reply->result != PPTP_OCR_RESL_OK) {
    char	errmsg[100];

    snprintf(errmsg, sizeof(errmsg),
      "outgoing call failed: res=%s err=%s",
      PPTP_OCR_RESL_CODE(reply->result), PPTP_ERROR_CODE(reply->err));
    CHLOG(LOG_NOTICE, "%s", errmsg);
    (*ch->linfo.result)(ch->linfo.cookie, errmsg);
    PptpCtrlKillChan(ch, "remote outgoing call failed");
    return;
  }

  /* Call succeeded */
  ch->peerPpd = reply->ppd;
  ch->recvWin = reply->recvWin;
  ch->peerCid = reply->cid;
  CHLOG(LOG_INFO, "outgoing call connected at %d bps", reply->speed);
  PptpCtrlNewChanState(ch, PPTP_CHAN_ST_ESTABLISHED);
  (*ch->linfo.result)(ch->linfo.cookie, NULL);
}

/*
 * PptpInCallRequest()
 */

static void
PptpInCallRequest(PptpCtrl c, struct pptpInCallRequest *req)
{
  struct pptp_engine		*const pptp = c->pptp;
  struct pptpInCallReply	reply;
  struct pptpctrlinfo		cinfo;
  struct pptplinkinfo		linfo;
  PptpChan			ch;
  char				callingNum[PPTP_PHONE_LEN + 1];
  char				calledNum[PPTP_PHONE_LEN + 1];
  char				subAddress[PPTP_SUBADDR_LEN + 1];

  /* Copy out fields */
  if (req->dialedLen > PPTP_PHONE_LEN)
    req->dialedLen = PPTP_PHONE_LEN;
  if (req->dialingLen > PPTP_PHONE_LEN)
    req->dialingLen = PPTP_PHONE_LEN;
  strncpy(callingNum, req->dialing, sizeof(callingNum) - 1);
  callingNum[req->dialingLen] = 0;
  strncpy(calledNum, req->dialed, sizeof(calledNum) - 1);
  calledNum[req->dialedLen] = 0;
  strncpy(subAddress, req->subaddr, sizeof(subAddress) - 1);
  subAddress[sizeof(subAddress) - 1] = 0;

  CLOG(LOG_INFO, "peer incoming call to \"%s\" from \"%s\"",
    calledNum, callingNum);

  /* Initialize reply */
  memset(&reply, 0, sizeof(reply));
  reply.peerCid = req->cid;
  reply.recvWin = PPTP_RECV_WIN;	/* XXX */
  reply.ppd = PPTP_PPD;			/* XXX */

  /* Get a data channel */
  if ((ch = PptpCtrlGetChan(c, PPTP_CHAN_ST_WAIT_CONNECT, FALSE, TRUE,
      req->bearType, 0, 0, INT_MAX,
      callingNum, calledNum, subAddress)) == NULL) {
    CLOG(LOG_ERR, "can't get channel for %s call", "incoming");
    reply.result = PPTP_ICR_RESL_ERR;
    reply.err = PPTP_ERROR_NO_RESOURCE;
    goto done;
  }
  reply.cid = ch->cid;

  /* Fill in details */
  ch->serno = req->serno;
  ch->peerCid = req->cid;
  ch->bearType = req->bearType;

  /* Ask link layer about accepting the incoming call */
  PptpCtrlInitCinfo(ch, &cinfo);
  linfo.cookie = NULL;
  if (pptp->get_in_link != NULL)
    linfo = (*pptp->get_in_link)(pptp->arg, cinfo, c->peer_addr, c->peer_port,
      req->bearType, callingNum, calledNum, subAddress);
  if (linfo.cookie == NULL) {
    CLOG(LOG_NOTICE, "peer's %s call request was denied", "incoming");
    reply.result = PPTP_ICR_RESL_NAK;
    goto done;
  }

  /* Link layer says it's OK */
  CHLOG(LOG_INFO, "accepting incoming call to \"%s\" from \"%s\"",
    calledNum, callingNum);
  reply.result = PPTP_ICR_RESL_OK;
  ch->linfo = linfo;
  strncpy(ch->callingNum, req->dialing, sizeof(ch->callingNum));
  strncpy(ch->calledNum, req->dialed, sizeof(ch->calledNum));
  strncpy(ch->subAddress, req->subaddr, sizeof(ch->subAddress));

  /* Return result */
done:
  PptpCtrlWriteMsg(c, PPTP_InCallReply, &reply);
  if (reply.result != PPTP_ICR_RESL_OK)
    PptpCtrlKillChan(ch, "peer incoming call failed");
}

/*
 * PptpInCallReply()
 */

static void
PptpInCallReply(PptpChan ch, struct pptpInCallReply *reply)
{
  PptpCtrl		const c = ch->ctrl;
  struct pptpInCallConn	con;

  /* Did call go through? */
  if (reply->result != PPTP_ICR_RESL_OK) {
    char	errmsg[100];

    snprintf(errmsg, sizeof(errmsg),
      "peer denied incoming call: res=%s err=%s",
      PPTP_ICR_RESL_CODE(reply->result), PPTP_ERROR_CODE(reply->err));
    CHLOG(LOG_NOTICE, "%s", errmsg);
    (*ch->linfo.result)(ch->linfo.cookie, errmsg);
    PptpCtrlKillChan(ch, "peer denied incoming call");
    return;
  }

  /* Call succeeded */
  CHLOG(LOG_INFO, "incoming call accepted by peer");
  ch->peerCid = reply->cid;
  ch->peerPpd = reply->ppd;
  ch->recvWin = reply->recvWin;
  PptpCtrlNewChanState(ch, PPTP_CHAN_ST_ESTABLISHED);
  (*ch->linfo.result)(ch->linfo.cookie, NULL);

  /* Send back connected message */
  memset(&con, 0, sizeof(con));
  con.peerCid = reply->cid;
  con.speed = 64000;			/* XXX */
  con.recvWin = PPTP_RECV_WIN;		/* XXX */
  con.ppd = PPTP_PPD;			/* XXX */
  con.frameType = PPTP_FRAMECAP_SYNC;
  PptpCtrlWriteMsg(c, PPTP_InCallConn, &con);
}

/*
 * PptpInCallConn()
 */

static void
PptpInCallConn(PptpChan ch, struct pptpInCallConn *con)
{
  CHLOG(LOG_INFO, "peer incoming call connected at %d bps", con->speed);
  ch->peerPpd = con->ppd;
  ch->recvWin = con->recvWin;
  ch->frameType = con->frameType;
  (*ch->linfo.result)(ch->linfo.cookie, NULL);
  PptpCtrlNewChanState(ch, PPTP_CHAN_ST_ESTABLISHED);
}

/*
 * PptpCallClearRequest()
 */

static void
PptpCallClearRequest(PptpChan ch, struct pptpCallClearRequest *req)
{
  struct pptpCallDiscNotify	notify;
  PptpCtrl			const c = ch->ctrl;

  if (PPTP_CHAN_IS_PNS(ch)) {
    CHLOG(LOG_NOTICE, "rec'd %s, but we are PNS for this call",
      gPptpMsgInfo[PPTP_CallClearRequest].name);
    PptpCtrlKillCtrl(c);
    return;
  }
  CHLOG(LOG_INFO, "call cleared by peer");
  memset(&notify, 0, sizeof(notify));
  notify.cid = ch->cid;			/* we are the PAC, use our CID */
  notify.result = PPTP_CDN_RESL_REQ;
  /* XXX stats? */
  PptpCtrlWriteMsg(c, PPTP_CallDiscNotify, &notify);
  PptpCtrlKillChan(ch, "cleared by peer");
}

/*
 * PptpCallDiscNotify()
 */

static void
PptpCallDiscNotify(PptpChan ch, struct pptpCallDiscNotify *notify)
{
  CHLOG(LOG_INFO, "peer call disconnected res=%s err=%s",
    PPTP_CDN_RESL_CODE(notify->result), PPTP_ERROR_CODE(notify->err));
  PptpCtrlKillChan(ch, "disconnected by peer");
}

/*
 * PptpWanErrorNotify()
 */

static void
PptpWanErrorNotify(PptpChan ch, struct pptpWanErrorNotify *notif)
{
  CHLOG(LOG_DEBUG, "ignoring %s", gPptpMsgInfo[PPTP_WanErrorNotify].name);
}

/*
 * PptpSetLinkInfo()
 */

static void
PptpSetLinkInfo(PptpChan ch, struct pptpSetLinkInfo *info)
{
  if (ch->linfo.setLinkInfo)
    (*ch->linfo.setLinkInfo)(ch->linfo.cookie, info->sendAccm, info->recvAccm);
  else {
    CHLOG(LOG_DEBUG, "ignoring %s", gPptpMsgInfo[PPTP_SetLinkInfo].name);
  }
}

