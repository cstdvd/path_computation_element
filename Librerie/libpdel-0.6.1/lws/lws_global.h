
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#ifndef _LWS_GLOBAL_H_
#define _LWS_GLOBAL_H_

/* Includes for everyone */

#include <sys/stat.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <openssl/ssl.h>

#include <pdel/pd_base.h>

#undef __unused

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#ifndef WIN32
#include <unistd.h>
#include <err.h>
#include <netdb.h>
#include <syslog.h>
#else
#include <wtypes.h>
#include <windows.h>
#include <winbase.h>
#include <direct.h>
#include <io.h>
#endif

#include <pdel/pd_port.h>
#include <pdel/pd_sys.h>
#include <pdel/pd_regex.h>
#include <pdel/pd_syslog.h>
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

#include <pdel/tmpl/tmpl.h>
#include <pdel/util/typed_mem.h>
#include <pdel/util/tinfo.h>
#include <pdel/util/pevent.h>
#include <pdel/config/app_config.h>
#include <pdel/sys/alog.h>
#include <pdel/io/string_fp.h>
#include <pdel/io/base64.h>

#include <pdel/http/http_defs.h>
#include <pdel/http/http_server.h>
#include <pdel/http/servlet/basicauth.h>
#include <pdel/http/servlet/cookieauth.h>
#include <pdel/http/servlet/redirect.h>
#include <pdel/http/servlet/tmpl.h>
#include <pdel/http/servlet/file.h>

#if PDEL_VERSION < 000003003
#error "libpdel version 0.3.3 or later is required"
#endif

/* Definitions */

#define DEFAULT_CONFIG_FILE	"default-config.xml"
#define CONFIG_FILE		"config.xml"

/* Memory type used by all template strings */
#define TMPL_MEM_TYPE		"lws_tmpl"

/* Variables */

extern pid_t	pid;
extern int	debug_level;
extern struct	pevent_ctx *lws_event_ctx;

#endif	/* !_LWS_GLOBAL_H_ */
