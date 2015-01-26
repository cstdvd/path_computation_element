/*
 * MinGW sys/select.h
 *
 * MinGW has no sys/select.h, but we need to pick up 
 * select() which is in winsock.
 */

#ifndef __SYS_SELECT_H__
#define __SYS_SELECT_H__ 1

#include <winsock2.h>

#endif
