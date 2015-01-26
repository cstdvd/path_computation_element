# $Id: Makefile 962 2005-02-03 17:50:03Z archie $

PROG=		xmlrpc_test
NOMAN=

SRCS+=		main.c
SRCS+=		arrayOfStructsTest.c
SRCS+=		countTheEntities.c
SRCS+=		easyStructTest.c
SRCS+=		echoStructTest.c
SRCS+=		manyTypesTest.c
SRCS+=		moderateSizeArrayCheck.c
SRCS+=		nestedStructTest.c
SRCS+=		simpleStructReturnTest.c
SRCS+=		faultTest.c

SRCS+=		uname.c
CLEANFILES+=	uname.c

uname.c:	Makefile
	(  HOST_OS=`uname -s`; HOST_ARCH=`uname -m`; \
	    echo '#include "xmlrpc_test.h"'; \
	    echo -n 'const char host_os[] = "'; \
	    echo -n $${HOST_OS}; \
	    echo '";'; \
	    echo -n 'const char host_arch[] = "'; \
	    echo -n $${HOST_ARCH}; \
	    echo '";' ) \
	> ${.TARGET}

CFLAGS+=	-pthread
CFLAGS+=	-I${PREFIX}/include

# Libraries

LDADD+=		-L${PREFIX}/lib -lpdel
DPADD+=		${PREFIX}/lib/libpdel.a

LDADD+=		-L${PREFIX}/lib -lexpat
DPADD+=		${PREFIX}/lib/libexpat.a

LDADD+=		-lssl
DPADD+=		${LIBSSL}

LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}

LDADD+=		-lnetgraph
DPADD+=		${LIBNETGRAPH}

LDADD+=		-lradius
DPADD+=		${LIBRADIUS}

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
