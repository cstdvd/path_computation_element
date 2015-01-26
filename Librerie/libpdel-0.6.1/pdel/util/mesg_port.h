
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_MESG_PORT_H_
#define _PDEL_UTIL_MESG_PORT_H_

/*
 * Message ports
 */

struct mesg_port;

typedef struct mesg_port *mesg_port_h;

__BEGIN_DECLS

/*
 * Create a new message port.
 */
extern struct	mesg_port *mesg_port_create(const char *mtype);

/*
 * Destroy a message port.
 */
extern void	mesg_port_destroy(struct mesg_port **portp);

/*
 * Send a message down a message port.
 */
extern int	mesg_port_put(struct mesg_port *port, void *data);

/*
 * Read the next message from a message port.
 */
extern void	*mesg_port_get(struct mesg_port *port, int timeout);

/*
 * Get the number of messages queued on a message port.
 */
extern u_int	mesg_port_qlen(struct mesg_port *port);

__END_DECLS

#endif	/* _PDEL_UTIL_MESG_PORT_H_ */
