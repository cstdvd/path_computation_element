# $Id: Makefile.inc 898 2004-06-02 17:22:59Z archie $

ppp_PATH=	${PDELTOP}/ppp
ppp_SRCS=	ppp_auth.c \
		ppp_auth_chap.c \
		ppp_auth_chap_md5.c \
		ppp_auth_chap_msv1.c \
		ppp_auth_chap_msv2.c \
		ppp_auth_pap.c \
		ppp_auth_radius.c \
		ppp_bundle.c \
		ppp_ccp.c \
		ppp_channel.c \
		ppp_engine.c \
		ppp_fsm.c \
		ppp_fsm_option.c \
		ppp_ipcp.c \
		ppp_l2tp_avp.c \
		ppp_l2tp_ctrl.c \
		ppp_l2tp_server.c \
		ppp_lcp.c \
		ppp_link.c \
		ppp_log.c \
		ppp_manager.c \
		ppp_msoft.c \
		ppp_node.c \
		ppp_pptp_ctrl.c \
		ppp_pptp_server.c \
		ppp_util.c

ppp_INCS=	ppp_auth.h \
		ppp_auth_chap.h \
		ppp_auth_radius.h \
		ppp_bundle.h \
		ppp_ccp.h \
		ppp_engine.h \
		ppp_ipcp.h \
		ppp_l2tp_server.h \
		ppp_lcp.h \
		ppp_lib.h \
		ppp_link.h \
		ppp_log.h \
		ppp_manager.h \
		ppp_msoft.h \
		ppp_pptp_server.h

ppp_MAN=	pdel_ppp.3

