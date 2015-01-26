
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_tmpl.h"
#include "lws_tmpl_passwd.h"
#ifndef WIN32
#include <pwd.h>
#endif

/***********************************************************************
			PASSWD OBJECT
***********************************************************************/

#define LWS_PASSWD_OBJECT_MTYPE	"passwd"

#define PASSWD_FIELD(n, t)	STRUCTS_STRUCT_FIELD2(passwd, pw_ ## n, #n, t)

/* Structs type for 'passwd' entry */
static const struct structs_field passwd_fields[] = {
	PASSWD_FIELD(name, &structs_type_string),
	PASSWD_FIELD(passwd, &structs_type_string),
	PASSWD_FIELD(uid, &structs_type_uint32),
	PASSWD_FIELD(gid, &structs_type_uint32),
#if !defined(__linux__) && !defined(__CYGWIN__) && !defined(WIN32)
	PASSWD_FIELD(change, &structs_type_time_abs),
	PASSWD_FIELD(class, &structs_type_string),
#endif
#ifdef __CYGWIN__
	PASSWD_FIELD(comment, &structs_type_string),
#endif
	PASSWD_FIELD(gecos, &structs_type_string),
	PASSWD_FIELD(dir, &structs_type_string),
	PASSWD_FIELD(shell, &structs_type_string),
#if !defined(__linux__) && !defined(__CYGWIN__) && !defined(WIN32)
	PASSWD_FIELD(expire, &structs_type_time_abs),
#endif
	{ NULL }
};
static const struct structs_type passwd_type
	= STRUCTS_STRUCT_TYPE(passwd, passwd_fields);

struct tinfo lws_tmpl_passwd_tinfo
	= TINFO_INIT(&passwd_type, LWS_PASSWD_OBJECT_MTYPE, NULL);

