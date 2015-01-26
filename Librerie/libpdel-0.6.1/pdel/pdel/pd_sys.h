/*
 * pd_sys.h
 *
 * PD system releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_SYS_H__
#define __PDEL_PD_SYS_H__

#ifndef PD_BASE_INCLUDED
#include <pdel/pd_base.h>
#endif

/* Public Test */
#define PD_SYS_INCLUDED 1

__BEGIN_DECLS

/*
 * Misc. string funcs including those missing on some platforms.
 */

int
pd_chown(const char *path, int uid, int gid);

int
pd_getopt(const char *progname, int argc, char * const argv[], 
	  const char *optstr);

PD_IMPORT char * pd_optarg;			/* getopt(3) external variables */
PD_IMPORT int pd_optind;
PD_IMPORT int pd_opterr;
PD_IMPORT int pd_optopt;

long 
pd_getpid(void);

__END_DECLS


#endif
 
