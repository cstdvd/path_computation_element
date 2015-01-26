
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_FSM_OPTION_H_
#define _PDEL_PPP_PPP_FSM_OPTION_H_

#ifndef _PDEL_PPP_PRIVATE_H_
#error "This header is only for use by the ppp library."
#endif

struct ppp_log;
struct ppp_fsm_optdesc;

/* One configuration option */
struct ppp_fsm_option {
	u_char	type;
	u_char	len;
	u_char	*data;
};

/* A set of configuration options */
struct ppp_fsm_options {
	u_int			num;
	struct ppp_fsm_option	*opts;
};

/* Option descriptors */
typedef	void	opt_pr_t(const struct ppp_fsm_optdesc *desc,
			const struct ppp_fsm_option *opt,
			char *buf, size_t bmax);

struct ppp_fsm_optdesc {
	const char	*name;		/* option name */
	u_char		type;		/* option type value */
	u_char		min;		/* option contents min valid length */
	u_char		max;		/* option contents max valid length */
	u_char		supported;	/* whether option is supported */
	opt_pr_t	*print;		/* optional contents ascii decoder */
};

__BEGIN_DECLS

/* Built-in descriptor functions */
extern opt_pr_t	ppp_fsm_pr_binary;	/* show option as binary data */
extern opt_pr_t	ppp_fsm_pr_hex32;	/* show option as 32 bit hex number */
extern opt_pr_t	ppp_fsm_pr_int16;	/* show option as decimal u_int16_t */

/* Functions */
extern struct	ppp_fsm_options *ppp_fsm_option_create(void);
extern void	ppp_fsm_option_destroy(struct ppp_fsm_options **optsp);
extern int	ppp_fsm_option_add(struct ppp_fsm_options *opts, u_char type,
			u_char len, const void *data);
extern int	ppp_fsm_option_del(struct ppp_fsm_options *opts, u_int index);
extern void	ppp_fsm_option_zero(struct ppp_fsm_options *opts);
extern struct	ppp_fsm_options *ppp_fsm_option_copy(
			struct ppp_fsm_options *opts);
extern int	ppp_fsm_option_equal(const struct ppp_fsm_options *o1,
			int i1, const struct ppp_fsm_options *o2, int i2);

extern const	struct ppp_fsm_optdesc *ppp_fsm_option_desc(
			const struct ppp_fsm_optdesc *list,
			const struct ppp_fsm_option *opt);
extern void	ppp_fsm_options_decode(const struct ppp_fsm_optdesc *optlist,
			const u_char *data, u_int len, char *buf, size_t bmax);

extern struct	ppp_fsm_options *ppp_fsm_option_unpack(const u_char *data,
			u_int len);
extern u_int	ppp_fsm_option_packlen(struct ppp_fsm_options *opts);
extern void	ppp_fsm_option_pack(struct ppp_fsm_options *opts, u_char *buf);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_FSM_OPTION_H_ */
