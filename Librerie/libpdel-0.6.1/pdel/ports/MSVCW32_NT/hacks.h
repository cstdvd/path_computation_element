/*
 * Microsoft Visual Studio hacks.h
 * 
 * These are hacks specific to VSC.  Hacks common to all the "native" Win32 
 * ports are in ports/WIN32 (Cygwin isn't regarded as native).
 *
 * We include the ports/WIN32/hacks.h here manually as we only do one
 * -include.
 */

#ifndef __PORTS_MSVC_HACKS_H_
#define __PORTS_MSVC_HACKS_H_ 1

#define snprintf _snprintf
#define vsnprintf _vsnprintf 

#define __restrict
#define MAXPATHLEN 255

#include "ports/WIN32/w32hacks.h"

#endif
