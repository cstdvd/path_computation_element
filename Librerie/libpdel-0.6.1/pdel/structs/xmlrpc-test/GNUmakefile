# $Id: GNUmakefile 1008 2006-12-05 01:48:31Z mgooderum $

DOTDOT=../../
include ${DOTDOT}GNUmakefile.inc

srcs=		main.c arrayOfStructsTest.c countTheEntities.c \
		easyStructTest.c echoStructTest.c manyTypesTest.c \
		moderateSizeArrayCheck.c nestedStructTest.c \
		simpleStructReturnTest.c faultTest.c uname.c
objs=		$(srcs:.c=.o)
prog=		xmlrpc_test
CFLAGS=		-I$(prefix)/include -Wall -DOPENSSL_NO_KRB5
cc=		cc

$(prog):	$(objs)
	$(cc) -o $@ $(objs) $(CFLAGS) $(DFLAGS) $(pdellink_static)

%.o:		%.c
	$(cc) -c -o $@ $(CFLAGS) $(DFLAGS) $+

uname.c:	GNUmakefile
	(  HOST_OS=`uname -s`; HOST_ARCH=`uname -m`; \
	    echo '#include "xmlrpc_test.h"'; \
	    echo -n 'const char host_os[] = "'; \
	    echo -n $${HOST_OS}; \
	    echo '";'; \
	    echo -n 'const char host_arch[] = "'; \
	    echo -n $${HOST_ARCH}; \
	    echo '";' ) \
	> $@

clean:
	rm -f $(prog) $(objs) uname.c

