
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_CHANNEL_H_
#define _PDEL_PPP_PPP_CHANNEL_H_

#ifndef _PDEL_PPP_PRIVATE_H_
#error "This header is only for use by the ppp library."
#endif

struct ppp_channel;

/* Channel events */
enum ppp_channeloutput {
	PPP_CHANNEL_OUTPUT_DOWN_FATAL = 1,	/* unrecoverable down event */
	PPP_CHANNEL_OUTPUT_DOWN_NONFATAL,	/* recoverable down event */
	PPP_CHANNEL_OUTPUT_UP,			/* up event */
};

/* Channel output structure */
struct ppp_channel_output {
	enum ppp_channeloutput		type;	/* type of output */
	char				*info;	/* info about down events */
};

/* Channel methods */
typedef void	ppp_channel_open_t(struct ppp_channel *chan);
typedef void	ppp_channel_close_t(struct ppp_channel *chan);
typedef void	ppp_channel_destroy_t(struct ppp_channel **chanp);
typedef void	ppp_channel_free_output_t(struct ppp_channel *chan,
			struct ppp_channel_output *output);

/* Methods supported by devices only */
typedef void	ppp_channel_set_link_info_t(struct ppp_channel *chan,
			u_int32_t accm);
typedef int	ppp_channel_get_origination_t(struct ppp_channel *chan);
typedef const	char *ppp_channel_get_node_t(struct ppp_channel *chan);
typedef const	char *ppp_channel_get_hook_t(struct ppp_channel *chan);
typedef int	ppp_channel_is_async_t(struct ppp_channel *chan);
typedef u_int	ppp_channel_get_mtu_t(struct ppp_channel *chan);
typedef int	ppp_channel_get_acfcomp_t(struct ppp_channel *chan);
typedef int	ppp_channel_get_pfcomp_t(struct ppp_channel *chan);

/* Channel methods */
struct ppp_channel_meth {
	ppp_channel_open_t		*open;
	ppp_channel_close_t		*close;
	ppp_channel_destroy_t		*destroy;
	ppp_channel_free_output_t	*free_output;
	ppp_channel_set_link_info_t	*set_link_info;
	ppp_channel_get_origination_t	*get_origination;
	ppp_channel_get_node_t		*get_node;
	ppp_channel_get_hook_t		*get_hook;
	ppp_channel_is_async_t		*is_async;
	ppp_channel_get_mtu_t		*get_mtu;
	ppp_channel_get_acfcomp_t	*get_acfcomp;
	ppp_channel_get_pfcomp_t	*get_pfcomp;
};

/* Channel structure */
struct ppp_channel {
	struct ppp_channel_meth	*meth;			/* methods */
	void			*priv;			/* channel private */
	struct mesg_port	*outport;		/* output msg port */
};

__BEGIN_DECLS

/* Convenience functions */
extern ppp_channel_open_t		ppp_channel_open;
extern ppp_channel_close_t		ppp_channel_close;
extern ppp_channel_destroy_t		ppp_channel_destroy;
extern ppp_channel_free_output_t	ppp_channel_free_output;
extern ppp_channel_set_link_info_t	ppp_channel_set_link_info;
extern ppp_channel_get_origination_t	ppp_channel_get_origination;
extern ppp_channel_get_node_t		ppp_channel_get_node;
extern ppp_channel_get_hook_t		ppp_channel_get_hook;
extern ppp_channel_is_async_t		ppp_channel_is_async;
extern ppp_channel_get_mtu_t		ppp_channel_get_mtu;
extern ppp_channel_get_acfcomp_t	ppp_channel_get_acfcomp;
extern ppp_channel_get_pfcomp_t		ppp_channel_get_pfcomp;

extern struct	mesg_port *ppp_channel_get_outport(struct ppp_channel *chan);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_CHANNEL_H_ */
