# $Id: Makefile.inc 153 2002-04-08 23:20:18Z archie $

tmpl_PATH+=	${PDELTOP}/tmpl

tmpl_SRCS+=	tmpl.c \
		tmpl_ctx.c \
		tmpl_exec.c \
		tmpl_mem.c \
		tmpl_parse.c \
		tmpl_vars.c

tmpl_INCS+=	tmpl.h

tmpl_MAN+=	tmpl.3
tmpl_MLINKS+=	tmpl.3 tmpl_create.3
tmpl_MLINKS+=	tmpl.3 tmpl_ctx_create.3
tmpl_MLINKS+=	tmpl.3 tmpl_ctx_destroy.3
tmpl_MLINKS+=	tmpl.3 tmpl_ctx_reset.3
tmpl_MLINKS+=	tmpl.3 tmpl_destroy.3
tmpl_MLINKS+=	tmpl.3 tmpl_execute.3
tmpl_MLINKS+=	tmpl.3 tmpl_execute_func.3

