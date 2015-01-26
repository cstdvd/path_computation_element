# $Id: Makefile 911 2004-06-03 21:17:30Z archie $

PREFIX?=	/usr/local
PROGS=		arptest iftest
CFLAGS+=	-I${PREFIX}/include
LDFLAGS+=	-L${PREFIX}/lib -lpdel -lcrypto -lssl -lnetgraph \
		-lradius -lexpat -pthread

all:	${PROGS}

clean cleandir:
	rm -f ${PROGS}

${PROG}:	${PROG}.c
	cc ${CFLAGS} ${.ALLSRC} -o ${.TARGET}

.include "../../Makefile.inc"
