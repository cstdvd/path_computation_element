# $Id: Makefile 1072 2007-05-09 21:35:36Z mgooderum $

LIB=		pdel

SUBDIRS=	config http io misc net pdel ppp regex structs sys thread tmpl util

VERSION!=	grep -w PDEL_VERSION_STRING ${.CURDIR}/version.h \
		    | awk '{ print $$NF }' | sed 's/"//g'
VERSION_MAJOR=	${VERSION:C/([0-9]+)\.([0-9]+)\.([0-9]+)/\1/g}
VERSION_MINOR=	${VERSION:C/([0-9]+)\.([0-9]+)\.([0-9]+)/\2/g}
VERSION_PATCH=	${VERSION:C/([0-9]+)\.([0-9]+)\.([0-9]+)/\3/g}

SHLIB_MAJOR!=	grep -w PDEL_SHLIB_MAJOR  ${.CURDIR}/version.h \
			| awk '{ print $3 }'
SHLIB_MINOR!=	grep -w PDEL_SHLIB_MINOR  ${.CURDIR}/version.h \
			| awk '{ print $3 }'

OS!=		uname -s
ARCH!=		uname -m

PREFIX?=	/usr/local
PDELTOP=	${.CURDIR}
LIBSUFX=	lib
LIBDIR=		${PREFIX}/${LIBSUFX}
SHLIBDIR=	${PREFIX}/${LIBSUFX}
MANSUFX=	man/man
MANDIR=		${PREFIX}/${MANSUFX}
INCSUFX=	include/pdel
INCDIR=		${PREFIX}/${INCSUFX}
OBJDIR=		${PDELTOP}/obj/${OS}.${ARCH}
.OBJDIR=	${OBJDIR}
SHLIB_MAJOR=	${VERSION_MAJOR}
SHLIB_MAJOR=	${VERSION_MINOR}
MAN=		libpdel.3
NO_PROFILE=
INCHDRS=	version.h
BASEINCHDRS=	

# Uncomment to get debug symbols
#CFLAGS+=-g
#STRIP=

#
# Read in info from subdirectories
#
.for DIR in ${SUBDIRS}
.include "${DIR}/Makefile.inc"
.for PDIR in ${${DIR}_PATH}
.PATH:		${PDIR}
SRCDIRS+=	${PDIR}
.endfor
SRCS+=		${${DIR}_SRCS}
INCHDRS+=	${${DIR}_INCS:S/^/${DIR}\//g}
BASEINCHDRS+=	${${DIR}_BASEINCS}
MAN+=		${${DIR}_MAN:S/^/${DIR}\//g}
MLINKS+=	${${DIR}_MLINKS}
.endfor

CFLAGS+=	-I${PDELTOP}
CFLAGS+=	-I${PREFIX}/include
CFLAGS+=	-DTYPED_MEM_UNDEFINE_ORIGINALS
CFLAGS+=	-DBUILDING_PDEL=1

CLEANFILES+=	${.CURDIR}/freebsd.port/pdel-port.tgz

#
# Cleaning up
#
extra_clean:	cleandir
	( cd ${.CURDIR} && find -E ${.CURDIR} \
	    -regex '.*/(tags|.*\.orig|xx)' | xargs rm )
	@if [ -d ${.CURDIR}/freebsd.port ]; then \
		cd ${.CURDIR}/freebsd.port && ${MAKE} cleandir; \
	fi
	@for DIR in ${SUBDIRS}; do \
		TDIRS=`find ${.CURDIR}/$${DIR} -type d -a -name '*test*'`; \
		for TDIR in $${TDIRS}; do \
			( cd $${TDIR} && ${MAKE} cleandir ); \
		done; \
	done

.if !defined(EXTRA_CLEAN)
clean cleandir distclean:
	cd ${.CURDIR} && ${MAKE} extra_clean EXTRA_CLEAN=
.endif

#
# Handle installation of #include files
# 
beforeinstall:	includedirs includefiles

#
# Create #include subdirectories
#
includedirs:
	@INCDIRS=`echo ${INCHDRS} | fmt 2 | grep / \
	    | sed -E 's,^(.*)/[^/]+$$,\1,g' | sort | uniq`; \
	for DIR in $${INCDIRS}; do \
		if ! [ -d ${DESTDIR}${INCDIR}$${DIR} ]; then \
			echo ${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 0755 \
			    ${DESTDIR}${INCDIR}/$${DIR}; \
			${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 0755 \
			    ${DESTDIR}${INCDIR}/$${DIR}; \
		fi; \
	done

#
# Install header files
#
includefiles:
	@for HDR in ${INCHDRS}; do \
		echo ${INSTALL} -C -o ${SHAREOWN} -g ${SHAREGRP} -m \
		    ${SHAREMODE} ${PDELTOP}/$${HDR} \
		    ${DESTDIR}${INCDIR}/$${HDR}; \
		${INSTALL} -C -o ${SHAREOWN} -g ${SHAREGRP} -m \
		    ${SHAREMODE} ${PDELTOP}/$${HDR} \
		    ${DESTDIR}${INCDIR}/$${HDR}; \
	done
	@for HDR in ${BASEINCHDRS}; do \
		echo ${INSTALL} -C -o ${SHAREOWN} -g ${SHAREGRP} -m \
		    ${SHAREMODE} ${PDELTOP}/pdel/$${HDR} \
		    ${DESTDIR}${INCDIR}/$${HDR}; \
		${INSTALL} -C -o ${SHAREOWN} -g ${SHAREGRP} -m \
		    ${SHAREMODE} ${PDELTOP}/pdel/$${HDR} \
		    ${DESTDIR}${INCDIR}/$${HDR}; \
	done

#
# Print out all created directories
#
dirlist:
	@INCDIRS=`echo ${INCHDRS} | fmt 2 | grep / \
	    | sed -E 's,^(.*)/[^/]+$$,\1,g' | sort | uniq`; \
	for DIR in $${INCDIRS}; do \
		echo ${INCSUFX}/$${DIR}; \
	done; \
	echo ${INCSUFX}

#
# Print out all installed files
#
filelist:
	@( cd ${.CURDIR} && ${MAKE} filelist-unsorted | sort )

filelist-unsorted:
	@echo ${LIBSUFX}/libpdel.a
	@echo ${LIBSUFX}/libpdel.so
	@echo ${LIBSUFX}/libpdel.so.${SHLIB_MAJOR}
	@for HDR in ${INCHDRS}; do \
		echo ${INCSUFX}/$${HDR}; \
	done
	@set `echo ${MAN} " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
	while : ; do \
		case $$# in \
			0) break;; \
			1) echo "warn: missing extension: $$1"; break;; \
		esac; \
		page=`basename $$1`; shift; sect=$$1; shift; \
		${ECHO} ${MANSUFX}$${sect}/$${page}.gz; \
	done
	@set `echo ${MLINKS} " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
	while : ; do \
		case $$# in \
			0) break;; \
			[123]) echo "warn: empty MLINK: $$1 $$2 $$3"; break;; \
		esac; \
		shift; shift; page=`basename $$1`; shift; sect=$$1; shift; \
		${ECHO} ${MANSUFX}$${sect}/$${page}.gz; \
	done

#
# Create distribution tarball, including substituted copyright notices
#
COPYRIGHT_TAG=	@COPYRIGHT@
COPYRIGHT_FILE=	${.CURDIR}/COPYRIGHT

tarball:	${COPYRIGHT_FILE}
	( cd ${.CURDIR} && ${MAKE} cleandir )
	rm -f ${.CURDIR}/GNUmakefile
	( cd ${.CURDIR} && ${MAKE} ${.CURDIR}/GNUmakefile )
	rm -rf libpdel-${VERSION}
	mkdir libpdel-${VERSION}
	( cd ${.CURDIR} && find COPYRIGHT README* Changes *akefile* libpdel.3 \
	    ports private version.h ${SUBDIRS} \( -name .svn -a -prune \) \
	    -o -type f -print | cpio -pudml ${OBJDIR}/libpdel-${VERSION} )
	FILES=`find ${OBJDIR}/libpdel-${VERSION} -type f -print`; \
	for f in $${FILES}; do \
	    if [ "$${f}" != "${OBJDIR}/libpdel-${VERSION}/Makefile" ] \
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
	( cd ${OBJDIR} && tar cf - libpdel-${VERSION} ) \
	    | gzip --best > ${.CURDIR}/libpdel-${VERSION}.tar.gz

CLEANFILES+=	${.CURDIR}/libpdel-${VERSION}.tar.gz
CLEANDIRS+=	${OBJDIR}/libpdel-${VERSION}

showgnu:
	echo "GNUMakefile Target is  ${.CURDIR}/GNUmakefile"

makefiles:
	${MAKE} ${.CURDIR}/GNUmakefile

.if make(${.CURDIR}/GNUmakefile)
#
# Create a GNUmakefile for Linux
#
L_OMIT=		(bpf|fs_mount|if_arp|if_iface|if_ip|if_util|route_msg|uroute|ppp_|pdel_ppp)

L_SRCS!=	echo ${SRCS} | fmt -w 1 | grep -vE '${L_OMIT}'
L_MAN!=		echo ${MAN} | fmt -w 1 | grep -vE '${L_OMIT}'
L_HDRS!=	echo ${HDRS} | fmt -w 1 | grep -vE '${L_OMIT}'
L_INCS!=	echo ${INCHDRS} | fmt -w 1 | grep -vE '${L_OMIT}'
L_BASEINCS!=	echo ${BASEINCHDRS} | fmt -w 1 | grep -vE '${L_OMIT}'
L_MLINKS!=	set `echo ${MLINKS}`; \
		while : ; do \
			case $$\# in \
				0) break;; \
				1) echo "warn: empty MLINK: $$1"; break;; \
			esac; \
			if ! echo $$1 $$2 | grep -qE "${L_OMIT}"; then \
			    echo $$1 $$2; \
			fi; \
			shift; shift; \
		done
L_CFLAGS=	${CFLAGS:S/${PDELTOP}/\$(srcdir)/g}
L_SRCDIRS=	${SRCDIRS:S/${PDELTOP}/\$(srcdir)/g:S/-DTYPED_MEM_UNDEFINE_ORIGINALS//g}

${.CURDIR}/GNUmakefile:	${.CURDIR}/Makefile ${.CURDIR}/GNUmakefile.in
	@echo creating ${.TARGET}
	@sed -e 's,\$$\I\d: .*$$,Note: generated file,g' \
	    -e 's,@@SRCS@,${L_SRCS},g' \
	    -e 's,@@SRCDIRS@,${L_SRCDIRS},g' \
	    -e 's,@@INCS@,${L_INCS},g' \
	    -e 's,@@BASEINCS@,${L_BASEINCS},g' \
	    -e 's,@@MAN@,${L_MAN},g' \
	    -e 's,@@MLINKS@,${L_MLINKS},g' \
	    -e 's,@@CFLAGS@,${L_CFLAGS},g' \
	    -e 's,@@SUBDIRS@,${L_SUBDIRS},g' \
	    < ${.CURDIR}/GNUmakefile.in > ${.TARGET}
.endif

#
# Print out version
#
version:
	@echo ${VERSION}

.include "${PDELTOP}/Makefile.inc"
.include <bsd.lib.mk>
