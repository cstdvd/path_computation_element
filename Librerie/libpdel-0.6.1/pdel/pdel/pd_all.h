/*
 * pd_all.h
 *
 * This header includes _all_ public PD headers.  It is useful for 
 * quick compile testing, pre-compiled header generation and 
 * reference for header ordering.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_ALL_H__
#define __PDEL_PD_ALL_H__

#include <sys/types.h>
#ifndef WIN32
#include <sys/param.h>
#endif
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

/* PDEL Public Includes */

#ifdef BUILDING_PDEL
#include "version.h"
#else
#include "pdel/version.h"
#endif

#include "pdel/pd_base.h"
#include "pdel/pd_port.h"
#include "pdel/pd_inet.h"
#include "pdel/pd_io.h"
#include "pdel/pd_mem.h"
#include "pdel/pd_poll.h"
#include "pdel/pd_regex.h"
#include "pdel/pd_stdio.h"
#include "pdel/pd_string.h"
#include "pdel/pd_thread.h"
#include "pdel/pd_time.h"

#ifdef PDEL_NET_SUPPORT
#include <netgraph.h>
#include <netgraph/ng_ppp.h>
#include <net/bpf.h>
#include <radlib.h>
#include <radlib_vs.h>
#endif

#include "config/app_config.h"
#include "http/http_defs.h"
#include "http/http_server.h"
#include "http/http_servlet.h"
#include "http/servlet/basicauth.h"
#include "http/servlet/cookieauth.h"
#include "http/servlet/redirect.h"
#include "http/servlet/xml.h"
#include "http/servlet/xmlrpc.h"
#include "io/base64.h"
#include "io/boundary_fp.h"
#include "io/count_fp.h"
#include "io/filter.h"
#include "io/ssl_fp.h"
#include "io/string_fp.h"
#include "io/timeout_fp.h"
#include "net/domain_server.h"
#include "net/tcp_server.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/array_define.h"
#include "structs/type/boolean.h"
#include "structs/type/data.h"
#include "structs/type/dnsname.h"
#include "structs/type/ether.h"
#include "structs/type/float.h"
#include "structs/type/id.h"
#include "structs/type/int.h"
#include "structs/type/ip4.h"
#include "structs/type/ip6.h"
#include "structs/type/null.h"
#include "structs/type/pointer.h"
#include "structs/type/regex.h"
#include "structs/type/string.h"
#include "structs/type/struct.h"
#include "structs/type/time.h"
#include "structs/type/union.h"
#include "structs/types.h"
#include "structs/xml.h"
#include "structs/xmlrpc.h"
#include "sys/alog.h"
#include "sys/logfile.h"
#include "tmpl/tmpl.h"
#include "util/ghash.h"
#include "util/gtree.h"
#include "util/mesg_port.h"
#include "util/paction.h"
#include "util/pevent.h"
#include "util/rsa_util.h"
#include "util/string_quote.h"
#include "util/tinfo.h"
#include "util/typed_mem.h"

/*
 * Public headers generally dependent on other headers in 
 * non-alphabetical order
 */
#include "http/xml.h"
#include "http/servlet/tmpl.h"
#include "http/servlet/file.h"

/* PDEL Public Includes - BSD Platforms Only */

#ifdef PDEL_NET_SUPPORT
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>

#include "net/if_util.h"
#include "net/route_msg.h"
#include "net/uroute.h"

#include "ppp/ppp_log.h"
#include "ppp/ppp_lib.h"
#ifdef BUILDING_PDEL 
#define _PDEL_PPP_PRIVATE_H_ 1
#include "ppp/ppp_fsm_option.h"
#endif
#include "ppp/ppp_auth.h"
#ifdef BUILDING_PDEL 
#undef _PDEL_PPP_PRIVATE_H_
#endif
#include "ppp/ppp_auth_radius.h"
#include "ppp/ppp_ccp.h"
#include "ppp/ppp_ipcp.h"
#include "ppp/ppp_l2tp_avp.h"
#include "ppp/ppp_l2tp_ctrl.h"
#include "ppp/ppp_l2tp_server.h"
#include "ppp/ppp_lcp.h"
#include "ppp/ppp_manager.h"
#include "ppp/ppp_msoft.h"
#include "ppp/ppp_node.h"
#include "ppp/ppp_pptp_ctrl.h"
#include "ppp/ppp_pptp_ctrl_defs.h"
#include "ppp/ppp_pptp_server.h"
#include "ppp/ppp_util.h"
#include "structs/type/bpf.h"
#include "sys/fs_mount.h"
#endif

#endif
