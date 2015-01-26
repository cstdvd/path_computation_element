
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_LOG_H_
#define _PDEL_PPP_PPP_LOG_H_

struct ppp_log;
struct ppp_log_private;

typedef void	ppp_log_vput_t(void *arg, int sev,
			const char *fmt, va_list args);
typedef void	ppp_log_close_t(void *arg);

__BEGIN_DECLS

/* Functions */
extern struct	ppp_log *ppp_log_create(void *arg,
			ppp_log_vput_t *put, ppp_log_close_t *close);
extern struct	ppp_log *ppp_log_dup(struct ppp_log *log);
extern void	ppp_log_put(struct ppp_log *log, int sev, const char *fmt, ...);
extern void	ppp_log_vput(struct ppp_log *log,
			int sev, const char *fmt, va_list args);
extern void	ppp_log_dump(struct ppp_log *log, int sev,
			const void *data, size_t len);
extern void	ppp_log_close(struct ppp_log **logp);

extern struct	ppp_log *ppp_log_prefix(struct ppp_log *log,
			const char *fmt, ...);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_LOG_H_ */
