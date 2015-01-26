/*
 * misc_io.h - Misc PD I/O routines.
 *
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com>
 */

#include <errno.h>
#include <stdio.h>

#ifndef WIN32
#include <unistd.h>
#else

#include <winsock2.h>
#include <winbase.h>
#include <io.h>

#endif

#include "pdel/pd_io.h"

/*
 * Win32ified wrapper for ftruncate.
 */
int 
pd_ftruncate(int fd, off_t len)
{
#ifndef WIN32
	return(ftruncate(fd, len));
#else
	HANDLE hFd = (HANDLE) _get_osfhandle(fd);

	LARGE_INTEGER oldp, newp;
	LARGE_INTEGER pos;

	/* save current position */
	pos.QuadPart = 0;
	if (!SetFilePointerEx(hFd, pos, &oldp, FILE_CURRENT)) {
		return(-1);
	}

	/* move to new position */
	pos.QuadPart = len;
	if (!SetFilePointerEx(hFd, pos, &newp, FILE_BEGIN)) {
		return(-1);
	}

	/* change file size */
	if (!SetEndOfFile(hFd)) {
		return(-1);
	}

	/*
	 *  ...and return to the original position,
	 * (the file pointer is not changed by ftruncate on POSIX systems.
	 */
	if (!SetFilePointerEx(hFd, oldp, &newp, FILE_BEGIN)) {
		return(-1);
	}

	return(0);
#endif
}

#undef read
int
pd_read(int s, char *buf, int len)
{
#ifndef WIN32
	return(read(s, buf, len));
#else
	int	ret;
	HANDLE	hFd = (HANDLE) _get_osfhandle(s);

	if (hFd != INVALID_HANDLE_VALUE) {
		/* We're a C runtime file descriptor */
		return(read(s, buf, len));
	}

	/*
	 * Otherwise we assume we're a socket.  We could be some other
	 * handle but those cannot usually be "read" usefully, we 
	 * could try to see if a disk file and call ReadFile[Ex] but
	 * something using CreateFile shouldn't be calling read() on it
	 * anyway.
	 */

	ret = recv(s, buf, len, 0);

	if (ret < 0 && WSAGetLastError() == WSAECONNRESET) {
		/* EOF on the pipe! (win32 socket based implementation) */
		ret = 0;
	}
	return(ret);
#endif
}

#undef write
int
pd_write(int s, const char *buf, int len)
{
#ifndef WIN32
	return(write(s, buf, len));
#else
	int	ret;
	int	err;
	HANDLE	hFd = (HANDLE) _get_osfhandle(s);

	if (hFd != INVALID_HANDLE_VALUE) {
		/* We're a C runtime file descriptor */
		return(write(s, buf, len));
	}

	/*
	 * Otherwise we assume we're a socket.  We could be some other
	 * handle but those cannot usually be "write" to usefully, we 
	 * could try to see if a disk file and call WriteFile[Ex] but
	 * something using CreateFile shouldn't be calling write() on it
	 * anyway.
	 */

	ret = send(s, buf, len, 0);

	if (ret < 0) {
		err = WSAGetLastError();
		errno = err;
	}
	return ret;
#endif
}

#undef close
int
pd_close(int s)
{
#ifndef WIN32
	return(close(s));
#else
	HANDLE	hFd = (HANDLE) _get_osfhandle(s);

	if (hFd != INVALID_HANDLE_VALUE) {
		/* We're a C runtime file descriptor */
		return(close(s));
	}

	/*
	 * Otherwise we assume we're a socket...
	 */

	return(closesocket(s));
#endif
}
