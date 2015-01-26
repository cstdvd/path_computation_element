/*
 * pd_regex.h
 *
 * PD integration of a private copy of FreeBSD regex to isolate portability
 * issues.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

/*-
 * Copyright (c) 1992 Henry Spencer.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer of the University of Toronto.
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
 *
 *	@(#)regex.h	8.2 (Berkeley) 1/3/94
 * $FreeBSD: src/include/regex.h,v 1.11 2004/07/12 06:07:26 tjr Exp $
 */

#ifndef _PD_REGEX_H_
#define	_PD_REGEX_H_

#ifndef WIN32
#include <sys/cdefs.h>
#endif
#include <sys/types.h>

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif

/* 
 * We don't build our private regex on platforms with a full regex including
 * REG_STARTEND.  Unfortunately, Linux doesn't reliably have REG_STARTEND.
 */
#if !defined(WIN32) && !defined(linux) 
#define SYS_REGEX_WORKS 1
#endif

/* glibc 2.5 seems to have finally picked up REG_STARTEND */
#ifdef __GLIBC__
#if  __GLIBC_PREREQ(2,5)
#define SYS_REGEX_WORKS 1
#endif
#endif

#ifdef SYS_REGEX_WORKS
#include <regex.h>

#define	pd_regcomp(r, c, f)	regcomp((r), (c), (f))
#define	pd_regerror(f, r, c, s)	regerror((f), (r), (c), (s))
#define pd_regexec(r, c, s, m, f) regexec((r), (c), (s), (m), (f))
#define pd_regfree(x)		regfree(x)

#else

#ifdef WIN32
#define REGEX_OVERRIDE
#endif

#ifdef REGEX_OVERRIDE
#define	regcomp(r, c, f)	pd_regcomp((r), (c), (f))
#define	regerror(f, r, c, s)	pd_regerror((f), (r), (c), (s))
#define regexec(r, c, s, m, f)	pd_regexec((r), (c), (s), (m), (f))
#define regfree(x)		pd_regfree(x)
#endif

/* types */
typedef	__off_t		regoff_t;

#if !defined(_SIZE_T_DECLARED) && !defined(_SIZE_T_DEFINED)
typedef	__size_t	size_t;
#define	_SIZE_T_DECLARED
#define _SIZE_T_DEFINED
#endif

typedef struct {
	int re_magic;
	size_t re_nsub;		/* number of parenthesized subexpressions */
	__const char *re_endp;	/* end pointer for REG_PEND */
	struct re_guts *re_g;	/* none of your business :-) */
} regex_t;

typedef struct {
	regoff_t rm_so;		/* start of match */
	regoff_t rm_eo;		/* end of match */
} regmatch_t;

/* regcomp() flags */
#define	REG_BASIC	0000
#define	REG_EXTENDED	0001
#define	REG_ICASE	0002
#define	REG_NOSUB	0004
#define	REG_NEWLINE	0010
#define	REG_NOSPEC	0020
#define	REG_PEND	0040
#define	REG_DUMP	0200

/* regerror() flags */
#define	REG_ENOSYS	(-1)
#define	REG_NOMATCH	 1
#define	REG_BADPAT	 2
#define	REG_ECOLLATE	 3
#define	REG_ECTYPE	 4
#define	REG_EESCAPE	 5
#define	REG_ESUBREG	 6
#define	REG_EBRACK	 7
#define	REG_EPAREN	 8
#define	REG_EBRACE	 9
#define	REG_BADBR	10
#define	REG_ERANGE	11
#define	REG_ESPACE	12
#define	REG_BADRPT	13
#define	REG_EMPTY	14
#define	REG_ASSERT	15
#define	REG_INVARG	16
#define	REG_ILLSEQ	17
#define	REG_ATOI	255	/* convert name to number (!) */
#define	REG_ITOA	0400	/* convert number to name (!) */

/* regexec() flags */
#define	REG_NOTBOL	00001
#define	REG_NOTEOL	00002
#define	REG_STARTEND	00004
#define	REG_TRACE	00400	/* tracing of execution */
#define	REG_LARGE	01000	/* force large representation */
#define	REG_BACKR	02000	/* force use of backref code */

__BEGIN_DECLS
int	pd_regcomp(regex_t * __restrict, const char * __restrict, int);
size_t	pd_regerror(int, const regex_t * __restrict, char * __restrict, size_t);
/*
 * XXX forth parameter should be `regmatch_t [__restrict]', but isn't because
 * of a bug in GCC 3.2 (when -std=c99 is specified) which perceives this as a
 * syntax error.
 */
int	pd_regexec(const regex_t * __restrict, const char * __restrict, size_t,
	    regmatch_t * __restrict, int);
void	pd_regfree(regex_t *);
__END_DECLS

#endif /* SYS_REGEX_WORKS */
#endif /* !_REGEX_H_ */
