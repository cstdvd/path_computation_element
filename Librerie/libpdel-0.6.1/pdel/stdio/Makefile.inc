#	@(#)Makefile.inc	8.3 (Berkeley) 4/17/94
# $FreeBSD: src/lib/libc/stdio/Makefile.inc,v 1.37 2006/04/22 15:09:15 deischen Exp $

# stdio sources


STDIO_SRCS=	_flock_stub.c clrerr.c fclose.c fcloseall.c \
	feof.c ferror.c fflush.c fgetc.c fgetln.c fgetpos.c fgets.c \
	findfp.c flags.c fprintf.c fpurge.c fputc.c fputs.c \
	fread.c fscanf.c fseek.c fsetpos.c \
	ftell.c funopen.c fvwrite.c fwalk.c \
	fwrite.c getc.c \
	getw.c makebuf.c \
	putc.c putw.c \
	refill.c rewind.c rget.c setbuf.c setbuffer.c \
	setvbuf.c stdio.c \
	ungetc.c unlocked.c vfprintf.c \
	vfscanf.c \
	wbuf.c \
	wsetup.c \
	pd_glue.c

STDIO_SRCDIR=	stdio


