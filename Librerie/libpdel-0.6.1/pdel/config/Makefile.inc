# $Id: Makefile.inc 482 2002-08-09 17:50:01Z archie $

config_PATH=	${PDELTOP}/config

config_SRCS=	app_config.c \
		app_config_alog.c \
		app_config_curconf.c \
		app_config_directory.c \
		app_config_pidfile.c

config_INCS=	app_config.h

config_MAN+=	app_config.3
config_MLINKS+=	app_config.3 app_config_alog_subsystem.3
config_MLINKS+=	app_config.3 app_config_copy.3
config_MLINKS+=	app_config.3 app_config_curconf_subsystem.3
config_MLINKS+=	app_config.3 app_config_directory_subsystem.3
config_MLINKS+=	app_config.3 app_config_free.3
config_MLINKS+=	app_config.3 app_config_get.3
config_MLINKS+=	app_config.3 app_config_get_type.3
config_MLINKS+=	app_config.3 app_config_init.3
config_MLINKS+=	app_config.3 app_config_load.3
config_MLINKS+=	app_config.3 app_config_new.3
config_MLINKS+=	app_config.3 app_config_pidfile_subsystem.3
config_MLINKS+=	app_config.3 app_config_reload.3
config_MLINKS+=	app_config.3 app_config_set.3
config_MLINKS+=	app_config.3 app_config_shutting_down.3
config_MLINKS+=	app_config.3 app_config_uninit.3

