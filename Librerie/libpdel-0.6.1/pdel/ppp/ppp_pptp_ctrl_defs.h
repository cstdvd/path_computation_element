
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

#ifndef _WANT_PPTP_FIELDS

/*
 * DEFINITIONS
 */

/* Definitions per the spec */
#define PPTP_PORT		1723
#define PPTP_MTU		1532
#define PPTP_PROTO_VERS		0x0100
#define PPTP_MAGIC		0x1a2b3c4d
#define PPTP_IDLE_TIMEOUT	60

#define PPTP_HOSTNAME_LEN	64
#define PPTP_VENDOR_LEN		64
#define PPTP_PHONE_LEN		64
#define PPTP_SUBADDR_LEN	64
#define PPTP_STATS_LEN		128

/* Control message header type */
#define PPTP_CTRL_MSG_TYPE	1

/* Control messages */
enum {
	PPTP_StartCtrlConnRequest = 1,
	PPTP_StartCtrlConnReply = 2,
	PPTP_StopCtrlConnRequest = 3,
	PPTP_StopCtrlConnReply = 4,
	PPTP_EchoRequest = 5,
	PPTP_EchoReply = 6,
	PPTP_OutCallRequest = 7,
	PPTP_OutCallReply = 8,
	PPTP_InCallRequest = 9,
	PPTP_InCallReply = 10,
	PPTP_InCallConn = 11,
	PPTP_CallClearRequest = 12,
	PPTP_CallDiscNotify = 13,
	PPTP_WanErrorNotify = 14,
	PPTP_SetLinkInfo = 15,
};

#define PPTP_MIN_CTRL_TYPE		1
#define PPTP_MAX_CTRL_TYPE		16

#define PPTP_VALID_CTRL_TYPE(x)	\
	((x) >= PPTP_MIN_CTRL_TYPE && (x) < PPTP_MAX_CTRL_TYPE)

/* Framing capabilities */
#define PPTP_FRAMECAP_ASYNC		0x01
#define PPTP_FRAMECAP_SYNC		0x02
#define PPTP_FRAMECAP_ANY		0x03

/* Bearer capabilities */
#define PPTP_BEARCAP_ANALOG		0x01
#define PPTP_BEARCAP_DIGITAL		0x02
#define PPTP_BEARCAP_ANY		0x03

/* General error codes */
#define PPTP_ERROR_NONE			0
#define PPTP_ERROR_NOT_CONNECTED	1
#define PPTP_ERROR_BAD_FORMAT		2
#define PPTP_ERROR_BAD_VALUE		3
#define PPTP_ERROR_NO_RESOURCE		4
#define PPTP_ERROR_BAD_CALL_ID		5
#define PPTP_ERROR_PAC_ERROR		6

/* All reserved fields have this prefix */
#define PPTP_RESV_PREF			"resv"

/* Message structures */
struct pptpMsgHead {
	u_int16_t	length;			/* total length */
	u_int16_t	msgType;		/* pptp message type */
	u_int32_t	magic;			/* magic cookie */
	u_int16_t	type;			/* control message type */
	u_int16_t	resv0;			/* reserved */
};
typedef struct	pptpMsgHead *PptpMsgHead;

#else
  { { "len", 2 }, { "msgType", 2 }, { "magic", 4 }, { "type", 2 },
    { PPTP_RESV_PREF "0", 2 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpStartCtrlConnRequest {
	u_int16_t	vers;			/* protocol version */
	u_int16_t	resv0;			/* reserved */
	u_int32_t	frameCap;		/* framing capabilities */
	u_int32_t	bearCap;		/* bearer capabilities */
	u_int16_t	maxChan;		/* maximum # channels */
	u_int16_t	firmware;		/* firmware revision */
	char		host[PPTP_HOSTNAME_LEN];	/* host name */
	char		vendor[PPTP_VENDOR_LEN];	/* vendor name */
};

#else
  { { "vers", 2 }, { PPTP_RESV_PREF "0", 2 }, { "frameCap", 4 },
    { "bearCap", 4 }, { "maxChan", 2 }, { "firm", 2 },
    { "host", PPTP_HOSTNAME_LEN }, { "vend", PPTP_VENDOR_LEN }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpStartCtrlConnReply {
	u_int16_t	vers;			/* protocol version */
	u_int8_t	result;			/* result code */
	u_int8_t	err;			/* error code */
	u_int32_t	frameCap;		/* framing capabilities */
	u_int32_t	bearCap;		/* bearer capabilities */
	u_int16_t	maxChan;		/* maximum # channels */
	u_int16_t	firmware;		/* firmware revision */
	char		host[PPTP_HOSTNAME_LEN];	/* host name */
	char		vendor[PPTP_VENDOR_LEN];	/* vendor name */
};

#else
  { { "vers", 2 }, { "result", 1 }, { "err", 1 }, { "frameCap", 4 },
    { "bearCap", 4 }, { "maxChan", 2 }, { "firm", 2 },
    { "host", PPTP_HOSTNAME_LEN }, { "vend", PPTP_VENDOR_LEN }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_SCCR_RESL_OK		1	/* channel established */
#define PPTP_SCCR_RESL_ERR		2	/* general error; see code */
#define PPTP_SCCR_RESL_EXISTS		3	/* command channel exists */
#define PPTP_SCCR_RESL_AUTH		4	/* not authorized */
#define PPTP_SCCR_RESL_VERS		5	/* incompatible version */

struct pptpStopCtrlConnRequest {
	u_int8_t	reason;			/* reason */
	u_int8_t	resv0;			/* reserved */
	u_int16_t	resv1;			/* reserved */
};

#else
  { { "reason", 1 }, { PPTP_RESV_PREF "0", 1 }, { PPTP_RESV_PREF "1", 2 },
    { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_SCCR_REAS_NONE		1	/* general */
#define PPTP_SCCR_REAS_PROTO		2	/* incompatible version */
#define PPTP_SCCR_REAS_LOCAL		3	/* local shutdown */

struct pptpStopCtrlConnReply {
	u_int8_t	result;			/* result code */
	u_int8_t	err;			/* error code */
	u_int16_t	resv0;			/* reserved */
};

#else
  { { "result", 1 }, { "err", 1 }, { PPTP_RESV_PREF "0", 2 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpEchoRequest {
	u_int32_t	id;			/* identifier */
};

#else
  { { "id", 4 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpEchoReply {
	u_int32_t	id;			/* identifier */
	u_int8_t	result;			/* result code */
	u_int8_t	err;			/* error code */
	u_int16_t	resv0;			/* reserved */
};

#else
  { { "id", 4 }, { "result", 1 }, { "err", 1 },
    { "ignore", 2 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_ECHO_RESL_OK		1	/* echo reply is valid */
#define PPTP_ECHO_RESL_ERR		2	/* general error; see code */

struct pptpOutCallRequest {
	u_int16_t	cid;			/* call id */
	u_int16_t	serno;			/* call serial # */
	u_int32_t	minBps;			/* minimum BPS */
	u_int32_t	maxBps;			/* maximum BPS */
	u_int32_t	frameType;		/* framing type */
	u_int32_t	bearType;		/* bearer type */
	u_int16_t	recvWin;		/* pkt receive window size */
	u_int16_t	ppd;			/* pkt processing delay */
	u_int16_t	numLen;			/* phone number length */
	u_int16_t	resv0;			/* reserved */
	char		phone[PPTP_PHONE_LEN];		/* phone number */
	char		subaddr[PPTP_SUBADDR_LEN];	/* sub-address */
};

#else
  { { "cid", 2 }, { "serno", 2 }, { "minBPS", 4 }, { "maxBPS", 4 },
    { "frameType", 4 }, { "bearType", 4 }, { "recvWin", 2 }, { "ppd", 2 },
    { "numLen", 2 }, { PPTP_RESV_PREF "0", 2 },
    { "phone", PPTP_PHONE_LEN }, { "subaddr", PPTP_SUBADDR_LEN },
    { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpOutCallReply {
	u_int16_t	cid;			/* call id */
	u_int16_t	peerCid;		/* peer call id */
	u_int8_t	result;			/* result code */
	u_int8_t	err;			/* error code */
	u_int16_t	cause;			/* cause code */
	u_int32_t	speed;			/* cause code */
	u_int16_t	recvWin;		/* pkt receive window size */
	u_int16_t	ppd;			/* pkt processing delay */
	u_int32_t	channel;		/* physical channel id */
};

#else
  { { "cid", 2 }, { "peerCid", 2 }, { "result", 1 }, { "err", 1 },
    { "cause", 2 }, { "speed", 4 }, { "recvWin", 2 }, { "ppd", 2 },
    { "channel", 4 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_OCR_RESL_OK		1	/* call established OK */
#define PPTP_OCR_RESL_ERR		2	/* general error; see code */
#define PPTP_OCR_RESL_NOCARR		3	/* no carrier */
#define PPTP_OCR_RESL_BUSY		4	/* busy */
#define PPTP_OCR_RESL_NODIAL		5	/* no dialtone */
#define PPTP_OCR_RESL_TIMED		6	/* timed out */
#define PPTP_OCR_RESL_ADMIN		7	/* administratvely prohibited */

struct pptpInCallRequest {
	u_int16_t	cid;			/* call id */
	u_int16_t	serno;			/* call serial # */
	u_int32_t	bearType;		/* bearer type */
	u_int32_t	channel;		/* physical channel id */
	u_int16_t	dialedLen;		/* dialed number len */
	u_int16_t	dialingLen;		/* dialing number len */
	char		dialed[PPTP_PHONE_LEN];		/* dialed number */
	char		dialing[PPTP_PHONE_LEN];	/* dialing number */
	char		subaddr[PPTP_SUBADDR_LEN];	/* sub-address */
};

#else
  { { "cid", 2 }, { "serno", 2 }, { "bearType", 4 }, { "channel", 4 },
    { "dialedLen", 2 }, { "dialingLen", 2 }, { "dialed", PPTP_PHONE_LEN },
    { "dialing", PPTP_PHONE_LEN }, { "subaddr", PPTP_SUBADDR_LEN },
    { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpInCallReply {
	u_int16_t	cid;			/* call id */
	u_int16_t	peerCid;		/* peer call id */
	u_int8_t	result;			/* result code */
	u_int8_t	err;			/* error code */
	u_int16_t	recvWin;		/* pkt receive window size */
	u_int16_t	ppd;			/* pkt processing delay */
	u_int16_t	resv0;			/* reserved */
};

#else
  { { "cid", 2 }, { "peerCid", 2 }, { "result", 1 }, { "err", 1 },
    { "recvWin", 2 }, { "ppd", 2 }, { PPTP_RESV_PREF "0", 2 },
    { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_ICR_RESL_OK		1	/* call established OK */
#define PPTP_ICR_RESL_ERR		2	/* general error; see code */
#define PPTP_ICR_RESL_NAK		3	/* do not accept */

struct pptpInCallConn {
	u_int16_t	peerCid;		/* peer call id */
	u_int16_t	resv0;			/* reserved */
	u_int32_t	speed;			/* connect speed */
	u_int16_t	recvWin;		/* pkt receive window size */
	u_int16_t	ppd;			/* pkt processing delay */
	u_int32_t	frameType;		/* framing type */
};

#else
  { { "peerCid", 2 }, { PPTP_RESV_PREF "0", 2 }, { "speed", 4 },
    { "recvWin", 2 }, { "ppd", 2 }, { "frameType", 4 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpCallClearRequest {
	u_int16_t	cid;			/* pns assigned call id */
	u_int16_t	resv0;			/* reserved */
};

#else
  { { "cid", 2 }, { PPTP_RESV_PREF "0", 2 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpCallDiscNotify {
	u_int16_t	cid;			/* pac assigned call id */
	u_int8_t	result;			/* result code */
	u_int8_t	err;			/* error code */
	u_int16_t	cause;			/* cause code */
	u_int16_t	resv0;			/* reserved */
	char		stats[PPTP_STATS_LEN];	/* call stats */
};

#else
  { { "cid", 2 }, { "result", 1 }, { "err", 1 }, { "cause", 2 },
    { PPTP_RESV_PREF "0", 2 }, { "stats", PPTP_STATS_LEN },
    { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_CDN_RESL_CARR		1	/* lost carrier */
#define PPTP_CDN_RESL_ERR		2	/* general error; see code */
#define PPTP_CDN_RESL_ADMIN		3	/* administrative reason */
#define PPTP_CDN_RESL_REQ		4	/* received disconnect req. */

struct pptpWanErrorNotify {
	u_int16_t	cid;			/* pns assigned call id */
	u_int16_t	resv0;			/* reserved */
	u_int32_t	crc;			/* crc errors */
	u_int32_t	frame;			/* framing errors */
	u_int32_t	hdw;			/* hardware errors */
	u_int32_t	ovfl;			/* buffer overrun errors */
	u_int32_t	timeout;		/* timeout errors */
	u_int32_t	align;			/* alignment errors */
};

#else
  { { "cid", 2 }, { PPTP_RESV_PREF "0", 2 }, { "crc", 4 },
    { "frame", 4 }, { "hdw", 4 }, { "ovfl", 4 }, { "timeout", 4 },
    { "align", 4 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

struct pptpSetLinkInfo {
	u_int16_t	cid;			/* call id */
	u_int16_t	resv0;			/* reserved */
	u_int32_t	sendAccm;		/* send accm */
	u_int32_t	recvAccm;		/* receive accm */
};

#else
  { { "cid", 2 }, { PPTP_RESV_PREF "0", 2 }, { "sendAccm", 4 },
    { "recvAccm", 4 }, { NULL, 0 } },
#endif
#ifndef _WANT_PPTP_FIELDS

#define PPTP_CTRL_MAX_FRAME \
	(sizeof(struct pptpMsgHead) + sizeof(struct pptpInCallRequest))
#define PPTP_CTRL_MAX_FIELDS		14

/* Describes one field of a PPTP control message structure */
struct pptpfield {
	const char	*name;
	u_short		length;
};
typedef const	struct pptpfield *PptpField;

#endif

