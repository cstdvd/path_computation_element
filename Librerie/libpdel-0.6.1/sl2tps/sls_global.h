
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _SL2TPS_GLOBAL_H_
#define _SL2TPS_GLOBAL_H_

/* Includes for everyone */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if.h>

#include <netgraph/ng_iface.h>
#include <netgraph/ng_message.h>

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <unistd.h>
#include <fcntl.h>
#include <netgraph.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#include <pdel/version.h>
#include <pdel/structs/structs.h>
#include <pdel/structs/type/array.h>
#include <pdel/structs/type/boolean.h>
#include <pdel/structs/type/id.h>
#include <pdel/structs/type/int.h>
#include <pdel/structs/type/ip4.h>
#include <pdel/structs/type/regex.h>
#include <pdel/structs/type/string.h>
#include <pdel/structs/type/struct.h>
#include <pdel/structs/type/time.h>
#include <pdel/structs/type/union.h>
#include <pdel/structs/xml.h>

#include <pdel/util/typed_mem.h>
#include <pdel/util/tinfo.h>
#include <pdel/util/pevent.h>
#include <pdel/net/if_util.h>
#include <pdel/config/app_config.h>
#include <pdel/sys/alog.h>
#include <pdel/io/string_fp.h>
#include <pdel/io/base64.h>

#include <pdel/ppp/ppp_lib.h>
#include <pdel/ppp/ppp_log.h>
#include <pdel/ppp/ppp_auth.h>
#include <pdel/ppp/ppp_auth_chap.h>
#include <pdel/ppp/ppp_link.h>
#include <pdel/ppp/ppp_bundle.h>
#include <pdel/ppp/ppp_msoft.h>
#include <pdel/ppp/ppp_engine.h>
#include <pdel/ppp/ppp_manager.h>
#include <pdel/ppp/ppp_l2tp_server.h>

#if PDEL_VERSION < 1000005000
#error "libpdel version 0.5.0 or later is required"
#endif

/*
 * Definitions
 */

#define CONFIG_FILE		"config.xml"

/*
 * Variables
 */

extern pid_t	pid;
extern int	debug_level;
extern struct	pevent_ctx *lws_event_ctx;

extern struct	ppp_manager sls_manager;
extern const	struct ppp_auth_config sls_auth_config;

extern struct	ppp_engine *engine;

extern u_int32_t	*ip_pool;

/*
 * Functions
 */

extern int	sls_l2tp_start(struct ppp_engine *engine);
extern void	sls_l2tp_stop(struct ppp_engine *engine);

#endif	/* !_SL2TPS_GLOBAL_H_ */
