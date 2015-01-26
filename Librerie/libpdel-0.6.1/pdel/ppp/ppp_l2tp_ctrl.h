
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PPP_L2TP_PDEL_PPP_PPP_L2TP_CTRL_H_
#define _PPP_L2TP_PDEL_PPP_PPP_L2TP_CTRL_H_

/*
 * API between L2TP 'control' and 'link' code. The control code handles
 * the L2TP control connection. The link code handles plumbing netgraph,
 * and interfacing with the PPP engine.
 *
 * Each control connection is represented by a 'struct ppp_l2tp_ctrl',
 * and each session by a 'struct ppp_l2tp_sess'. The link side may store
 * its own opaque pointer in these structures as well.
 *
 * The functions and callbacks are designed so that whenever either side
 * declares a control connection or session 'terminated', then the other
 * side (whose function is being called) must not refer to that object
 * any more.
 *
 * It is presumed that the 'link' side has a mutex which is held while
 * calling any of the 'control' side functions, and which is acquired
 * first when any 'link' side functions are invoked. In other words,
 * the control side is not reentrant (though see 'ppp_l2tp_initiated_t'
 * for an exception).
 */

struct ppp_l2tp_ctrl;			/* control connection structure */
struct ppp_l2tp_sess;			/* call session structure */

/************************************************************************
		CONTROL -> LINK CALLBACK FUNCTIONS
************************************************************************/

/*
 * This is called when a control connection is terminated for any reason
 * other than a call ppp_l2tp_ctrl_destroy().
 *
 * Arguments:
 *	ctrl	Control connection
 *	result	Result code (never zero)
 *	error	Error code (if result == L2TP_RESULT_GENERAL, else zero)
 *	errmsg	Error message string
 */
typedef void	ppp_l2tp_ctrl_terminated_t(struct ppp_l2tp_ctrl *ctrl,
			u_int16_t result, u_int16_t error, const char *errmsg);

/*
 * This callback is used to report the peer's initiating a new incoming
 * or outgoing call.
 *
 * In the case of an incoming call, when/if the call eventually connects,
 * the ppp_l2tp_connected_t callback will be called by the control side.
 *
 * In the case of an outgoing call, the link side is responsible for calling
 * ppp_l2tp_connected() when/if the call is connected.
 *
 * This callback may choose to call ppp_l2tp_terminate() (to deny the call)
 * or ppp_l2tp_connected() (to accept it immediately if outgoing) before
 * returning.
 *
 * The link side is expected to plumb the netgraph session hook at this time.
 *
 * Arguments:
 *	ctrl	Associated control connection
 *	sess	Session structure
 *	out	Non-zero to indicate outgoing call
 *	avps	AVP's contained in the associated Outgoing-Call-Request
 *		or Incoming-Call-Request control message.
 */
typedef void	ppp_l2tp_initiated_t(struct ppp_l2tp_ctrl *ctrl,
			struct ppp_l2tp_sess *sess, int out,
			const struct ppp_l2tp_avp_list *avps);

/*
 * This callback is used to report successful connection of a remotely
 * initiated incoming call (see ppp_l2tp_initiated_t) or a locally initiated
 * outgoing call (see ppp_l2tp_initiate()).  That is, we are acting as
 * the LNS for this session.
 *
 * Arguments:
 *	sess	Session structure
 *	avps	AVP's contained in the associated Outgoing-Call-Connected
 *		or Incoming-Call-Connected control message.
 */
typedef void	ppp_l2tp_connected_t(struct ppp_l2tp_sess *sess,
			const struct ppp_l2tp_avp_list *avps);

/*
 * This callback is called when any call, whether successfully connected
 * or not, is terminated for any reason other than explict termination
 * from the link side (via a call to either ppp_l2tp_terminate() or
 * ppp_l2tp_ctrl_destroy()).
 *
 * Arguments:
 *	sess	Session structure
 *	result	Result code (never zero)
 *	error	Error code (if result == L2TP_RESULT_GENERAL, else zero)
 *	errmsg	Error message string
 */
typedef void	ppp_l2tp_terminated_t(struct ppp_l2tp_sess *sess,
			u_int16_t result, u_int16_t error, const char *errmsg);

/*
 * This callback is used when the remote side sends a Set-Link-Info
 * message. It is optional and may be NULL if not required.
 *
 * Arguments:
 *	sess	Session structure
 *	xmit	LAC's send ACCM
 *	recv	LAC's receive ACCM
 */
typedef void	ppp_l2tp_set_link_info_t(struct ppp_l2tp_sess *sess,
			u_int32_t xmit, u_int32_t recv);

/*
 * This callback is used when the remote side sends a WAN-Error-Notify
 * message. It is optional and may be NULL if not required.
 *
 * Arguments:
 *	sess	Session structure
 *	crc	CRC errors
 *	frame	Framing errors
 *	overrun	Hardware overrun errors
 *	buffer	Buffer overrun errors
 *	timeout	Timeout errors
 *	align	Alignment errors
 */
typedef void	ppp_l2tp_wan_error_notify_t(struct ppp_l2tp_sess *sess,
			u_int32_t crc, u_int32_t frame, u_int32_t overrun,
			u_int32_t buffer, u_int32_t timeout, u_int32_t align);

/* Callback structure provided by the link side */
struct ppp_l2tp_ctrl_cb {
	ppp_l2tp_ctrl_terminated_t	*ctrl_terminated;
	ppp_l2tp_initiated_t		*initiated;
	ppp_l2tp_connected_t		*connected;
	ppp_l2tp_terminated_t		*terminated;
	ppp_l2tp_set_link_info_t	*set_link_info;
	ppp_l2tp_wan_error_notify_t	*wan_error_notify;
};

/************************************************************************
			LINK -> CONTROL FUNCTIONS
************************************************************************/

__BEGIN_DECLS

/*
 * This function creates a new control connection. If successful, the
 * control code will create a ng_l2tp(4) node and fill in *nodep and
 * the 'hook' buffer. The link code must then connect this hook to the
 * appropriate L2TP packet delivery node before the next context switch.
 *
 * The control code guarantees exactly one call in the future to the
 * ppp_l2tp_ctrl_terminated_t callback unless ppp_l2tp_ctrl_destroy()
 * is called first.
 *
 * Arguments:
 *	ctx	Event context for use by control code
 *	mutex	Mutex for operation
 *	cb	Pointer to link callback functions
 *	log	Where to log stuff (if successful, the log is consumed)
 *	peer_id	Unique identifier for peer (used for tie-breakers)
 *	initiate Whether to send a SCCRQ or just wait for one
 *	nodep	Pointer to netgraph node ID variable
 *	hook	Buffer for hook on L2TP netgraph node (size >= NG_HOOKLEN + 1)
 *	avps	List of AVP's to include in the associated
 *		Start-Control-Connection-Request or
 *		Start-Control-Connection-Reply control message.
 *	secret	Shared secret (or NULL if none required)
 *	seclen	Length of shared secret (if secret != NULL)
 *
 * Returns NULL if failure (errno is set), otherwise a pointer to an
 * opaque control connection object.
 */
extern struct	ppp_l2tp_ctrl *ppp_l2tp_ctrl_create(struct pevent_ctx *ctx,
			pthread_mutex_t *mutex,
			const struct ppp_l2tp_ctrl_cb *cb, struct ppp_log *log,
			int initiate, u_int32_t peer_id, ng_ID_t *nodep,
			char *hook, const struct ppp_l2tp_avp_list *avps,
			const void *secret, size_t seclen);

/*
 * Close a control connection gracefully.
 *
 * All active sessions will be terminated, and eventually
 * ppp_l2tp_ctrl_terminated_t will be called, assuming no
 * intervening call to ppp_l2tp_ctrl_destroy() has been made.
 *
 * Arguments:
 *	ctrl	Control connection structure
 *	result	Result code (never zero)
 *	error	Error code (if result == L2TP_RESULT_GENERAL, else zero)
 *	errmsg	Error message string
 */
extern void	ppp_l2tp_ctrl_shutdown(struct ppp_l2tp_ctrl *ctrl,
			u_int16_t result, u_int16_t error, const char *errmsg);

/*
 * Immediately destroy a control connection and all associated sessions.
 * This does *not* result in any callbacks to the link side for either
 * active sessions (ppp_l2tp_terminated_t) or the control connection
 * itself (ppp_l2tp_ctrl_terminated_t).
 *
 * Upon return, *ctrlp will be set to NULL.
 *
 * Arguments:
 *	ctrlp	Pointer to the control connection descriptor pointer
 */
extern void	ppp_l2tp_ctrl_destroy(struct ppp_l2tp_ctrl **ctrlp);

/*
 * This function initiates a new session, either an as an incoming or
 * outgoing call request to the peer.
 *
 * In the case of an incoming call, when/if the call eventually connects,
 * ppp_l2tp_connected() must be called.
 *
 * In the case of an outgoing call, when/if the call eventually connects,
 * the link's ppp_l2tp_connected_t callback will be invoked.
 *
 * The link side is expected to plumb the netgraph session hook at this time.
 *
 * Arguments:
 *	ctrl	Control connection
 *	out	Non-zero to indicate outgoing call
 *	avps	List of AVP's to include in the associated
 *		Outgoing-Call-Request or Incoming-Call-Request
 *		control message.
 *
 * Returns:
 *	NULL	Initiation failed, errno is set
 *	sess	Session structure
 */
extern struct	ppp_l2tp_sess *ppp_l2tp_initiate(struct ppp_l2tp_ctrl *ctrl,
			int out, const struct ppp_l2tp_avp_list *avps);

/*
 * This function is used to report successful connection of a remotely
 * initiated outgoing call (see ppp_l2tp_initiated_t) or a locally initiated
 * incoming call (see ppp_l2tp_initiate()). That is, we are acting as
 * the LAC for this session.
 *
 * Note: if this function returns -1, no special action is required;
 * the control side will close the connection gracefully as if
 * ppp_l2tp_terminate() had been called.
 *
 * Arguments:
 *	sess	Session structure
 *	avps	List of AVP's to include in the associated
 *		Outgoing-Call-Connected or Incoming-Call-Connected
 *		control message.
 *
 * Returns:
 *	 0	Connection successful
 *	-1	Connection failed, errno is set
 */
extern int	ppp_l2tp_connected(struct ppp_l2tp_sess *sess,
			const struct ppp_l2tp_avp_list *avps);

/*
 * This function terminates a session. The session may be in any state.
 * The control code will *not* make any futher calls to link callbacks
 * regarding this session, so the link should clean up any associated
 * state.
 *
 * Arguments:
 *	sess	Session structure
 *	result	Result code (must not be zero)
 *	error	Error code (if result == L2TP_RESULT_GENERAL, else zero)
 *	errmsg	Error message string (optional; may be NULL)
 */
extern void	ppp_l2tp_terminate(struct ppp_l2tp_sess *sess,
			u_int16_t result, u_int16_t error, const char *errmsg);

/*
 * Get or set the link side cookie corresponding to a control connection
 * or a call session.
 */
extern void	*ppp_l2tp_ctrl_get_cookie(struct ppp_l2tp_ctrl *ctrl);
extern void	ppp_l2tp_ctrl_set_cookie(struct ppp_l2tp_ctrl *ctrl,
			void *cookie);
extern void	*ppp_l2tp_sess_get_cookie(struct ppp_l2tp_sess *sess);
extern void	ppp_l2tp_sess_set_cookie(struct ppp_l2tp_sess *sess,
			void *cookie);

/*
 * Get the node ID and hook name for the hook that corresponds
 * to a control connection's L2TP frames.
 */
extern void	ppp_l2tp_ctrl_get_hook(struct ppp_l2tp_ctrl *ctrl,
			ng_ID_t *nodep, const char **hookp);

/*
 * Get the node ID and hook name for the hook that corresponds
 * to a session's data packets.
 */
extern void	ppp_l2tp_sess_get_hook(struct ppp_l2tp_sess *sess,
			ng_ID_t *nodep, const char **hookp);

__END_DECLS

#endif /* _PPP_L2TP_PDEL_PPP_PPP_L2TP_CTRL_H_ */
