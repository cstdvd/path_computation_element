
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_PEVENT_H_
#define _PDEL_UTIL_PEVENT_H_

/*
 * Event-based scheduling
 */

struct pevent;
struct pevent_ctx;
struct mesg_port;

typedef struct pevent		*pevent_h;
typedef struct pevent_ctx	*pevent_ctx_h;

#define PEVENT_MAX_EVENTS		128

/*
 * Event handler function type
 */
typedef void	pevent_handler_t(void *arg);

/*
 * Event types
 */
enum pevent_type {
	PEVENT_READ = 1,
	PEVENT_WRITE,
	PEVENT_TIME,
	PEVENT_MESG_PORT,
	PEVENT_USER
};

/*
 * Event info structure filled in by pevent_get_info().
 */
typedef struct pevent_info {
	enum pevent_type	type;	/* event type */
	union {
	    int			fd;	/* file descriptor (READ, WRITE) */
	    int			millis;	/* delay in milliseconds (TIME) */
	    struct mesg_port	*port;	/* mesg_port(3) (MESG_PORT) */
	}			u;
} pevent_info;

/*
 * Event flags
 */
#define PEVENT_RECURRING	0x0001
#define PEVENT_OWN_THREAD	0x0002

__BEGIN_DECLS

/*
 * Create a new event context.
 */
extern struct	pevent_ctx *pevent_ctx_create(const char *mtype,
			const pthread_attr_t *attr);

/*
 * Destroy an event context.
 *
 * All events are unregistered. This is safe to be called
 * from within an event handler.
 */
extern void	pevent_ctx_destroy(struct pevent_ctx **ctxp);

/*
 * Return the number of registered events.
 */
extern u_int	pevent_ctx_count(struct pevent_ctx *ctx);

/*
 * Create a new event.
 */
extern int	pevent_register(struct pevent_ctx *ctx, struct pevent **peventp,
			int flags, pthread_mutex_t *mutex,
			pevent_handler_t *handler, void *arg,
			enum pevent_type type, ...);

/*
 * Trigger a user event.
 */
extern void	pevent_trigger(struct pevent *pevent);

/*
 * Get the type and parameters for an event.
 */
extern int	pevent_get_info(struct pevent *pevent,
			struct pevent_info *info);

/*
 * Destroy an event.
 */
extern void	pevent_unregister(struct pevent **eventp);

__END_DECLS

#endif	/* _PDEL_UTIL_PEVENT_H_ */
