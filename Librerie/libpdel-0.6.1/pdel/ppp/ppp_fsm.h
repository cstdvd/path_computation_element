
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_FSM_H_
#define _PDEL_PPP_PPP_FSM_H_

#ifndef _PDEL_PPP_PRIVATE_H_
#error "This header is only for use by the ppp library."
#endif

struct ppp_fsm;
struct ppp_fsm_instance;
struct ppp_fsm_options;
struct ppp_fsm_optdesc;

/*
 * FSM states
 */
enum ppp_fsm_state {
	FSM_STATE_INITIAL	=0,
	FSM_STATE_STARTING	=1,
	FSM_STATE_CLOSED	=2,
	FSM_STATE_STOPPED	=3,
	FSM_STATE_CLOSING	=4,
	FSM_STATE_STOPPING	=5,
	FSM_STATE_REQSENT	=6,
	FSM_STATE_ACKRCVD	=7,
	FSM_STATE_ACKSENT	=8,
	FSM_STATE_OPENED	=9
};
#define FSM_STATE_MAX		10

/*
 * FSM codes
 */
enum ppp_fsm_code {
	FSM_CODE_VENDOR		=0,
	FSM_CODE_CONFIGREQ	=1,
	FSM_CODE_CONFIGACK	=2,
	FSM_CODE_CONFIGNAK	=3,
	FSM_CODE_CONFIGREJ	=4,
	FSM_CODE_TERMREQ	=5,
	FSM_CODE_TERMACK	=6,
	FSM_CODE_CODEREJ	=7,
	FSM_CODE_PROTOREJ	=8,
	FSM_CODE_ECHOREQ	=9,
	FSM_CODE_ECHOREP	=10,
	FSM_CODE_DISCREQ	=11,
	FSM_CODE_IDENT		=12,
	FSM_CODE_TIMEREM	=13,
	FSM_CODE_RESETREQ	=14,
	FSM_CODE_RESETACK	=15
};
#define FSM_CODE_MAX		16

/*
 * FSM packet format
 */
struct ppp_fsm_pkt {
	u_char			code;		/* fsm code */
	u_char			id;		/* packet id */
	u_int16_t		length;		/* length (network order) */
	u_char			data[0];	/* packet data */
};

/*
 * Input to an FSM
 */
enum ppp_fsm_input {
	FSM_INPUT_OPEN = 1,		/* request to open */
	FSM_INPUT_CLOSE,		/* request to close */
	FSM_INPUT_UP,			/* lower layer went up */
	FSM_INPUT_DOWN_FATAL,		/* lower layer went down, fatal */
	FSM_INPUT_DOWN_NONFATAL,	/* lower layer went down, not fatal */
	FSM_INPUT_DATA,			/* packet recieved */
	FSM_INPUT_XMIT_PROTOREJ,	/* send a protocol-reject to peer */
	FSM_INPUT_RECD_PROTOREJ,	/* indicate rec'd fatal proto-rej */
};

/*
 * Output from an FSM
 */
enum ppp_fsmoutput {
	FSM_OUTPUT_OPEN	= 1,		/* request opening lower layer */
	FSM_OUTPUT_CLOSE,		/* request closing lower layer */
	FSM_OUTPUT_UP,			/* fsm has reached the open state */
	FSM_OUTPUT_DOWN,		/* fsm has left the open state */
	FSM_OUTPUT_DATA,		/* packet to be sent */
	FSM_OUTPUT_PROTOREJ,		/* rec'd non-fatal protocol reject */
	FSM_OUTPUT_DEAD,		/* fsm has finished or failed */
};

/* Reasons associated with a FSM_OUTPUT_DOWN or FSM_OUTPUT_DEAD output */
enum ppp_fsm_reason {
	FSM_REASON_CLOSE = 1,		/* rec'd FSM_INPUT_CLOSE */
	FSM_REASON_DOWN_FATAL,		/* rec'd FSM_INPUT_DOWN_FATAL */
	FSM_REASON_DOWN_NONFATAL,	/* rec'd FSM_INPUT_DOWN_NONFATAL */
	FSM_REASON_CONF,		/* rec'd Conf-Req, etc. (DOWN only) */
	FSM_REASON_TERM,		/* rec'd terminate request */
	FSM_REASON_CODEREJ,		/* rec'd fatal code reject */
	FSM_REASON_PROTOREJ,		/* rec'd fatal protocol reject */
	FSM_REASON_NEGOT,		/* negotiation failed/didn't converge */
	FSM_REASON_BADMAGIC,		/* bad magic number received */
	FSM_REASON_LOOPBACK,		/* looped back connection detected */
	FSM_REASON_TIMEOUT,		/* peer not responding to echo */
	FSM_REASON_SYSERR,		/* internal system error */
};

/* FSM output structure */
struct ppp_fsm_output {
	enum ppp_fsmoutput		type;	/* type of output */
	union {
		struct {			/* if FSM_OUTPUT_DATA */
			u_char	*data;
			u_int	length;
		}			data;
		u_int16_t		proto;	/* if FSM_OUTPUT_PROTOREJ */
		struct {			/* if FSM_OUTPUT_DOWN, DEAD */
		    enum ppp_fsm_reason	reason;	  /* reason for DOWN or DEAD */
		    union {
			int	    error;	  /* FSM_REASON_SYSERR */
			u_char	    code;	  /* FSM_REASON_CODEREJ/CONF */
			u_int16_t   proto;	  /* FSM_REASON_PROTOREJ */
		    }		u;
		}			down;
	}				u;	/* type specific contents */
};

/*
 * FSM type function types
 */
typedef void	ppp_fsm_type_destroy_t(struct ppp_fsm_instance *fsm);
typedef int	ppp_fsm_type_build_conf_req_t(struct ppp_fsm_instance *fsm,
			struct ppp_fsm_options *opts);
typedef int	ppp_fsm_type_recv_conf_req_t(struct ppp_fsm_instance *fsm,
			struct ppp_fsm_options *req,
			struct ppp_fsm_options *nak,
			struct ppp_fsm_options *rej);
typedef int	ppp_fsm_type_recv_conf_rej_t(struct ppp_fsm_instance *fsm,
			struct ppp_fsm_options *rej);
typedef int	ppp_fsm_type_recv_conf_nak_t(struct ppp_fsm_instance *fsm,
			struct ppp_fsm_options *nak);
typedef u_int32_t ppp_fsm_type_get_magic_t(struct ppp_fsm_instance *fsm,
			int dir);
typedef void	ppp_fsm_type_recv_reset_req_t(struct ppp_fsm_instance *fsm,
			const u_char *data, u_int len);
typedef void	ppp_fsm_type_recv_reset_ack_t(struct ppp_fsm_instance *fsm,
			const u_char *data, u_int len);
typedef void	ppp_fsm_type_recv_vendor_t(struct ppp_fsm_instance *fsm,
			const u_char *data, u_int len);

/*
 * FSM type
 *
 * Information describing one type of FSM (e.g., LCP, IPCP)
 */
struct ppp_fsm_type {

	/* Basic stuff */
	const char		*name;		/* name of protocol */
	u_int16_t		proto;		/* fsm protocol number */
	u_int32_t		sup_codes;	/* supported fsm codes */
	u_int32_t		req_codes;	/* required fsm codes */

	/* FSM configuration options */
	const struct ppp_fsm_optdesc	*options;	/* option descriptors */
	const struct ppp_fsm_options	*defaults;	/* default options */

	/* Required callbacks */
	ppp_fsm_type_destroy_t		*destroy;
	ppp_fsm_type_build_conf_req_t	*build_conf_req;
	ppp_fsm_type_recv_conf_req_t	*recv_conf_req;
	ppp_fsm_type_recv_conf_rej_t	*recv_conf_rej;
	ppp_fsm_type_recv_conf_nak_t	*recv_conf_nak;

	/* Magic numbers */
	ppp_fsm_type_get_magic_t	*get_magic;

	/* Optional callbacks */
	ppp_fsm_type_recv_reset_req_t	*recv_reset_req;
	ppp_fsm_type_recv_reset_ack_t	*recv_reset_ack;
	ppp_fsm_type_recv_vendor_t	*recv_vendor;
};

/*
 * One instance of an FSM
 */
struct ppp_fsm_instance {
	const struct ppp_fsm_type	*type;	/* which type of fsm */
	struct ppp_fsm			*fsm;	/* back-pointer to fsm */
	void				*arg;	/* per-instance info */
};

__BEGIN_DECLS

/* Functions */
extern struct	ppp_fsm *ppp_fsm_create(struct pevent_ctx *ctx,
			pthread_mutex_t *mutex, struct ppp_fsm_instance *inst,
			struct ppp_log *log);
extern void	ppp_fsm_destroy(struct ppp_fsm **fsmp);

extern struct	mesg_port *ppp_fsm_get_outport(struct ppp_fsm *fsm);

extern void	ppp_fsm_input(struct ppp_fsm *fsm,
			enum ppp_fsm_input input, ...);
extern void	ppp_fsm_free_output(struct ppp_fsm_output *output);
extern enum	ppp_fsm_state ppp_fsm_get_state(struct ppp_fsm *fsm);
extern time_t	ppp_fsm_last_heard(struct ppp_fsm *fsm);
extern struct	ppp_fsm_instance *ppp_fsm_get_instance(struct ppp_fsm *fsm);
extern void	ppp_fsm_send_reset_req(struct ppp_fsm *fsm,
			const void *data, size_t dlen);
extern void	ppp_fsm_send_reset_ack(struct ppp_fsm *fsm,
			const void *data, size_t dlen);

extern const	char *ppp_fsm_output_str(struct ppp_fsm_output *output);
extern const	char *ppp_fsm_reason_str(struct ppp_fsm_output *output);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_FSM_H_ */
