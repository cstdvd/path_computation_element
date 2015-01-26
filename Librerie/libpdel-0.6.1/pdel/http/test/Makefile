# $Id: Makefile 911 2004-06-03 21:17:30Z archie $

PROG=		demo_server
SRCS+=		main.c
NOMAN=

CFLAGS+=	-pthread
CFLAGS+=	-I${PREFIX}/include

clean_certs:
		cd ${.CURDIR}/cert && ${MAKE} clean

# SSL key and cert
${PROG}:	${.CURDIR}/demo.key ${.CURDIR}/demo.crt

${.CURDIR}/demo.key ${.CURDIR}/demo.crt:
		cd ${.CURDIR}/cert && ${MAKE}

CLEANFILES+=	${.CURDIR}/demo.key ${.CURDIR}/demo.crt

# Libraries

LDADD+=		-L${PREFIX}/lib -lpdel
DPADD+=		${PREFIX}/lib/libpdel.a

LDADD+=		-L${PREFIX}/lib -lexpat
DPADD+=		${PREFIX}/lib/libexpat.a

LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}

LDADD+=		-lssl
DPADD+=		${LIBSSL}

LDADD+=		-lnetgraph
DPADD+=		${LIBNETGRAPH}

LDADD+=		-lradius
DPADD+=		${LIBRADIUS}

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>

clean cleandir:	clean_certs
