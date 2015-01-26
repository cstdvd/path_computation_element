/*
 * MinGW sys/socket.h
 *
 * MinGW has no sys/socket, but we need to pick up the Winsock stuff.
 * We bite the bullet and go with Winsock 2.
 * to avoid modifying multiple source files.
 */

#ifndef __SYS_SOCKET_H__
#define __SYS_SOCKET_H__ 1

#include <winsock2.h>

#endif
