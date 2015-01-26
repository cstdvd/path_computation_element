
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_PACTION_H_
#define _PDEL_UTIL_PACTION_H_

struct paction;

typedef void	paction_handler_t(void *arg);
typedef void	paction_finish_t(void *arg, int was_canceled);

__BEGIN_DECLS

extern int	paction_start(struct paction **actionp, pthread_mutex_t *mutex,
			paction_handler_t *handler, paction_finish_t *finish,
			void *arg);

extern void	paction_cancel(struct paction **actionp);

__END_DECLS

#endif	/* _PDEL_UTIL_PACTION_H_ */
