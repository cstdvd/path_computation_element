# $Id: Makefile.inc 1025 2006-12-13 01:08:36Z mgooderum $

sys_PATH=	${PDELTOP}/sys
sys_SRCS=	alog.c \
		fs_mount.c \
		logfile.c \
		pd_mmap.c

sys_INCS=	alog.h \
		fs_mount.h \
		logfile.h 

sys_MAN+=	alog.3
sys_MLINKS+=	alog.3 alog_clear_history.3
sys_MLINKS+=	alog.3 alog_configure.3
sys_MLINKS+=	alog.3 alog_expand.3
sys_MLINKS+=	alog.3 alog_facility.3
sys_MLINKS+=	alog.3 alog_facility_name.3
sys_MLINKS+=	alog.3 alog_get_history.3
sys_MLINKS+=	alog.3 alog_set_channel.3
sys_MLINKS+=	alog.3 alog_set_debug.3
sys_MLINKS+=	alog.3 alog_severity.3
sys_MLINKS+=	alog.3 alog_severity_name.3
sys_MLINKS+=	alog.3 alog_shutdown.3
sys_MLINKS+=	alog.3 valog.3

sys_MAN+=	logfile.3
sys_MLINKS+=	logfile.3 logfile_close.3
sys_MLINKS+=	logfile.3 logfile_get.3
sys_MLINKS+=	logfile.3 logfile_num_entries.3
sys_MLINKS+=	logfile.3 logfile_open.3
sys_MLINKS+=	logfile.3 logfile_put.3
sys_MLINKS+=	logfile.3 logfile_sync.3
sys_MLINKS+=	logfile.3 logfile_trim.3

sys_MAN+=	fs_mount.3
sys_MLINKS+=	fs_mount.3 fs_unmount.3

