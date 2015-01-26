
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include <netdb.h>
#include <pthread.h>
#include <err.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>

#include <pdel/structs/structs.h>
#include <pdel/structs/structs.h>
#include <pdel/structs/type/array.h>
#include <pdel/structs/type/int.h>
#include <pdel/structs/type/string.h>
#include <pdel/structs/type/struct.h>
#include <pdel/structs/type/union.h>
#include <pdel/structs/xmlrpc.h>

#include <pdel/tmpl/tmpl.h>
#include <pdel/sys/alog.h>
#include <pdel/util/typed_mem.h>
#include <pdel/util/pevent.h>

#include <pdel/http/http_defs.h>
#include <pdel/http/http_server.h>
#include <pdel/http/http_servlet.h>
#include <pdel/http/servlet/xmlrpc.h>

#include <pdel/version.h>

#define MEM_TYPE		"xmlrpc_test"

struct three_stooges {
	int32_t		moe;
	int32_t		larry;
	int32_t		curly;
};

extern const struct structs_type three_stooges_type;

extern int	debug_level;

extern const	char host_os[];
extern const	char host_arch[];

extern const	struct http_servlet_xmlrpc_method arrayOfStructsTest_method;
extern const	struct http_servlet_xmlrpc_method countTheEntities_method;
extern const	struct http_servlet_xmlrpc_method easyStructTest_method;
extern const	struct http_servlet_xmlrpc_method echoStructTest_method;
extern const	struct http_servlet_xmlrpc_method manyTypesTest_method;
extern const	struct http_servlet_xmlrpc_method moderateSizeArrayCheck_method;
extern const	struct http_servlet_xmlrpc_method nestedStructTest_method;
extern const	struct http_servlet_xmlrpc_method simpleStructReturnTest_method;
extern const	struct http_servlet_xmlrpc_method faultTest_method;

