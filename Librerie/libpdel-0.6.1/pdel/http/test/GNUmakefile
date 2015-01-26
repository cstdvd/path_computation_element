# $Id: GNUmakefile 1002 2006-11-29 16:01:57Z mgooderum $

DOTDOT=../../
include GNUmakefile.inc

srcs=		main.c
objs=		$(srcs:.c=.o)
sslfiles=	demo.key demo.crt
prog=		demo_server
CFLAGS=		-I$(prefix)/include -DOPENSSL_NO_KRB5 -Wall
cc=		cc

$(prog):	$(objs) $(sslfiles)
	$(cc) -o $@ $(objs) -L$(prefix)/lib $(DFLAGS) $(pdellink)


%.o:		%.c
	$(cc) -c -o $@ $(CFLAGS) $(DFLAGS) $+

$(sslfiles):
	cd cert && make

clean:
	rm -f $(sslfiles) $(prog) $(objs)
	cd cert && make clean

