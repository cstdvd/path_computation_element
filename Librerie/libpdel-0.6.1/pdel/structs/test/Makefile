# $Id: Makefile 911 2004-06-03 21:17:30Z archie $

PROG=		structs_test
SRCS=		main.c
NOMAN=

CFLAGS+=	-pthread
CFLAGS+=	-I${PREFIX}/include

# Libraries

LDADD+=		-L${PREFIX}/lib -lpdel
DPADD+=		${PREFIX}/lib/libpdel.a

LDADD+=		-L${PREFIX}/lib -lexpat
DPADD+=		${PREFIX}/lib/libexpat.a

LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}

LDADD+=		-lpcap
DPADD+=		${LIBPCAP}

LDADD+=		-lssl
DPADD+=		${LIBSSL}

LDADD+=		-lnetgraph
DPADD+=		${LIBNETGRAPH}

LDADD+=		-lradius
DPADD+=		${LIBRADIUS}

# test target
test:
	./structs_test -ncx < structs_test.xml > structs_test.nrm
	./structs_test -ecx < structs_test.xml > structs_test.bin
	./structs_test -dx < structs_test.bin | diff -u structs_test.nrm -
	./structs_test -tcx < structs_test.xml > structs_test.xmlrpc
	./structs_test -Tcx < structs_test.xmlrpc | diff -u structs_test.nrm -
	./structs_test -c < structs_test.xml

CLEANFILES+=	structs_test.bin structs_test.nrm structs_test.xmlrpc

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
