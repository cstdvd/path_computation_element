# $Id: Makefile 911 2004-06-03 21:17:30Z archie $

PROG=		tmpl
SRCS+=		main.c
NOMAN=
CFLAGS+=	-pthread
CFLAGS+=	-I${PREFIX}/include

#CFLAGS+=	-DTMPL_DEBUG=1

CLEANFILES+=	test.out test.diff

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

# Tests

test:
		rm -f test.out
		${MAKE} test.out
		${MAKE} test-input

test-input:
		${.CURDIR}/${PROG} test-input.in

test.out:	${PROG} test.in
		${.CURDIR}/${PROG} -n test.in > ${.TARGET}
		@if ! diff -q ${.TARGET}.sav ${.TARGET}; then \
			echo ERROR: Test output does not match saved output; \
			echo ERROR: The difference is saved in test.diff; \
			diff -u ${.TARGET}.sav ${.TARGET} > test.diff; \
			exit 1; \
		fi

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
