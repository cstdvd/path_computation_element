/*
 * Linux hacks.h
 *
 * Portability hacks needed to build libpel.
 *
 * Most of this file has been moved to pd_port.h, only things needed
 * for the implementaiton or that need to get defined _before_ system
 * headers go here. 
 */

#ifndef __PORTS_LINUX_HACKS_H_
#define __PORTS_LINUX_HACKS_H_ 1

#define _XOPEN_SOURCE	600
#define _GNU_SOURCE	1
#define _BSD_SOURCE	1
#define _ISOC99_SOURCE	1

#include <string.h>		/* memcpy(), etc. */
#include <netinet/in.h>		/* htonl(), etc. */

#endif
