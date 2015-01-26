/*
 * pd_string.h
 *
 * PD string releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_STRING_H__
#define __PDEL_PD_STRING_H__

/* Public Test */
#define PD_STRING_INCLUDED 1

#include <sys/time.h>

#ifndef PD_PORT_INCLUDED
#include <pdel/pd_port.h>
#endif

__BEGIN_DECLS

/*
 * Misc. string funcs including those missing on some platforms.
 */

char *
pd_strsep(char **stringp, const char *delim);

char *
pd_realpath(const char *name, char *resolved);

const char *
pd_gai_strerror(int ecode);

__END_DECLS


#endif
