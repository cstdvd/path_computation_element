#
# Common util/setup for GNU makefiles
#

ifdef MSVC
OS=		MSVC-WIN32
ARCH=	i686
endif

ifndef OS
OS=		$(shell uname -s)
endif
ifndef ARCH
ARCH=	$(shell uname -m)
endif

#
# Common Variables. Edit to suit.
#
srcdir = .
top_srcdir = ./${DOTDOT}

prefix = /usr/local
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib
slibdir = ${exec_prefix}/lib
bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
mandir = ${prefix}/man
includedir = ${prefix}/include/pdel
etcdir = ${prefix}/etc
cc= gcc

OS=		$(shell uname -s)
ARCH=		$(shell uname -m)
shlib_major=	$(shell egrep PDEL_SHLIB_MAJOR version.h | awk '{print $$3}')
shlib_minor=	$(shell egrep PDEL_SHLIB_MINOR version.h | awk '{print $$3}')

VERSION_MAJOR=	$(shell egrep PDEL_LIB_MAJOR version.h | awk '{print $$3}')
VERSION_MINOR=	$(shell egrep PDEL_LIB_MINOR version.h | awk '{print $$3}')
VERSION_PATCH=	$(shell egrep PDEL_LIB_PATCH version.h | awk '{print $$3}')

objdir=		obj/$(OS).$(ARCH)

ar = /usr/bin/ar
ranlib = ranlib
install = /usr/bin/install
chown=  chown
user=	$(shell uname)
ifeq (root,$(uname))
no_chown=
else
no_chown=1
endif

binown = root
bingroup = 0

libown = root
libgroup = 0

manown = root
mangroup = 0

includeown = root
includegroup = 0

shareown = root
sharegroup = 0

exe_sfx=

MISSING_FUNOPEN=
NEED_REGEX=

ssl_libbase=	ssl
crypto_libbase= crypto
expat_libbase=	expat
pthread_libbase=   pthread
WIN32_PORT=

#
# Port specific Overrides
#
ifeq ($(OS),FreeBSD)
BSD_PORT=1        
endif
ifeq ($(OS),NetBSD)
BSD_PORT=1        
endif
ifeq ($(OS),OpenBSD)
BSD_PORT=1        
endif

ifeq ($(OS),CYGWIN_NT-6.0)

# Normalize Vista back to XP version for now
OS=CYGWIN_NT-5.1

endif

ifeq ($(OS),CYGWIN_NT-5.1)

prefix=		/usr
libdir=		/lib
bindir=		/usr/local/bin
slibdir=	/bin
etcdir =	/usr/local/etc
exe_sfx=	.exe

MISSING_FUNOPEN=1
NEED_REGEX=1
no_chown=1

compsfx=GC

endif

ifeq ($(OS),MINGW32_NT-6.0)

# Normalize Vista back to XP version for now
OS=MINGW32_NT-5.1

endif

ifeq ($(OS),MINGW32_NT-5.1)
WIN32_PORT=1

bindir=		/mingw/bin
libdir=		/mingw/lib
slibdir=	/mingw/bin
ar=         /mingw/bin/ar

compsfx=GC

endif

ifeq ($(OS),MSVC-WIN32)

WIN32_PORT=1
bindir=		/mingw/bin
libdir=		/mingw/lib
ar =		/mingw/bin/ar
chown=		echo
compsfx=VC
cc= cc

endif

ifeq ($(OS),Linux)
otherlibs+=     -l$(pthread_libbase)
# Missing REG_STARTEND
NEED_REGEX=1
crypt_libbase= crypt
otherlibs+=     -l$(crypt_libbase)
endif

#
# Common Multi-port overrides
# 

ifdef BSD_PORT
radius_libbase=    radius
netgraph_libbase=  netgraph
otherlibs+=     -l$(radius_libbase)
otherlibs+=     -l$(netgraph_libbase)
otherlibs+=     -l$(pthread_libbase)
endif


ifdef WIN32_PORT
exe_sfx=	.exe
otherlibs+=	-lws2_32	# Need Winsock2, not Winsock 1
otherlibs+=	-lmsvcp60	# C runtime with mb chars

ssl_libbase=	ssleay32
crypto_libbase= libeay32

OTHERINCDIRS+=	-I/c/OpenSSL/include
OTHERINCDIRS+=	-I$(expat_incdir)

# Point to pthread-win32 libs/includes  
OTHERINCDIRS+=	-I../../pthread-win32/include
OTHERLIBDIRS+=	-L../../pthread-win32/lib
OTHERINCDIRS+=	-I../../pthread-win32/Pre-built.2/include
OTHERLIBDIRS+=	-L../../pthread-win32/Pre-built.2/lib

ifeq ($(compsfx),GC)
expat_implib=	$(objdir)/lib$(expat_libbase).a
expat_implibw=	$(objdir)/lib$(expat_libbase)w.a
expat_implibs=	$(expat_implib) $(expat_implibw)

openssl_implib=	$(objdir)/lib$(ssl_libbase).a
crypt_implib=	$(objdir)/lib$(crypto_libbase).a
openssl_implibs= $(openssl_implib) $(crypt_implib)
openssl_libdir=	/c/OpenSSL/lib/MinGW
endif

ifeq ($(compsfx),VC)
expat_implib=	$(expat_libdir)/lib$(expat_libbase).lib
expat_implibw=	$(expat_libdir)/lib$(expat_libbase)w.lib
OTHERLIBDIRS+=	-L/c/OpenSSL/lib/VC
endif

expat_libdir=	/c/Expat-2.0.0/Libs
expat_incdir=	/c/Expat-2.0.0/Source/lib
OTHERLIBDIRS+=	-L$(expat_libdir)
otherlibs+=     -lpthread$(compsfx)2

MISSING_FUNOPEN=1
NEED_REGEX=1

no_chown=1

endif


#
# Common bits that have to come after port specific overrides
#


ifndef no_chown

install_bin_own= -o $(binown) -g $(bingroup)
install_lib_own= -o $(libown) -g $(libgroup)
install_include_own= -o $(includeown) -g $(includegroup)
install_man_own= -o $(manown) -g $(mangroup)
install_share_own= -o $(shareown) -g $(sharegroup)

else

ifndef USER
USER=	${USERNAME}

endif

binown =  ${USER}
libown =  ${USER}
manown =  ${USER}
includeown =  ${USER}
shareown =  ${USER}

endif


#
# Helpers for program makefiles
#
ifdef WIN32_PORT

chown=		echo
pdel_libbase_dynamic=	pdel$(compsfx)50
#pdel_libbase_static=	pdel$(compsfx)
pdel_libbase_static=	pdel
pdel_libbase=           ${pdel_libbase_static}

else
pdel_libbase=           pdel
pdel_libbase_dynamic=	$(pdel_libbase)
pdel_libbase_static=    $(pdel_libbase)
endif

#
# Common Compiler support/help
#
ifdef DEBUG
DFLAGS=	-g -O0
endif
ifdef PREPROC
DFLAGS+= -E -C
PPOUT=	> $@.E
OBJOUT=
else
OBJOUT= -o $@
endif

pdellibs_dynamic=-l$(pdel_libbase_dynamic) -l$(ssl_libbase) -l$(crypto_libbase)  -l$(expat_libbase) $(otherlibs)

pdellibs_static=-l$(pdel_libbase_static) -l$(ssl_libbase) -l$(crypto_libbase)  -l$(expat_libbase) $(otherlibs)

pdellink_static=-L$(objdir) $(OTHERLIBDIRS) -L$(libdir) $(pdellibs_static)
pdellink_dynamic=-L$(objdir) $(OTHERLIBDIRS) -L$(libdir) $(pdellibs_dynamic)

pdellink= ${pdellink_static}
