
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_NET_IF_UTIL_H_
#define _PDEL_NET_IF_UTIL_H_

struct sockaddr_dl;

__BEGIN_DECLS

/*
 * Gets the names of all system interfaces, returned as an array of
 * char * pointers. Caller must free each pointer and the array itself.
 * Returns the length of the array.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_get_list(char ***listp, const char *mtype);

/*
 * Get the interface type.
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_get_type(const char *ifname);

/*
 * Get the flags associated with an interface.
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_get_flags(const char *ifname);

/*
 * Set the flags associated with an interface.
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_set_flags(const char *ifname, int flags);

/*
 * Get the MTU associated with an interface.
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_get_mtu(const char *ifname);

/*
 * Set the MTU associated with an interface.
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_set_mtu(const char *ifname, u_int mtu);

/*
 * Get the link address associated with an interface.
 * Caller must free the returned structure.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_get_link_addr(const char *ifname,
			struct sockaddr_dl **sdlp, const char *mtype);

/*
 * Get a list of all IP addresses and netmasks configured on an interface.
 * Caller must free both lists. Returns the length of the arrays.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_get_ip_addrs(const char *ifname, struct in_addr **iplistp,
			struct in_addr **nmlistp, const char *mtype);

/*
 * Get the first IP address and netmask configured on an interface.
 * "ipp" and/or "nmp" may be NULL. Returns -1 and sets errno on error;
 * in particular, if there are no addresses, then errno will be ENOENT.
 */
extern int	if_get_ip_addr(const char *ifname,
			struct in_addr *ipp, struct in_addr *nmp);

/*
 * Add an IP address assignment to a broadcast or p2p interface.
 * The "dest" should be 0.0.0.0 for non-p2p interfaces.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_add_ip_addr(const char *iface, struct in_addr ip,
			struct in_addr mask, struct in_addr dest);

/*
 * Remove an IP address assignment from a broadcast or p2p interface.
 * The "dest" should be 0.0.0.0 for non-p2p interfaces.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_del_ip_addr(const char *iface, struct in_addr ip,
			struct in_addr mask, struct in_addr dest);

/*
 * Get an ARP entry.
 *
 * Returns -1 and sets errno if there was a problem.
 * In particular, errno = ENOENT if entry not found.
 */
extern int	if_get_arp(struct in_addr ip, u_char *ether);

/*
 * Set (or remove, if ether == NULL) an ARP entry.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_set_arp(struct in_addr ip, const u_char *ether,
			int temp, int publish);

/*
 * Flush all ARP entries.
 *
 * Returns -1 and sets errno if there was a problem.
 */
extern int	if_flush_arp(void);

__END_DECLS

#endif	/* _PDEL_NET_IF_UTIL_H_ */

