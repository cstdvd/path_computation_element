
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_UTIL_STRING_QUOTE_H_
#define _PDEL_UTIL_STRING_QUOTE_H_

__BEGIN_DECLS

/*
 * Put a string in double quotes with unprintable characters escaped.
 *
 * The caller must free the result.
 */
extern char	*string_enquote(const char *str, const char *mtype);

/*
 * Read in a doubly-quoted string token. The input stream is assumed
 * to be pointing at the character after the opening quote. Upon return
 * it will be pointing to the character after the closing quote,
 * unless there was a system error, or EOF was reached, in which case
 * NULL is returned. In the EOF case, errno will be EINVAL.
 *
 * The normal C backslash escapes are recognized.
 * The caller must free the result.
 */
extern char	*string_dequote(FILE *fp, const char *mtype);

__END_DECLS

#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

#endif	/* _PDEL_UTIL_STRING_QUOTE_H_ */

