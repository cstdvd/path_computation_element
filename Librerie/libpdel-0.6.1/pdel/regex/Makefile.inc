#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/regex/Makefile.inc,v 1.7 2002/11/18 09:50:56 ru Exp $

# regex sources

REGEX_SRCDIR=	regex

REGEX_SRCS+=	regcomp.c regerror.c regexec.c regfree.c

#regex_SRCS=	

#regex_INCS+=

CFLAGS+=-DPOSIX_MISTAKE

#MAN+=	regex.3
#MAN+=	re_format.7

#MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3
#MLINKS+=regexec.3 regfree.3
