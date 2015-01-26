
DOTDOT=../../
include ${DOTDOT}GNUmakefile.inc

pda_bins=	pdalltest_pub pdalltest_priv

all:	${pda_bins}


CFLAGS=		-O2 -fno-strict-aliasing -pipe -I$(top_srcdir) -I$(prefix)/include -DTYPED_MEM_UNDEFINE_ORIGINALS -DBUILDING_PDEL=1  -O2 -Wall -Wcast-align -Wchar-subscripts -Wcomment -Wformat -Wimplicit -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wno-long-long -Wparentheses -Wpointer-arith -Wreturn-type -Wswitch -Wtrigraphs -Wuninitialized -Wunused -Wwrite-strings -g  -DOPENSSL_NO_KRB5

%:	%.o
	$(cc) -o $@ $(objs) $(CFLAGS) $(DFLAGS) $+ $(pdellink_static)

%_pub.o:	%.c
	$(cc) -c -o $@ $(CFLAGS) $(DFLAGS)  -UBUILDING_PDEL $+

%_priv.o:	%.c
	$(cc) -c -o $@ $(CFLAGS) $(DFLAGS) -DBUILDING_PDEL=1 $+

clean:
	rm -f ${pda_bins} *.o