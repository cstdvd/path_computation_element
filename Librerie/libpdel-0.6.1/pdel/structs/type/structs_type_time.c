
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#ifdef WIN32
#include <pthread.h>	/* Need to pickup gmtime_r in pthread.h */
#endif

#include "pdel/pd_time.h"
#include "structs/structs.h"
#include "structs/type/array.h"
#include "structs/type/int.h"
#include "structs/type/time.h"
#include "util/typed_mem.h"

/*********************************************************************
			    TIME TYPE
*********************************************************************/

#define FMT_GMT		"%a %b %e %T GMT %Y"
#define FMT_LOCAL	"%a %b %e %T %Y"
#define FMT_IS08601	"%Y%m%dT%H:%M:%S"

#ifndef _KERNEL

static structs_ascify_t		structs_time_ascify;
static structs_binify_t		structs_time_binify;

static char *
structs_time_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const char *fmt = type->args[0].s;
	const int local = type->args[1].i;
	const time_t when = *((time_t *)data);
	struct tm tm;
	char buf[64];

	if (local)
		localtime_r(&when, &tm);
	else
		gmtime_r(&when, &tm);
	strftime(buf, sizeof(buf), fmt, &tm);
	return (STRDUP(mtype, buf));
}

static int
structs_time_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const char *fmt = type->args[0].s;
	const int local = type->args[1].i;
	struct tm whentm;
	time_t when;

	memset(&whentm, 0, sizeof(whentm));
	while (isspace(*ascii))
		ascii++;
	if (strptime(ascii, fmt, &whentm) == NULL) {
		strlcpy(ebuf, "invalid time", emax);
		errno = EINVAL;
		return (-1);
	}
	if (local) {
		whentm.tm_isdst = -1;
		when = mktime(&whentm);
	} else
		when = pd_timegm(&whentm);
	if (when == (time_t)-1) {
		errno = EDOM;
		return (-1);
	}
	*((time_t *)data) = when;
	return (0);
}
#endif	/* !_KERNEL */

const struct structs_type structs_type_time_gmt = {
	sizeof(time_t),
	"time",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
#ifndef _KERNEL
	structs_time_ascify,
	structs_time_binify,
#else
	structs_notsupp_ascify,
	structs_notsupp_binify,
#endif
	structs_region_encode_netorder,
	structs_region_decode_netorder,
	structs_nothing_free,
	{ { (void *)FMT_GMT }, { (void *)0 } }
};

const struct structs_type structs_type_time_local = {
	sizeof(time_t),
	"time",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
#ifndef _KERNEL
	structs_time_ascify,
	structs_time_binify,
#else
	structs_notsupp_ascify,
	structs_notsupp_binify,
#endif
	structs_region_encode_netorder,
	structs_region_decode_netorder,
	structs_nothing_free,
	{ { (void *)FMT_LOCAL }, { (void *)1 } }
};

const struct structs_type structs_type_time_iso8601 = {
	sizeof(time_t),
	"time",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
#ifndef _KERNEL
	structs_time_ascify,
	structs_time_binify,
#else
	structs_notsupp_ascify,
	structs_notsupp_binify,
#endif
	structs_region_encode_netorder,
	structs_region_decode_netorder,
	structs_nothing_free,
	{ { (void *)FMT_IS08601 }, { (void *)0 } }
};

/*
 * Absolute time.
 *
 * Use custom ascify/binify routines to work around FreeBSD
 * bug with '%s' format of strftime().
 */

/* XXX This assumes sizeof(time_t) <= sizeof(long) */

static structs_ascify_t		structs_time_abs_ascify;
static structs_binify_t		structs_time_abs_binify;

static char *
structs_time_abs_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const u_long when = (u_long)*((time_t *)data);

	return (structs_type_ulong.ascify(&structs_type_ulong, mtype, &when));
}

static int
structs_time_abs_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	u_long when;

	if (structs_type_ulong.binify(&structs_type_ulong,
	    ascii, &when, ebuf, emax) == -1)
		return (-1);
	*((time_t *)data) = (time_t)when;
	return (0);
}

const struct structs_type structs_type_time_abs = {
	sizeof(time_t),
	"time",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_time_abs_ascify,
	structs_time_abs_binify,
	structs_region_encode_netorder,
	structs_region_decode_netorder,
	structs_nothing_free,
};

/*
 * Relative time
 */

static structs_ascify_t		structs_reltime_ascify;
static structs_binify_t		structs_reltime_binify;

/* XXX This assumes sizeof(time_t) <= sizeof(long) */

static char *
structs_reltime_ascify(const struct structs_type *type,
	const char *mtype, const void *data)
{
	const time_t now = time(NULL);
	const time_t diff = *((time_t *)data) - now;
	char buf[32];

	snprintf(buf, sizeof(buf), "%ld", (u_long)diff);
	return (STRDUP(mtype, buf));
}

static int
structs_reltime_binify(const struct structs_type *type,
	const char *ascii, void *data, char *ebuf, size_t emax)
{
	const time_t now = time(NULL);
	int64_t absolute;
	int64_t diff;

	if (structs_type_int64.binify(&structs_type_int64,
	    ascii, &diff, ebuf, emax) == -1)
		return (-1);
	absolute = (int64_t)now + diff;
	if (absolute < (int64_t)~0
	    || absolute > ((int64_t)1 << ((sizeof(time_t) * 8) - 1)) - 1) {
		errno = EDOM;
		return (-1);
	}
	*((time_t *)data) = (time_t)absolute;
	return (0);
}

const struct structs_type structs_type_time_rel = {
	sizeof(time_t),
	"reltime",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_reltime_ascify,
	structs_reltime_binify,
	structs_region_encode_netorder,
	structs_region_decode_netorder,
	structs_nothing_free,
};

