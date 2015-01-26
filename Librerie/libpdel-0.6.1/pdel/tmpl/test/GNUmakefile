# $Id: GNUmakefile 1002 2006-11-29 16:01:57Z mgooderum $

DOTDOT=../../
include GNUmakefile.inc

srcs=		main.c
objs=		$(srcs:.c=.o)
prog=		tmpl
CFLAGS=		-I$(prefix)/include -Wall -DOPENSSL_NO_KRB5
cc=		cc

$(prog):	$(objs)
	$(cc) -o $@ $(objs) $(DFLAGS) $(pdellink)

%.o:		%.c
	$(cc) -c -o $@ $(CFLAGS) $(DFLAGS) $+

test:
	rm -f test.out
	make test.out
	make test-input

test-input:
	$(prog) test-input.in

test.out:	$(prog) test.in
	$(prog) -n test.in > $@
	@if ! diff -q $@.sav $@; then \
		echo ERROR: Test output does not match saved output; \
		echo ERROR: The difference is saved in test.diff; \
		diff -u $@.sav $@ > test.diff; \
		exit 1; \
	fi

clean:
	rm -f $(prog) $(objs)
	rm -f test.out test.diff

