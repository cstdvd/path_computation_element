/*-
 * vfprintf.c - Hacked vfprintf.
 *
 * The real vfprintf.c is dependent on additional BSD libc stuff like
 * gdtoa that we want to avoid bringing in, so we cheat and call the 
 * underlying OS snprintf and then call fwrite.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "pdel/pd_base.h"
#include <pd_stdio_p.h>

#undef FILE
#undef vfprintf
#undef vsnprintf
#undef vasprintf

#define BLEN 210

#ifdef WIN32
#define vsnprintf _vsnprintf
#endif

int
pd_vfprintf_p(struct __sFILEpd * __restrict fp, const char * __restrict fmt0,
	      __va_list ap)
{
	char	buf[BLEN+1];
	char	*tbuf = NULL;
	int	ret;

	/*
	 * Simple hack.  
	 * Try a local buffer, if that fails, let vasprintf allocate
	 * a buffer.
	 */

	ret = vsnprintf(buf, sizeof(buf), fmt0, ap);
	if (sizeof(buf) < ret) {
		tbuf = buf;
	} else {
#ifdef WIN32
		ret = pd_vasprintf_p(&tbuf, fmt0, ap);
#else
		ret = vasprintf(&tbuf, fmt0, ap);
#endif
		if (ret < 0) {
			return(ret);
		}
	}
	ret = strlen(tbuf);
	if (fwrite(tbuf, ret, 1, fp) < 1) {
		ret = 0;
	}
	if (tbuf != NULL && tbuf != buf) {
		free(tbuf);
	}
	return(ret);
}

/*      $OpenBSD: vasprintf.c,v 1.4 1998/06/21 22:13:47 millert Exp $   */

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MIN
#define MIN(a,b)	((a)<(b)?(a):(b))
#endif

int
pd_vasprintf_p(str, fmt, ap)
        char **str;
        const char *fmt;
        __va_list ap;
{
        int ret;
	char *old_buf = NULL, *buf = NULL;
	int buf_size = 500;

#define MAX_STRING 15000

	do {	  
		buf_size = MIN(buf_size * 2, MAX_STRING);
		old_buf = buf;
		buf = (unsigned char *)realloc(buf, buf_size);
		if (buf == NULL) {
			*str = NULL;
			if (old_buf != NULL) {
				free(old_buf);
			}
			errno = ENOMEM;
			return (-1);
		}
		ret = vsnprintf(buf, buf_size, fmt, ap);
	} while (buf_size <= MAX_STRING && ret > buf_size);
	*str = buf;
	return(ret);
}
