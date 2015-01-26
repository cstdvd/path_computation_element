
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

#ifndef _PDEL_PPP_PPP_PPTP_CTRL_H_
#define _PDEL_PPP_PPP_PPTP_CTRL_H_

struct ppp_log;
struct pptp_engine;

/*
 * Callback function types
 */

/*
 * Link callback info (passed to PPTP control code)
 *
 * cookie	Link private cookie for this connection, NULL = failed
 * result	Called to report success/failure of a locally initiated
 *		incoming or outgoing call to peer; also used to report
 *		when/if a call is terminated by error or the remote side.
 * setLinkInfo	Called when peer notifies us of the remote call link info
 * cancel	Called to cancel a local outgoing call initiated by peer
 */
struct pptplinkinfo {		/* PPTP's info for accessing link code */
	void	*cookie;	/* NULL indicates response is invalid */
	void	(*result)(void *cookie, const char *errmsg);
	void	(*setLinkInfo)(void *cookie, u_int32_t sa, u_int32_t ra);
	void	(*cancel)(void *cookie);	/* cancel outgoing call */
};
typedef struct pptplinkinfo	*PptpLinkInfo;

/*
 * PPTP control callback info (passed to link code)
 *
 * cookie	PPTP control private cookie for this connection, NULL = failed
 * peer_addr	Peer's IP address
 * peer_port	Peer's TCP port
 * close	Close/shutdown the call
 * answer	Notify of outgoing call (initiated by peer) success/failure
 *		Must be called sometime after return from the link-supplied
 *		'PptpGetOutLink_t' function.
 */
struct pptpctrlinfo {		/* Link's info for accessing PPTP code */
	void		*cookie;	/* NULL indicates response is invalid */
	struct in_addr	peer_addr;	/* Peer IP address and port */
	u_int16_t	peer_port;
	void		(*close)(void *cookie, int result, int err, int cause);
	void		(*answer)(void *cookie, int rs, int er, int cs, int sp);
};
typedef struct pptpctrlinfo	*PptpCtrlInfo;

typedef int	PptpCheckNewConn_t(void *arg, struct in_addr ip,
			u_int16_t port, char *logname, size_t lnmax);

typedef struct	pptplinkinfo PptpGetInLink_t(void *arg,
			struct pptpctrlinfo cinfo, struct in_addr peer,
			u_int16_t port, int bearType, const char *callingNum,
			const char *calledNum, const char *subAddress);

typedef struct	pptplinkinfo PptpGetOutLink_t(void *arg,
			struct pptpctrlinfo cinfo, struct in_addr peer,
			u_int16_t port, int bearType, int frameType,
			int minBps, int maxBps, const char *calledNum,
			const char *subAddress);

/*
 * Public functions
 */

__BEGIN_DECLS

extern struct	pptp_engine *PptpCtrlInit(void *arg, struct pevent_ctx *ctx,
			pthread_mutex_t *mutex,
			PptpCheckNewConn_t *checkNewConn,
			PptpGetInLink_t *getInLink,
			PptpGetOutLink_t *getOutLink,
			struct in_addr ip, u_int16_t port, const char *vendor,
			struct ppp_log *log, int nocd);

extern void	PptpCtrlShutdown(struct pptp_engine **enginep);

extern int	PptpCtrlListen(struct pptp_engine *engine, int enable);

extern struct	pptpctrlinfo PptpCtrlInCall(struct pptp_engine *engine,
			struct pptplinkinfo linfo, struct in_addr ip,
			u_int16_t port, const char *logname, int bearType,
			int frameType, int minBps, int maxBps,
			const char *callingNum, const char *calledNum,
			const char *subAddress);

extern struct	pptpctrlinfo PptpCtrlOutCall(struct pptp_engine *engine,
			struct pptplinkinfo linfo, struct in_addr ip,
			u_int16_t port, const char *logname, int bearType,
			int frameType, int minBps, int maxBps,
			const char *calledNum, const char *subAddress);

extern int	PptpCtrlGetSessionInfo(const struct pptpctrlinfo *cp,
			struct in_addr *selfAddr, struct in_addr *peerAddr,
			u_int16_t *selfCid, u_int16_t *peerCid,
			u_int16_t *peerWin, u_int16_t *peerPpd);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_PPTP_CTRL_H_ */
