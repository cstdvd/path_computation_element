
/*
 * thread.c
 *
 * Misc. thread utilities.
 * adapted from tcp_server.c.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#include <pthread.h>
#include "pdel/pd_thread.h"

/***************************************************
 *
 * Public Global Data
 *
 ***************************************************/

#ifdef PTW32_VERSION

/* GNU pthread-win32 */

pthread_t	pd_null_pthread;

#else

pthread_t	pd_null_pthread;

#endif


/***************************************************
 *
 * Public APIs
 *
 ***************************************************/
