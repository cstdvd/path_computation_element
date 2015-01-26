
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_INTERNAL_H_
#define _PDEL_UTIL_INTERNAL_H_

struct mesg_port;
struct pevent;

__BEGIN_DECLS

/* pevent.c */
extern int	_pevent_canceled(struct pevent *ev);
extern void	_pevent_unref(struct pevent *ev);

/* mesg_port.c */
extern int	_mesg_port_set_event(struct mesg_port *port, struct pevent *ev);

__END_DECLS

#endif	/* _PDEL_UTIL_INTERNAL_H_ */
