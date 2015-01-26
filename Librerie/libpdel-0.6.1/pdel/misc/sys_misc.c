/*
 * sys_misc.c
 *
 * PD system releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/cdefs.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <winsock2.h>
#include <windows.h>
#endif

#include "pdel/pd_sys.h"

/*
 * The PDEL configuration.
 */

const pd_config pd_config_default = {
	PDC_CUR_VERSION,
	PDC_DEF_FLAGS,
	PDC_DEF_THREADWAIT
};

int		pd_init_done = 0;
pd_config	pd_config_running;
#ifdef WIN32
WSADATA		pd_wsinfo;
int		pd_fmode_save = -1;
#endif

/*
 * VS 2k3 is missing these..
 */
#ifdef WIN32
#ifndef _set_fmode
#define _set_fmode(x) _fmode = (x)
#endif
#ifndef _get_fmode
#define _get_fmode(x) (x) = _fmode
#endif
#endif

/*
 * Initialize the PDEL library.  NULL pdc for reasonable defaults.
 *
 */
int
pd_init(const pd_config *pdc)
{
	if (pd_init_done++ > 0) {
		return(0);
	}
	if (pdc == NULL) {
		pd_config_running = pd_config_default;
	} else {
		pd_config_running = *pdc;
	}
#ifdef WIN32
	if (!PDB_ISSET(pd_config_running.flags, PDC_NO_NET)) {
	   WORD	wsVerReq = MAKEWORD( 2, 0 );
	   
	   WSAStartup(wsVerReq, &pd_wsinfo);
	}
	if (!PDB_ISSET(pd_config_running.flags, PDC_NO_BINARY)) {
		_get_fmode(pd_fmode_save);
		_set_fmode(_O_BINARY);
	}
#endif
	return(0);
}

/*
 * Cleanup the PDEL library.  (Eventually) terminate service threads, 
 * free internal memory if possible, etc.
 */
int
pd_cleanup(const pd_config *pdc)
{
	if (pd_init_done-- > 1) {
		return(0);
	}
#ifdef WIN32
	if (pd_fmode_save > 0) {
		_set_fmode(pd_fmode_save);
	}
	if (!PDB_ISSET(pd_config_running.flags, PDC_NO_NET)) {
		WSACleanup();
	}
#endif
	return(0);
}

/*
 * Misc. compatiability stubs.
 */
long 
pd_getpid(void)
{
#ifndef WIN32
	return(getpid());
#else
	return(GetCurrentProcessId());
#endif
}

int
pd_chown(const char *path, int uid, int gid)
{
#ifdef WIN32
	return(0);
#else
	return(chown(path, uid, gid));
#endif

}
/*	$NetBSD: getopt.c,v 1.26 2003/08/07 16:43:40 agc Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "pdel/pd_sys.h"

PD_EXPORT int	pd_opterr = 1;		/* if error message should be printed */
PD_EXPORT int	pd_optind = 1;		/* index into parent argv vector */
PD_EXPORT int	pd_optopt;			/* character checked for validity */
PD_EXPORT int	pd_optreset;		/* reset getopt */
PD_EXPORT char *pd_optarg;		/* argument associated with option */

#define	BADCH	(int)'?'
#define	BADARG	(int)':'
#define	EMSG	((char *)"")

/*
 * pd_getopt --
 *	Parse argc/argv argument vector.
 */
int
pd_getopt(pname, nargc, nargv, ostr)
	const char *pname;
	int nargc;
	char * const nargv[];
	const char *ostr;
{
	static char *place = EMSG;	/* pd_option letter processing */
	char *oli;			/* pd_option letter list index */

	if (pname == NULL) {
		pname = "option";
	}
	if (pd_optreset || *place == 0) { /* update scanning pointer */
		pd_optreset = 0;
		place = nargv[pd_optind];
		if (pd_optind >= nargc || *place++ != '-') {
			/* Argument is absent or is not an pd_option */
			place = EMSG;
			return (-1);
		}
		pd_optopt = *place++;
		if (pd_optopt == '-' && *place == 0) {
			/* "--" => end of pd_options */
			++pd_optind;
			place = EMSG;
			return (-1);
		}
		if (pd_optopt == 0) {
			/* Solitary '-', treat as a '-' pd_option
			   if the program (eg su) is looking for it. */
			place = EMSG;
			if (strchr(ostr, '-') == NULL)
				return (-1);
			pd_optopt = '-';
		}
	} else
		pd_optopt = *place++;

	/* See if pd_option letter is one the caller wanted... */
	if (pd_optopt == ':' || (oli = strchr(ostr, pd_optopt)) == NULL) {
		if (*place == 0)
			++pd_optind;
		if (pd_opterr && *ostr != ':')
			(void)fprintf(stderr,
			    "%s: illegal pd_option -- %c\n", pname,
			    pd_optopt);
		return (BADCH);
	}

	/* Does this pd_option need an argument? */
	if (oli[1] != ':') {
		/* don't need argument */
		pd_optarg = NULL;
		if (*place == 0)
			++pd_optind;
	} else {
		/* Pd_Option-argument is either the rest of this argument or the
		   entire next argument. */
		if (*place)
			pd_optarg = place;
		else if (nargc > ++pd_optind)
			pd_optarg = nargv[pd_optind];
		else {
			/* pd_option-argument absent */
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (pd_opterr)
				(void)fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    pname, pd_optopt);
			return (BADCH);
		}
		place = EMSG;
		++pd_optind;
	}
	return (pd_optopt);			/* return pd_option letter */
}
