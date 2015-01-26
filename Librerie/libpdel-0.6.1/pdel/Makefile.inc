# $Id: Makefile.inc 907 2004-06-03 21:15:58Z archie $

PREFIX?=	/usr/local

CFLAGS+=	-O2

CFLAGS+=	-Wall \
		-Wcast-align \
		-Wchar-subscripts \
		-Wcomment \
		-Wformat \
		-Wimplicit \
		-Wmissing-declarations \
		-Wmissing-prototypes \
		-Wnested-externs \
		-Wno-long-long \
		-Wparentheses \
		-Wpointer-arith \
		-Wreturn-type \
		-Wswitch \
		-Wtrigraphs \
		-Wuninitialized \
		-Wunused \
		-Wwrite-strings

#CFLAGS+=	-Waggregate-return
#CFLAGS+=	-Wcast-qual
#CFLAGS+=	-Werror
#CFLAGS+=	-Wredundant-decls
#CFLAGS+=	-Wshadow
#CFLAGS+=	-Wstrict-prototypes

CFLAGS+=	-g

