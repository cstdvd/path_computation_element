
/*
 * string_misc.c
 *
 * Misc. string utilities.  Mostly borrowed from FreeBSD except as noted.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */


#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "pdel/pd_string.h"


/* Borrowed from FreeBSD - BSD License */

/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
char *
pd_strsep(stringp, delim)
        char **stringp;
        const char *delim;
{
        char *s;
        const char *spanp;
        int c, sc;
        char *tok;

        if ((s = *stringp) == NULL)
                return (NULL);
        for (tok = s;;) {
                c = *s++;
                spanp = delim;
                do {
                        if ((sc = *spanp++) == c) {
                                if (c == 0)
                                        s = NULL;
                                else
                                        s[-1] = 0;
                                *stringp = s;
                                return (tok);
                        }
                } while (sc != 0);
        }
        /* NOTREACHED */
}


/* 
 * realpath.c
 * 
 * Borrowed from MinGW and adapted to fall through to native realpath()
 * if available.
 *
 * Provides an implementation of the "realpath" function, conforming
 * approximately to SUSv3, and adapted for use on native Microsoft(R)
 * Win32 platforms.
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 *
 * This is free software.  You may redistribute and/or modify it as you
 * see fit, without restriction of copyright.
 *
 * This software is provided "as is", in the hope that it may be useful,
 * but WITHOUT WARRANTY OF ANY KIND, not even any implied warranty of
 * MERCHANTABILITY, nor of FITNESS FOR ANY PARTICULAR PURPOSE.  At no
 * time will the author accept any form of liability for any damages,
 * however caused, resulting from the use of this software.
 *
 */

char *
pd_realpath( const char *name, char *resolved )
{
#ifndef WIN32
	return(realpath(name, resolved));
#else
	char *retname = NULL;	/* we will return this, if we fail */

	/* SUSv3 says we must set `errno = EINVAL', and return NULL,
	 * if `name' is passed as a NULL pointer.
	 */

	if( name == NULL )
		errno = EINVAL;

	/* Otherwise, `name' must refer to a readable filesystem object,
	 * if we are going to resolve its absolute path name.
	 */

	else if( access( name, 4 ) == 0 )
	{
		/* If `name' didn't point to an existing entity,
		 * then we don't get to here; we simply fall past this block,
		 * returning NULL, with `errno' appropriately set by `access'.
		 *
		 * When we _do_ get to here, then we can use `_fullpath' to
		 * resolve the full path for `name' into `resolved', but first,
		 * check that we have a suitable buffer, in which to return it.
		 */

		if( (retname = resolved) == NULL )
		{
			/* 
			 * Caller didn't give us a buffer, 
			 * so we'll exercise the
			 * option granted by SUSv3, and allocate one.
			 *
			 * `_fullpath' would do this for us, 
			 * but it uses `malloc', and
			 * Microsoft's implementation doesn't set `errno' 
			 * on failure. If we don't do this explicitly
			 * ourselves, then we will not know if 
			 * `_fullpath' fails on `malloc' failure, 
			 * or for some other reason, and we want to 
			 * set `errno = ENOMEM' for the `malloc' failure case.
			 */

			retname = malloc( _MAX_PATH );
		}

		/* By now, we should have a valid buffer.
		 * If we don't, then we know that `malloc' failed,
		 * so we can set `errno = ENOMEM' appropriately.
		 */

		if( retname == NULL )
			errno = ENOMEM;

		/* Otherwise, when we do have a valid buffer,
		 * `_fullpath' should only fail if the path name is too long.
		 */

		else if( (retname = _fullpath( retname, name, 
					       _MAX_PATH )) == NULL )
			errno = ENAMETOOLONG;
	}

	/* By the time we get to here,
	 * `retname' either points to the required resolved path name,
	 * or it is NULL, with `errno' set appropriately, either of which
	 * is our required return condition.
	 */
	
	return retname;
#endif
}



/* Entries EAI_ADDRFAMILY (1) and EAI_NODATA (7) are obsoleted, but left */
/* for backward compatibility with userland code prior to 2553bis-02 */
static const char *ai_errlist[] = {
	"Success",					/* 0 */
	"Address family for hostname not supported",	/* 1 */
	"Temporary failure in name resolution",		/* EAI_AGAIN */
	"Invalid value for ai_flags",			/* EAI_BADFLAGS */
	"Non-recoverable failure in name resolution",	/* EAI_FAIL */
	"ai_family not supported",			/* EAI_FAMILY */
	"Memory allocation failure", 			/* EAI_MEMORY */
	"No address associated with hostname",		/* 7 */
	"hostname nor servname provided, or not known",	/* EAI_NONAME */
	"servname not supported for ai_socktype",	/* EAI_SERVICE */
	"ai_socktype not supported", 			/* EAI_SOCKTYPE */
	"System error returned in errno", 		/* EAI_SYSTEM */
	"Invalid value for hints",			/* EAI_BADHINTS */
	"Resolved protocol is unknown"			/* EAI_PROTOCOL */
};

#ifndef EAI_MAX
#define EAI_MAX (sizeof(ai_errlist)/sizeof(ai_errlist[0]))
#endif

const char *
pd_gai_strerror(int ecode)
{
	if (ecode >= 0 && ecode < EAI_MAX)
		return ai_errlist[ecode];
	return "Unknown error";
}

    


