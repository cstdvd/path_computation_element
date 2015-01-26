/*-
 * Copyright (c) 2003 Tim J. Robbins.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>


#include <pd_stdio_p.h>

#undef FILE

#if 0
#undef getchar_unlocked

int
pd_getchar_unlocked_p(void)
{

	return (__sgetc(stdin));
}
#endif

#undef getc_unlocked
int
pd_getc_unlocked_p(FILEpd *fp)
{

	return (__sgetc(fp));
}

#if 0
#undef putchar_unlocked
int
pd_putchar_unlocked_p(int ch)
{

	return (__sputc(ch, stdout));
}
#endif

#undef putc_unlocked
int
pd_putc_unlocked_p(int ch, FILEpd *fp)
{

	return (__sputc(ch, fp));
}

#undef feof_unlocked
int
pd_feof_unlocked_p(FILEpd *fp)
{

	return (__sfeof(fp));
}

#undef ferror_unlocked
int
pd_ferror_unlocked_p(FILEpd *fp)
{

	return (__sferror(fp));
}

#undef clearerr_unlocked
void
pd_clearerr_unlocked_p(FILEpd *fp)
{

	__sclearerr(fp);
}

#undef fileno_unlocked
int
pd_fileno_unlocked_p(FILEpd *fp)
{

	return (__sfileno(fp));
}
