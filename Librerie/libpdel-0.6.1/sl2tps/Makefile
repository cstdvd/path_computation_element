# $Id: Makefile 1073 2007-05-10 18:21:05Z mgooderum $

PROG=		sl2tps
VERSION=	0.4.2
SRCS=		main.c \
		sls_auth.c \
		sls_config.c \
		sls_l2tp.c \
		sls_manager.c \
		sls_server_subsystem.c

PREFIX?=	/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/man/man
CONFIG_SAMPLE=	config.xml.sample
MAN1=		${PROG}.1

CFLAGS+=	-DSL2TPS_VERSION=\"${VERSION}\"
CFLAGS+=	-DPREFIX=\"${PREFIX}\"
CFLAGS+=	-I${PREFIX}/include
CFLAGS+=	-Wall -Wbad-function-cast -Wcast-align -Wchar-subscripts \
		-Wformat -Winline -Wmissing-prototypes -Wnested-externs \
		-Wpointer-arith -Wunused -Wwrite-strings
CFLAGS+=	-pthread
CFLAGS+=	-g
#CFLAGS+=	-Werror

LDADD+=		-L${PREFIX}/lib -lpdel
LDADD+=		-lexpat -lssl -lcrypto -lcrypt -lradius -lnetgraph
DPADD+=		${PREFIX}/lib/libpdel.a

${PROG}.1:	${PROG}.1.in
	sed 's,@PREFIX@,${PREFIX},g' < ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	${PROG}.1

#
# Install example config file
#
beforeinstall:
	@if [ ! -d ${PREFIX}/etc/${PROG} ]; then \
		${ECHO} ${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0755 \
		    ${PREFIX}/etc/${PROG}; \
		${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0755 \
		    ${PREFIX}/etc/${PROG}; \
	fi
	${INSTALL} -C -o ${SHAREOWN} -g ${SHAREGRP} -m 0644 \
	    ${CONFIG_SAMPLE} ${PREFIX}/etc/${PROG}/${CONFIG_SAMPLE}

distclean:	cleandir

#
# Create distribution tarball, including substituted copyright notices
#
COPYRIGHT_TAG=	@COPYRIGHT@
COPYRIGHT_FILE=	${.CURDIR}/COPYRIGHT

tarball:	${COPYRIGHT_FILE}
	( cd ${.CURDIR} && ${MAKE} cleandir )
	rm -rf ${PROG}-${VERSION}
	mkdir ${PROG}-${VERSION}
	( cd ${.CURDIR} && find README *.[ch] *akefile* ${PROG}.1.in \
	    COPYRIGHT Changes ${CONFIG_SAMPLE} \( -name .svn -a -prune \) \
	    -o -type f -print | cpio -pudml ${PROG}-${VERSION} )
	FILES=`find ${PROG}-${VERSION} -type f -print`; \
	for f in $${FILES}; do \
	    if [ "$${f}" != "${PROG}-${VERSION}/Makefile" ] \
	      && grep -q '${COPYRIGHT_TAG}' $${f}; then \
		echo substituting '${COPYRIGHT_TAG}' in $${f}; \
		LINE=`grep -n '${COPYRIGHT_TAG}' $${f} \
		    | head -1 | awk -F: '{print $$1}'`; \
		TAIL=`grep -n '${COPYRIGHT_TAG}' $${f} \
		    | head -1 | awk -F: '{print $$2}' \
		    | sed -e 's/\\\\/\\\\\\\\/g' -e 's/"/\\\\"/g'`; \
		PREFX=`echo "$${TAIL}" \
		    | sed -E 's/^(.*)${COPYRIGHT_TAG}(.*)$$/\\1/g'`; \
		SUFFX=`echo "$${TAIL}" \
		    | sed -E 's/^(.*)${COPYRIGHT_TAG}(.*)$$/\\2/g'`; \
		if [ "$${LINE}" -gt 1 ]; then \
			head -`expr $${LINE} - 1` < $${f} > $${f}.new; \
		else \
			cat /dev/null > $${f}.new; \
		fi; \
		awk "{ printf \"%s%s%s\\n\", \
		    \"$${PREFX}\", \$$0, \"$${SUFFX}\" }" \
		    < ${COPYRIGHT_FILE} >> $${f}.new; \
		tail +`expr $${LINE} + 1` < $${f} >> $${f}.new; \
		mv $${f}.new $${f}; \
	    fi; \
	done
	tar cf - ${PROG}-${VERSION} | gzip --best \
	    > ${.CURDIR}/${PROG}-${VERSION}.tar.gz

CLEANFILES+=	${.CURDIR}/${PROG}-${VERSION}.tar.gz
CLEANDIRS+=	${.CURDIR}/${PROG}-${VERSION}

.include <bsd.prog.mk>
