# $Id: Makefile.inc 1025 2006-12-13 01:08:36Z mgooderum $

net_PATH+=	${PDELTOP}/net

net_SRCS+=	domain_server.c \
		if_arp.c \
		if_iface.c \
		if_ip.c \
		pd_inet_ntop.c	\
		pd_inet_pton.c	\
		route_msg.c \
		tcp_server.c \
		uroute.c

net_INCS+=	domain_server.h \
		if_util.h \
		route_msg.h \
		tcp_server.h \
		uroute.h

net_MAN+=	domain_server.3
net_MLINKS+=	domain_server.3 domain_connection_get_cookie.3
net_MLINKS+=	domain_server.3 domain_connection_get_fd.3
net_MLINKS+=	domain_server.3 domain_connection_get_fp.3
net_MLINKS+=	domain_server.3 domain_connection_get_peer.3
net_MLINKS+=	domain_server.3 domain_connection_get_server.3
net_MLINKS+=	domain_server.3 domain_server_get_cookie.3
net_MLINKS+=	domain_server.3 domain_server_start.3
net_MLINKS+=	domain_server.3 domain_server_stop.3

net_MAN+=	if_util.3
net_MLINKS+=	if_util.3 if_add_ip_addr.3
net_MLINKS+=	if_util.3 if_del_ip_addr.3
net_MLINKS+=	if_util.3 if_flush_arp.3
net_MLINKS+=	if_util.3 if_get_arp.3
net_MLINKS+=	if_util.3 if_get_flags.3
net_MLINKS+=	if_util.3 if_get_ip_addr.3
net_MLINKS+=	if_util.3 if_get_ip_addrs.3
net_MLINKS+=	if_util.3 if_get_link_addr.3
net_MLINKS+=	if_util.3 if_get_list.3
net_MLINKS+=	if_util.3 if_get_mtu.3
net_MLINKS+=	if_util.3 if_get_type.3
net_MLINKS+=	if_util.3 if_set_arp.3
net_MLINKS+=	if_util.3 if_set_flags.3
net_MLINKS+=	if_util.3 if_set_mtu.3

net_MAN+=	route_msg.3
net_MLINKS+=	route_msg.3 route_msg_create.3
net_MLINKS+=	route_msg.3 route_msg_decode.3
net_MLINKS+=	route_msg.3 route_msg_destroy.3
net_MLINKS+=	route_msg.3 route_msg_encode.3
net_MLINKS+=	route_msg.3 route_msg_get_author.3
net_MLINKS+=	route_msg.3 route_msg_get_brd.3
net_MLINKS+=	route_msg.3 route_msg_get_dest.3
net_MLINKS+=	route_msg.3 route_msg_get_error.3
net_MLINKS+=	route_msg.3 route_msg_get_flags.3
net_MLINKS+=	route_msg.3 route_msg_get_gateway.3
net_MLINKS+=	route_msg.3 route_msg_get_genmask.3
net_MLINKS+=	route_msg.3 route_msg_get_ifa.3
net_MLINKS+=	route_msg.3 route_msg_get_ifp.3
net_MLINKS+=	route_msg.3 route_msg_get_index.3
net_MLINKS+=	route_msg.3 route_msg_get_netmask.3
net_MLINKS+=	route_msg.3 route_msg_get_pid.3
net_MLINKS+=	route_msg.3 route_msg_get_seq.3
net_MLINKS+=	route_msg.3 route_msg_get_type.3
net_MLINKS+=	route_msg.3 route_msg_print.3
net_MLINKS+=	route_msg.3 route_msg_recv.3
net_MLINKS+=	route_msg.3 route_msg_send.3
net_MLINKS+=	route_msg.3 route_msg_set_author.3
net_MLINKS+=	route_msg.3 route_msg_set_brd.3
net_MLINKS+=	route_msg.3 route_msg_set_dest.3
net_MLINKS+=	route_msg.3 route_msg_set_flags.3
net_MLINKS+=	route_msg.3 route_msg_set_gateway.3
net_MLINKS+=	route_msg.3 route_msg_set_genmask.3
net_MLINKS+=	route_msg.3 route_msg_set_ifa.3
net_MLINKS+=	route_msg.3 route_msg_set_ifp.3
net_MLINKS+=	route_msg.3 route_msg_set_index.3
net_MLINKS+=	route_msg.3 route_msg_set_netmask.3
net_MLINKS+=	route_msg.3 route_msg_set_type.3

net_MAN+=	tcp_server.3
net_MLINKS+=	tcp_server.3 tcp_connection_get_cookie.3
net_MLINKS+=	tcp_server.3 tcp_connection_get_fd.3
net_MLINKS+=	tcp_server.3 tcp_connection_get_fp.3
net_MLINKS+=	tcp_server.3 tcp_connection_get_peer.3
net_MLINKS+=	tcp_server.3 tcp_connection_get_server.3
net_MLINKS+=	tcp_server.3 tcp_server_get_cookie.3
net_MLINKS+=	tcp_server.3 tcp_server_start.3
net_MLINKS+=	tcp_server.3 tcp_server_stop.3

net_MAN+=	uroute.3
net_MLINKS+=	uroute.3 uroute_add.3
net_MLINKS+=	uroute.3 uroute_create.3
net_MLINKS+=	uroute.3 uroute_delete.3
net_MLINKS+=	uroute.3 uroute_destroy.3
net_MLINKS+=	uroute.3 uroute_get.3
net_MLINKS+=	uroute.3 uroute_get_all.3
net_MLINKS+=	uroute.3 uroute_get_dest.3
net_MLINKS+=	uroute.3 uroute_get_flags.3
net_MLINKS+=	uroute.3 uroute_get_gateway.3
net_MLINKS+=	uroute.3 uroute_get_netmask.3
net_MLINKS+=	uroute.3 uroute_print.3
net_MLINKS+=	uroute.3 uroute_set_flags.3

