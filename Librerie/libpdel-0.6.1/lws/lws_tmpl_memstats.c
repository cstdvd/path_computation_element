
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@packetdesign.com>
 */

#include "lws_global.h"
#include "lws_tmpl.h"
#include "lws_tmpl_memstats.h"

/***********************************************************************
			MEMORY STATISTICS OBJECT
***********************************************************************/

static tinfo_init_t	lws_memstats_object_init;

struct tinfo lws_tmpl_memstats_tinfo = TINFO_INIT(&typed_mem_stats_type,
	"tmpl_memstats", lws_memstats_object_init);

/*
 * Initialize this thread's memory statistics object.
 */
static int
lws_memstats_object_init(struct tinfo *t, void *data)
{
	return (typed_mem_usage(data));
}

