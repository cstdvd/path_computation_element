
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_PPP_PPP_DEFS_H_
#define _PDEL_PPP_PPP_DEFS_H_

/************************************************************************
			PPP PRIVATE DEFINITIONS
************************************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_ppp.h>
#include <netgraph/ng_vjc.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <syslog.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <netgraph.h>

#include <openssl/ssl.h>

#include "structs/structs.h"
#include "structs/type/array.h"

#include "util/ghash.h"
#include "util/pevent.h"
#include "util/paction.h"
#include "util/mesg_port.h"
#include "util/typed_mem.h"

#include "ppp/ppp_lib.h"

/*
 * PPP Protocols
 */
#define PPP_PROTO_LCP		0xc021
#define PPP_PROTO_PAP		0xc023
#define PPP_PROTO_CHAP		0xc223
#define PPP_PROTO_MP		0x003d
#define PPP_PROTO_IPCP		0x8021
#define PPP_PROTO_IP		0x0021
#define PPP_PROTO_VJCOMP	0x002d
#define PPP_PROTO_VJUNCOMP	0x002f
#define PPP_PROTO_CCP		0x80fd
#define PPP_PROTO_COMPD		0x00fd

#define PPP_PROTO_VALID(p)		(((p) & 0x0101) == 0x0001)
#define PPP_PROTO_NETWORK_DATA(p)	(((p) & 0xC000) == 0x0000)
#define PPP_PROTO_LOW_VOLUME(p)		(((p) & 0xC000) == 0x4000)
#define PPP_PROTO_NETWORK_CTRL(p)	(((p) & 0xC000) == 0x8000)
#define PPP_PROTO_LINK_LAYER(p)		(((p) & 0xC000) == 0xC000)
#define PPP_PROTO_COMPRESSIBLE(p)	(((p) & 0xFF00) == 0x0000)

/*
 * To get ppp internal stuff
 */
#define _PDEL_PPP_PRIVATE_H_	1

/*
 * Debugging
 */
#define PPP_DEBUG		0

/*
 * Macro for logging
 */
#if PPP_DEBUG
#define PPP_LOG(log, sev, fmt, args...)				\
	    ppp_log_put(log, sev, "%s:%u: " fmt,		\
		__FUNCTION__, __LINE__ , ## args)
#else
#define PPP_LOG(log, sev, fmt, args...)				\
	    ppp_log_put(log, sev, fmt , ## args)
#endif

#endif	/* _PDEL_PPP_PPP_DEFS_H_ */
