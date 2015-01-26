# $Id: Makefile 911 2004-06-03 21:17:30Z archie $

PROG=		demo_server
SRCS+=		main.c
NOMAN=

CFLAGS+=	-pthread
CFLAGS+=	-I${PREFIX}/include

# Libraries

LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}

LDADD+=		-lnetgraph
DPADD+=		${LIBNETGRAPH}

LDADD+=		-lssl
DPADD+=		${LIBSSL}

LDADD+=		-lnetgraph
DPADD+=		${LIBNETGRAPH}

LDADD+=		-lradius
DPADD+=		${LIBRADIUS}

LDADD+=		-L${PREFIX}/lib -lexpat
DPADD+=		${PREFIX}/lib/libexpat.a

LDADD+=		-L${PREFIX}/lib -lpdel
DPADD+=		${PREFIX}/lib/libpdel.a

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
