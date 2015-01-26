# $Id: Makefile 889 2004-06-02 17:01:55Z archie $

PDIR=		pdel-port

VERSION!=	cd ${.CURDIR}/.. && ${MAKE} version

MAKEFILE=	${PDIR}/Makefile
PLIST=		${PDIR}/pkg-plist
DISTINFO=	${PDIR}/distinfo
TARBALL=	${.CURDIR}/../libpdel-${VERSION}.tar.gz
PBALL=		${.CURDIR}/pdel-port.tgz

${PBALL}:	${TARBALL} ${.CURDIR}/../libpdel.3
		rm -rf ${.TARGET} ${PDIR}
		mkdir -p ${PDIR}
		( cd ${.CURDIR}/.. && ${MAKE} filelist ) > ${PLIST}
		( cd ${.CURDIR}/.. && ${MAKE} dirlist ) \
		    | sort -r | sed 's/^/@dirrm /g' >> ${PLIST}
		cp ${.CURDIR}/pkg-descr ${PDIR}
		( cd ${.CURDIR}/.. && md5 `basename ${TARBALL}` ) > ${DISTINFO}
		( \
		  SIZE=`wc -c < ${TARBALL} | awk '{ print $$1 }'`; \
		  echo "SIZE (`basename ${TARBALL}`) = $${SIZE}"; \
		) >> ${DISTINFO}
		sed "s/@VERSION@/${VERSION}/g" < Makefile.in > ${MAKEFILE}
		cd ${.CURDIR} && tar cvf - ${PDIR} | gzip --best > ${.TARGET}
		rm -rf ${PDIR}

${TARBALL}:
		@cd ${.CURDIR}/.. && ${MAKE} tarball

clean cleandir:
		rm -f ${PBALL}

