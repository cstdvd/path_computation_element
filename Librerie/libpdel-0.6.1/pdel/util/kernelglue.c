
/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include "kernelglue.h"

/*
 * Kernelglue kernel memory type
 */
MALLOC_DEFINE(M_LIBPDEL, "LIBPDEL", "libpdel kernel memory");

/* Errno */
int	errno;

char *
strdup(const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if ((copy = MALLOC(NULL, len)) == NULL)
		return (NULL);
	memcpy(copy, str, len);
	return (copy);
}

int
asprintf(char **ret, const char *format, ...)
{
	va_list args;
	int rtn;

	va_start(args, format);
	rtn = vasprintf(ret, format, args);
	va_end(args);
	return (rtn);
}

int
vasprintf(char **ret, const char *format, va_list ap)
{
	size_t buflen = 256;
	char *buf;
	int slen;

	/* Initialize return value in case of failure */
	*ret = NULL;

try_again:
	/* Allocate buffer */
	if ((buf = malloc(buflen, M_LIBPDEL, M_NOWAIT)) == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	/* Format string */
	slen = vsnprintf(buf, buflen, format, ap);

	/* If buffer was big enough, we're done */
	if (slen < buflen) {
		*ret = buf;
		return (slen);
	}

	/* Increase buffer size and try again */
	free(buf, M_LIBPDEL);
	buflen = slen + 1;
	goto try_again;
}

char *
strchr(const char *s, int c)
{
	while (1) {
		if (*s == (char)c)
			return ((char *)s);
		if (*s == '\0')
			return (NULL);
		s++;
	}
}

int
strcasecmp(const char *s1, const char *s2)
{
	const u_char *s = s1;
	const u_char *t = s2;

        while (*s != '\0' && *t != '\0' && tolower(*s) == tolower(*t)) {
		s++;
		t++;
        }
        return (*s - *t);
}

size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

void *
memmove(void *dst, const void *src, size_t len)
{
	ovbcopy(src, dst, len);
	return (dst);
}

time_t 
time(time_t *newtime)
{
	struct timeval valtime;

	getmicrotime(&valtime);
	if (newtime)
		*newtime = valtime.tv_sec;
	return (valtime.tv_sec);
}

char *
strerror(int errnum)
{
	static char buf[32];
	const char *s;

	switch (errnum) {
	case EPERM:		s = "Operation not permitted"; break;
	case ENOENT:		s = "No such file or directory"; break;
	case ESRCH:		s = "No such process"; break;
	case EINTR:		s = "Interrupted system call"; break;
	case EIO:		s = "Input/output error"; break;
	case ENXIO:		s = "Device not configured"; break;
	case E2BIG:		s = "Argument list too long"; break;
	case ENOEXEC:		s = "Exec format error"; break;
	case EBADF:		s = "Bad file descriptor"; break;
	case ECHILD:		s = "No child processes"; break;
	case EDEADLK:		s = "Resource deadlock avoided"; break;
	case ENOMEM:		s = "Cannot allocate memory"; break;
	case EACCES:		s = "Permission denied"; break;
	case EFAULT:		s = "Bad address"; break;
	case ENOTBLK:		s = "Block device required"; break;
	case EBUSY:		s = "Device busy"; break;
	case EEXIST:		s = "File exists"; break;
	case EXDEV:		s = "Cross-device link"; break;
	case ENODEV:		s = "Operation not supported by device"; break;
	case ENOTDIR:		s = "Not a directory"; break;
	case EISDIR:		s = "Is a directory"; break;
	case EINVAL:		s = "Invalid argument"; break;
	case ENFILE:		s = "Too many open files in system"; break;
	case EMFILE:		s = "Too many open files"; break;
	case ENOTTY:		s = "Inappropriate ioctl for device"; break;
	case ETXTBSY:		s = "Text file busy"; break;
	case EFBIG:		s = "File too large"; break;
	case ENOSPC:		s = "No space left on device"; break;
	case ESPIPE:		s = "Illegal seek"; break;
	case EROFS:		s = "Read-only file system"; break;
	case EMLINK:		s = "Too many links"; break;
	case EPIPE:		s = "Broken pipe"; break;
	case EDOM:		s = "Numerical argument out of domain"; break;
	case ERANGE:		s = "Result too large"; break;
	case EAGAIN:		s = "Resource temporarily unavailable"; break;
	case EINPROGRESS:	s = "Operation now in progress"; break;
	case EALREADY:		s = "Operation already in progress"; break;
	case ENOTSOCK:		s = "Socket operation on non-socket"; break;
	case EDESTADDRREQ:	s = "Destination address required"; break;
	case EMSGSIZE:		s = "Message too long"; break;
	case EPROTOTYPE:	s = "Protocol wrong type for socket"; break;
	case ENOPROTOOPT:	s = "Protocol not available"; break;
	case EPROTONOSUPPORT:	s = "Protocol not supported"; break;
	case ESOCKTNOSUPPORT:	s = "Socket type not supported"; break;
	case EOPNOTSUPP:	s = "Operation not supported"; break;
	case EPFNOSUPPORT:	s = "Protocol family not supported"; break;
	case EAFNOSUPPORT:
		s = "Address family not supported by protocol family"; break;
	case EADDRINUSE:	s = "Address already in use"; break;
	case EADDRNOTAVAIL:	s = "Can't assign requested address"; break;
	case ENETDOWN:		s = "Network is down"; break;
	case ENETUNREACH:	s = "Network is unreachable"; break;
	case ENETRESET:	
		s = "Network dropped connection on reset"; break;
	case ECONNABORTED:	s = "Software caused connection abort"; break;
	case ECONNRESET:	s = "Connection reset by peer"; break;
	case ENOBUFS:		s = "No buffer space available"; break;
	case EISCONN:		s = "Socket is already connected"; break;
	case ENOTCONN:		s = "Socket is not connected"; break;
	case ESHUTDOWN:		s = "Can't send after socket shutdown"; break;
	case ETOOMANYREFS:	s = "Too many references: can't splice"; break;
	case ETIMEDOUT:		s = "Operation timed out"; break;
	case ECONNREFUSED:	s = "Connection refused"; break;
	case ELOOP:		s = "Too many levels of symbolic links"; break;
	case ENAMETOOLONG:	s = "File name too long"; break;
	case EHOSTDOWN:		s = "Host is down"; break;
	case EHOSTUNREACH:	s = "No route to host"; break;
	case ENOTEMPTY:		s = "Directory not empty"; break;
	case EPROCLIM:		s = "Too many processes"; break;
	case EUSERS:		s = "Too many users"; break;
	case EDQUOT:		s = "Disc quota exceeded"; break;
	case ESTALE:		s = "Stale NFS file handle"; break;
	case EREMOTE:		s = "Too many levels of remote in path"; break;
	case EBADRPC:		s = "RPC struct is bad"; break;
	case ERPCMISMATCH:	s = "RPC version wrong"; break;
	case EPROGUNAVAIL:	s = "RPC prog. not avail"; break;
	case EPROGMISMATCH:	s = "Program version wrong"; break;
	case EPROCUNAVAIL:	s = "Bad procedure for program"; break;
	case ENOLCK:		s = "No locks available"; break;
	case ENOSYS:		s = "Function not implemented"; break;
	case EFTYPE:		s = "Inappropriate file type or format"; break;
	case EAUTH:		s = "Authentication error"; break;
	case ENEEDAUTH:		s = "Need authenticator"; break;
	case EIDRM:		s = "Identifier removed"; break;
	case ENOMSG:		s = "No message of desired type"; break;
	case EOVERFLOW:		s =
		"Value too large to be stored in data type"; break;
	case ECANCELED:		s = "Operation canceled"; break;
	case EILSEQ:		s = "Illegal byte sequence"; break;
	default:
		snprintf(buf, sizeof(buf), "Unknown error: %d", errnum);
		errno = EINVAL;
		return (buf);
	}
	strlcpy(buf, s, sizeof(buf));
	return (buf);
}

void *
kern_malloc(size_t size)
{
	void *mem;

	if ((mem = malloc((u_long)size, M_LIBPDEL, M_NOWAIT)) == NULL)
		errno = ENOMEM;
	return (mem);
}

void *
kern_realloc(void *mem, size_t size)
{
	if ((mem = realloc(mem,
	    (u_long)size, M_LIBPDEL, M_NOWAIT)) == NULL)
		errno = ENOMEM;
	return (mem);
}

void
kern_free(void *mem)
{
	free(mem, M_LIBPDEL);
}

