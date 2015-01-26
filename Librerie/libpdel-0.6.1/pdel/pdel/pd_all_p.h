/*
 * pd_all.h
 *
 * This header includes _all_ public PD headers.  It is useful for 
 * quick compile testing, pre-compiled header generation and 
 * reference for header ordering.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_ALL_P_H__
#define __PDEL_PD_ALL_P_H__

/* PDEL Private Include Dependencies */

#include <wchar.h>
#include <wctype.h>
#include <sys/queue.h>

/* PDEL Private Includes */

#ifdef PDEL_NET_SUPPORT
#define _PDEL_PPP_PRIVATE_H_ 1

#include "ppp/ppp_defs.h"
#include "ppp/ppp_auth_chap.h"
#include "ppp/ppp_auth_pap.h"
#include "ppp/ppp_bundle.h"
#include "ppp/ppp_channel.h"
#include "ppp/ppp_engine.h"
#include "ppp/ppp_fsm.h"
#include "ppp/ppp_link.h"
#endif

#include "private/debug.h"
#include "http/http_internal.h"
#include "regex/pd_regex_utils.h"
#include "regex/pd_regex2.h"
#include "regex/cname.h"
#include "stdio/floatio.h"
#include "stdio/fvwrite.h"
#include "stdio/glue.h"
#include "stdio/libc_private.h"
#include "stdio/namespace.h"
#ifdef NEED_FUNOPEN
#include "stdio/local.h"
#include "stdio/pd_stdio_p.h"
#include "stdio/un-namespace.h"
#endif
#include "tmpl/tmpl_internal.h"
#include "util/internal.h"

#endif
