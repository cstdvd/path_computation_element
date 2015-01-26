# Note: generated file

all:	realall

DOTDOT=
include GNUmakefile.inc

#
# Variables. Edit to suit.
#
srcdir = .
top_srcdir = .
#prefix = /usr/local
#exec_prefix = ${prefix}
#bindir = ${exec_prefix}/bin
#mandir = ${prefix}/man
#etcdir = ${prefix}/etc

#
# Below here should not need to be edited
#

ifeq ($(OS),MINGW32_NT-5.1)
HACKARG=	-include $(srcdir)/win32hacks.h
#CRYPTLIB=	-lcrypt
endif
ifeq ($(OS),CYGWIN_NT-5.1)
HACKARG=	-include $(srcdir)/cygwinhacks.h
CRYPTLIB=	-lcrypt
endif
ifeq ($(OS),Linux)
HACKARG=	-include $(srcdir)/linuxhacks.h
endif


#
# Below here should not need to be edited
#

PROG=		lws$(exe_sfx)
VERSION=	0.6.1
SRCS=		lws_config.c lws_server_subsystem.c lws_tmpl.c  lws_tmpl_auth.c lws_tmpl_config.c lws_tmpl_http.c  lws_tmpl_logs.c lws_tmpl_memstats.c lws_tmpl_misc.c  lws_tmpl_object.c lws_tmpl_passwd.c lws_tmpl_string.c  main.c
CFLAGS=		-O2 -fno-strict-aliasing -pipe -DLWS_SERVER_VERSION=\"0.2.1\" -DNEED_VA_UNDERSCORE=1 -DPREFIX=\"/usr/local\" -I/usr/local/include -Wall -Wbad-function-cast -Wcast-align -Wchar-subscripts -Wformat -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wunused -Wwrite-strings $(HACKARG) -DOPENSSL_NO_KRB5
MAN=		lws.1
SFILES=		default-config.xml docroot admin ca

#EXTRAINCS=	-I$(srcdir)/../pdel/ports/$(OS) 
#ifdef WIN32_PORT
#EXTRAINCS+=	-I$(srcdir)/../pdel/ports/WIN32
#endif
EXTRAINCS+=	$(OTHERINCDIRS)
# Temp Hack
#EXTRAINCS+=	-I$(srcdir)/../pdel
OTHERLIBDIRS+=	-L$(srcdir)/../pdel/$(objdir)

CFLAGS+=	-DNEED_VA_UNDERSCORE=1

prog=		$(objdir)/$(PROG)
objs=		$(SRCS:%.c=$(objdir)/%.o)
cman=		$(foreach m,$(MAN),$(m).gz)
files=		$(SFILES)

realall:	objdir $(prog) $(cman)

objdir:		$(objdir)

$(objdir):
	mkdir -p $(objdir)

vpath %.o $(objdir)
vpath %.So $(objdir)
vpath $(prog) $(objdir)

$(objdir):
	mkdir -p $(objdir)

%.gz:		%
	gzip < $+ > $@

$(objdir)/%.o:		%.c
	$(cc) -c $(OBJOUT) $(CFLAGS) $(DFLAGS) -I$(srcdir) $(EXTRAINCS) $+ $(PPOUT)

lws.1:		lws.1.in
	sed 's,@PREFIX@,${prefix},g' < $+ > $@

$(prog):	$(objs)
	$(cc) -o $@ $+ $(pdellink_static) $(CRYPTLIB)

clean:
	rm -rf $(objs) $(prog) $(cman) \
	    lws-$(VERSION) lws-$(VERSION).tar.gz lws.1

install:	install-prog install-manpages install-files

uninstall:	uninstall-prog uninstall-manpages uninstall-files

install-prog:	$(prog)
	$(install) -c $(install_man_own) -m 755 \
	    $(prog) $(DESTDIR)$(bindir)/$(PROG)

uninstall-prog:
	rm -f $(DESTDIR)$(bindir)/$(prog)

install-manpages:	$(cman)
	@for p in $(MAN); do \
		section=`echo $$p | sed 's/^[^.]*.//g'`; \
		f=`basename $$p`; \
		if [ ! -d $(DESTDIR)$(mandir)/man$$section ]; then \
			echo $(install) -d  $(install_man_own) -m 755 \
			    $(DESTDIR)$(mandir)/man$$section; \
			$(install) -d $(install_man_own) -m 755 \
			    $(DESTDIR)$(mandir)/man$$section; \
		fi; \
		echo $(install) -c $(install_man_own) -m 444 \
		    $$p.gz $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
		$(install) -c $(install_man_own) -m 444 \
		    $$p.gz $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
	done

uninstall-manpages:
	@for p in $(MAN); do \
		section=`echo $$p | sed 's/^[^.]*.//g'`; \
		f=`basename $$p`; \
		echo rm -f $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
		rm -f $(DESTDIR)$(mandir)/man$$section/$$f.gz; \
	done

install-files:	$(files)
	@if [ ! -d $(DESTDIR)$(etcdir) ]; then \
		echo $(install) -d $(install_share_own) -m 755 \
		    $(DESTDIR)$(etcdir); \
		$(install) -d $(install_share_own) -m 755 \
		    $(DESTDIR)$(etcdir); \
	fi
	@if [ ! -d $(DESTDIR)$(etcdir)/lws ]; then \
		echo $(install) -d $(install_share_own) -m 755 \
		    $(DESTDIR)$(etcdir)/lws; \
		$(install) -d $(install_share_own) -m 755 \
		    $(DESTDIR)$(etcdir)/lws; \
	fi
	cp -R $+ $(DESTDIR)$(etcdir)/lws
	$(chown) -R $(shareown):$(sharegroup) $(DESTDIR)$(etcdir)/lws
	find $(DESTDIR)$(etcdir)/lws -type d | xargs chmod 0755
	find $(DESTDIR)$(etcdir)/lws -type f | xargs chmod 0644
	( cd $(DESTDIR)$(etcdir)/lws/ca && sh check_cert \
	    ../bogus-ssl.key ../bogus-ssl.crt `hostname` )
	chmod 0600 $(DESTDIR)$(etcdir)/lws/bogus-ssl.key
	chmod 0600 $(DESTDIR)$(etcdir)/lws/ca/ca.key

uninstall-files:
	@for p in $(files); do \
		echo rm -rf $(DESTDIR)$(etcdir)/lws/$$p; \
		rm -rf $(DESTDIR)$(etcdir)/lws/$$p; \
	done

