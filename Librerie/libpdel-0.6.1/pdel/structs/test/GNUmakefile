# $Id: GNUmakefile 1008 2006-12-05 01:48:31Z mgooderum $

DOTDOT=../../
include ${DOTDOT}GNUmakefile.inc

srcs=		main.c
objs=		$(srcs:.c=.o)
prog=		structs_test
CFLAGS=		-I$(prefix)/include -Wall -DNO_BPF -DOPENSSL_NO_KRB5
cc=		cc


$(prog):	$(objs)
	$(cc) -o $@ $(objs) $(DFLAGS)  $(DFLAGS) $(pdellink_static)

%.o:		%.c
	$(cc) -c -o $@ $(CFLAGS) $(DFLAGS) $+

# test target
test:
	grep -v bpf < structs_test.xml | ./structs_test -ncx > structs_test.nrm
	grep -v bpf < structs_test.xml | ./structs_test -ecx > structs_test.bin
	./structs_test -dx < structs_test.bin | diff -u structs_test.nrm -
	grep -v bpf < structs_test.xml | ./structs_test -tcx \
		> structs_test.xmlrpc
	./structs_test -Tcx < structs_test.xmlrpc | diff -u structs_test.nrm -
	grep -v bpf < structs_test.xml | ./structs_test -c

clean:
	rm -f $(prog) $(objs)
	rm -f structs_test.bin structs_test.nrm structs_test.xmlrpc

