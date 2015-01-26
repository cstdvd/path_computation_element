/*
 * MinGW net/in.h
 *
 * MinGW has no net/in.h, but we need to pick up the Winsock stuff.
 * We bite the bullet and go with Winsock 2.
 * to avoid modifying multiple source files.
 */

#ifndef __NET_IN_H__
#define __NET_IN_H__

#include <winsock2.h>

#endif
