/*
 * MinGW sys/syslog.h
 *
 * MinGW has no sys/syslog.h. Traditionally there is 
 * voodoo that sys/syslog.h and syslog.h are equiv, so
 * repeat that here.
 */

#ifndef __SYS_SYSLOG_H__
#define __SYS_SYSLOG_H__ 1

#ifndef __SYSLOG_H__
#include <syslog.h>
#endif

#endif
