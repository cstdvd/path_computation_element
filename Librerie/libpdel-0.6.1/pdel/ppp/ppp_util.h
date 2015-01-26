
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_UTIL_H_
#define _PDEL_PPP_PPP_UTIL_H_

__BEGIN_DECLS

extern void	ppp_util_ascify(char *buf, size_t max,
			const u_char *bytes, size_t len);
extern int	ppp_util_random(void *buf, size_t len);

__END_DECLS

#endif	/* _PDEL_PPP_PPP_UTIL_H_ */
