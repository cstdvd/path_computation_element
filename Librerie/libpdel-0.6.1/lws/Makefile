# $Id: Makefile 1072 2007-05-09 21:35:36Z mgooderum $

PROG=		lws
VERSION=	0.6.1
SRCS=		lws_config.c lws_server_subsystem.c lws_tmpl.c \
		lws_tmpl_auth.c lws_tmpl_config.c lws_tmpl_http.c \
		lws_tmpl_logs.c lws_tmpl_memstats.c lws_tmpl_misc.c \
		lws_tmpl_object.c lws_tmpl_passwd.c lws_tmpl_string.c \
		main.c

PREFIX?=	/usr/local
LWSTOP=		${.CURDIR}
BINDIR=		${PREFIX}/bin
OBJDIR=		${LWSTOP}/obj/${OS}.${ARCH}
.OBJDIR=	${OBJDIR}
MANDIR=		${PREFIX}/man/man
SFILES=		default-config.xml docroot admin ca
MAN1=		lws.1

CFLAGS+=	-DLWS_SERVER_VERSION=\"${VERSION}\"
CFLAGS+=	-DNEED_VA_UNDERSCORE=1
CFLAGS+=	-DPREFIX=\"${PREFIX}\"
CFLAGS+=	-I${PREFIX}/include
CFLAGS+=	-Wall -Wbad-function-cast -Wcast-align -Wchar-subscripts \
		-Wformat -Winline -Wmissing-prototypes -Wnested-externs \
		-Wpointer-arith -Wunused -Wwrite-strings
CFLAGS+=	-pthread
#CFLAGS+=	-g -static

LDADD+=		-L${PREFIX}/lib -lpdel
LDADD+=		-lexpat -lssl -lcrypto -lcrypt -lradius -lnetgraph
DPADD+=		${PREFIX}/lib/libpdel.a

lws.1:		lws.1.in
	sed 's,@PREFIX@,${PREFIX},g' < ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	lws.1

#
# Install template files and create SSL key and cert
#
beforeinstall:
	@if [ ! -d ${PREFIX}/etc/lws ]; then \
		${ECHO} ${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0755 \
		    ${PREFIX}/etc/lws; \
		${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0755 \
		    ${PREFIX}/etc/lws; \
	fi
	( cd ${.CURDIR} ; cp -R ${SFILES} ${PREFIX}/etc/lws )
	chown -R ${SHAREOWN}:${SHAREGRP} ${PREFIX}/etc/lws
	find ${PREFIX}/etc/lws -type d | xargs chmod 0755
	find ${PREFIX}/etc/lws -type f | xargs chmod 0644
	( cd ${PREFIX}/etc/lws/ca && sh check_cert \
	    ../bogus-ssl.key ../bogus-ssl.crt `hostname` )
	chmod 0600 ${PREFIX}/etc/lws/bogus-ssl.key
	chmod 0600 ${PREFIX}/etc/lws/ca/ca.key

distclean:	cleandir

#
# Create distribution tarball, including substituted copyright notices
#
COPYRIGHT_TAG=	@COPYRIGHT@
COPYRIGHT_FILE=	${.CURDIR}/COPYRIGHT

tarball:	${COPYRIGHT_FILE}
	( cd ${.CURDIR} && ${MAKE} cleandir )
	rm -rf lws-${VERSION}
	mkdir lws-${VERSION}
	( cd ${.CURDIR} && find README *.[ch] *akefile* lws.1.in ca \
	    COPYRIGHT Changes ${SFILES} \( -name .svn -a -prune \) \
	    -o -type f -print | cpio -pudml ${OBJDIR}/lws-${VERSION} )
	FILES=`find ${OBJDIR}/lws-${VERSION} -type f -print`; \
	for f in $${FILES}; do \
	    if [ "$${f}" != "${OBJDIR}/lws-${VERSION}/Makefile" ] \
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
	( cd ${OBJDIR} && tar cf - lws-${VERSION} ) | gzip --best > ${.CURDIR}/lws-${VERSION}.tar.gz

CLEANFILES+=	${.CURDIR}/lws-${VERSION}.tar.gz
CLEANDIRS+=	${.CURDIR}/lws-${VERSION}

#
# Create a GNUmakefile for Linux
#

L_PROG=		${PROG}
L_SRCS=		${SRCS}
L_MAN=		${MAN}
L_CFLAGS=	${CFLAGS:N-pthread:S,",\\",g}
L_SFILES=	${SFILES}
L_VERSION=	${VERSION}

makefiles: ${.CURDIR}/GNUmakefile

${.CURDIR}/GNUmakefile:	${.CURDIR}/Makefile ${.CURDIR}/GNUmakefile.in
	sed -e 's,\$$\I\d: .*$$,Note: generated file,g' \
	    -e 's,@@PROG@,${L_PROG},g' \
	    -e 's,@@VERSION@,${L_VERSION},g' \
	    -e 's,@@SRCS@,${L_SRCS},g' \
	    -e 's,@@CFLAGS@,${L_CFLAGS},g' \
	    -e 's,@@MAN@,${L_MAN},g' \
	    -e 's,@@SFILES@,${L_SFILES},g' \
	    < ${.CURDIR}/GNUmakefile.in > ${.TARGET}

.include <bsd.prog.mk>
