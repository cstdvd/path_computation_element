
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_NET_UROUTE_H_
#define _PDEL_NET_UROUTE_H_

struct uroute;

__BEGIN_DECLS

extern struct	uroute *uroute_create(const struct sockaddr *dest,
			const struct sockaddr *gateway,
			const struct sockaddr *netmask);
extern void	uroute_destroy(struct uroute **routep);
extern const	struct sockaddr *uroute_get_dest(struct uroute *route);
extern const	struct sockaddr *uroute_get_gateway(struct uroute *route);
extern const	struct sockaddr *uroute_get_netmask(struct uroute *route);
extern int	uroute_get_flags(struct uroute *route);
extern void	uroute_set_flags(struct uroute *route, int flags);
extern int	uroute_add(struct uroute *route);
extern int	uroute_delete(struct uroute *route);
extern struct	uroute *uroute_get(const struct sockaddr *dest);
extern int	uroute_get_all(struct uroute ***listp, const char *mtype);
extern void	uroute_print(struct uroute *route, FILE *fp);

__END_DECLS

#endif	/* _PDEL_NET_UROUTE_H_ */

