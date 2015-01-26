# $Id: Makefile.inc 1022 2006-12-08 06:13:40Z mgooderum $

util_PATH+=	${PDELTOP}/util

util_SRCS+=	ghash.c \
		gtree.c \
		mesg_port.c \
		paction.c \
		pevent.c \
		rsa_util.c \
		string_quote.c \
		tinfo.c \
		typed_mem.c

util_INCS+=	ghash.h \
		gtree.h \
		mesg_port.h \
		paction.h \
		pevent.h \
		rsa_util.h \
		string_quote.h \
		tinfo.h \
		typed_mem.h

util_MAN+=	ghash.3
util_MLINKS+=	ghash.3 ghash_arg.3
util_MLINKS+=	ghash.3 ghash_create.3
util_MLINKS+=	ghash.3 ghash_destroy.3
util_MLINKS+=	ghash.3 ghash_dump.3
util_MLINKS+=	ghash.3 ghash_get.3
util_MLINKS+=	ghash.3 ghash_iter_create.3
util_MLINKS+=	ghash.3 ghash_iter_destroy.3
util_MLINKS+=	ghash.3 ghash_iter_has_next.3
util_MLINKS+=	ghash.3 ghash_iter_next.3
util_MLINKS+=	ghash.3 ghash_iter_remove.3
util_MLINKS+=	ghash.3 ghash_put.3
util_MLINKS+=	ghash.3 ghash_remove.3
util_MLINKS+=	ghash.3 ghash_size.3
util_MLINKS+=	ghash.3 ghash_walk_init.3
util_MLINKS+=	ghash.3 ghash_walk_next.3

util_MAN+=	gtree.3
util_MLINKS+=	gtree.3 gtree_arg.3
util_MLINKS+=	gtree.3 gtree_create.3
util_MLINKS+=	gtree.3 gtree_destroy.3
util_MLINKS+=	gtree.3 gtree_dump.3
util_MLINKS+=	gtree.3 gtree_first.3
util_MLINKS+=	gtree.3 gtree_get.3
util_MLINKS+=	gtree.3 gtree_last.3
util_MLINKS+=	gtree.3 gtree_next.3
util_MLINKS+=	gtree.3 gtree_prev.3
util_MLINKS+=	gtree.3 gtree_print.3
util_MLINKS+=	gtree.3 gtree_put.3
util_MLINKS+=	gtree.3 gtree_remove.3
util_MLINKS+=	gtree.3 gtree_size.3
util_MLINKS+=	gtree.3 gtree_traverse.3

util_MAN+=	mesg_port.3
util_MLINKS+=	mesg_port.3 mesg_port_create.3
util_MLINKS+=	mesg_port.3 mesg_port_destroy.3
util_MLINKS+=	mesg_port.3 mesg_port_get.3
util_MLINKS+=	mesg_port.3 mesg_port_put.3
util_MLINKS+=	mesg_port.3 mesg_port_qlen.3

util_MAN+=	paction.3
util_MLINKS+=	paction.3 paction_start.3
util_MLINKS+=	paction.3 paction_cancel.3

util_MAN+=	pevent.3
util_MLINKS+=	pevent.3 pevent_ctx_create.3
util_MLINKS+=	pevent.3 pevent_ctx_destroy.3
util_MLINKS+=	pevent.3 pevent_ctx_count.3
util_MLINKS+=	pevent.3 pevent_register.3
util_MLINKS+=	pevent.3 pevent_unregister.3

util_MAN+=	rsa_util_sign.3
util_MLINKS+=	rsa_util_sign.3 rsa_util_verify.3
util_MLINKS+=	rsa_util_sign.3 rsa_util_verify_priv.3

util_MAN+=	string_enquote.3
util_MLINKS+=	string_enquote.3 string_dequote.3

util_MAN+=	tinfo.3
util_MLINKS+=	tinfo.3 TINFO_INIT.3
util_MLINKS+=	tinfo.3 tinfo_get.3
util_MLINKS+=	tinfo.3 tinfo_set.3
util_MLINKS+=	tinfo.3 tinfo_set_nocopy.3

util_MAN+=	typed_mem.3
util_MLINKS+=	typed_mem.3 ASPRINTF.3
util_MLINKS+=	typed_mem.3 CALLOC.3
util_MLINKS+=	typed_mem.3 FREE.3
util_MLINKS+=	typed_mem.3 MALLOC.3
util_MLINKS+=	typed_mem.3 REALLOC.3
util_MLINKS+=	typed_mem.3 REALLOCF.3
util_MLINKS+=	typed_mem.3 STRDUP.3
util_MLINKS+=	typed_mem.3 VASPRINTF.3
util_MLINKS+=	typed_mem.3 typed_mem_dump.3
util_MLINKS+=	typed_mem.3 typed_mem_enable.3
util_MLINKS+=	typed_mem.3 typed_mem_usage.3

