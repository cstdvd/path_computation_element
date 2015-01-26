/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)findfp.c	8.2 (Berkeley) 1/4/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>


#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/param.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>

#include <pd_stdio_p.h>

#include "structs/structs.h"
#include "structs/type/array.h"
#include "util/typed_mem.h"
#include "util/ghash.h"
#include "local.h"
#include "glue.h"
#include "libc_private.h"

PD_EXPORT int	__sdidinit;
PD_EXPORT int	__isthreaded = 1;
void	(*__cleanup)(void);


struct ghash		*pd_files;	/* Our public/private files	*/
struct ghash		*pd_filepds;	/* Our private files		*/

static ghash_equal_t	FILEpd_equal;
static ghash_hash_t	FILEpd_hash;

static ghash_equal_t	FILE_equal;
static ghash_hash_t	FILE_hash;

static int		pd_funopen_init(void);

#define FILE_HASH_TYPE		"funopen.files"
#define FILEPD_HASH_TYPE	"funopen.filepds"
#define FILEPD_TYPE		"funopen.filepd"

int			pd_null_fd = -1;

/*
 * TO DO:
 */

#define THREAD_LOCK	do { if (__isthreaded) {  } } while (0)
#define THREAD_UNLOCK	do { if (__isthreaded) {  } } while (0)

/*
 * Allocate a free FILE for fopen et al.
 */
#undef FILE
#undef funopen
#undef fopen
#undef fdopen
#undef fclose

FILEpd *
__sfp()
{
	int	new_fd;
	int	r;
	FILEpd	*fp = NULL;
	FILE	*rfp = NULL;
	
	if (!__sdidinit) {
		__sinit();
	}

	/*
	 * Get our FD and "real" shadow FILE via fdopen.
	 */

	new_fd = dup(pd_null_fd);
	if (new_fd < 0) {
		return(NULL);
	}
	rfp = fdopen(new_fd, "r+");
	if (rfp == NULL) {
		pd_close(new_fd);
		return(NULL);
	}

	THREAD_LOCK;

	/*
	 * Get memory for our private FILEpd.
	 */
	fp = CALLOC(FILEPD_TYPE, sizeof(*fp), 1);
	if (fp == NULL) {
		fclose(rfp);
		return(NULL);
	}

	/* Now we're all setup, init the rest of our FILEpd */

	fp->_flags = 1;		/* reserve this slot; caller sets real flags */

	fp->_p = NULL;		/* no current pointer */
	fp->_w = 0;		/* nothing to read or write */
	fp->_r = 0;
	fp->_bf._base = NULL;	/* no buffer */
	fp->_bf._size = 0;
	fp->_lbfsize = 0;	/* not line buffered */
	fp->_file = -1;		/* no file */
/*	fp->_cookie = <any>; */	/* caller sets cookie, _read/_write etc */
	fp->_ub._base = NULL;	/* no ungetc buffer */
	fp->_ub._size = 0;
	fp->_lb._base = NULL;	/* no line buffer */
	fp->_lb._size = 0;
/*	fp->_lock = NULL; */	/* once set always set (reused) */
	fp->_extra = &fp->_extrabody;
	fp->_extra->orientation = 0;
	memset(&fp->_extra->mbstate, 0, sizeof(fp->_extra->mbstate));

	fp->_real.rfp = rfp;
	fp->_real.pdfp = fp;

	/*
	 * pthreads allows implicit dynamic initialization of a 
	 * pthread_mutex_t that has been statically initialized.
	 * In FreeBSD the default magic value is 0, but in other platforms
	 * it may be other magic numbers, so explicitly set here.
	 */
	fp->_extra->fl_mutex = PTHREAD_MUTEX_INITIALIZER;

	/* Add this to our hash tables */
	if ((r = ghash_put(pd_files, &fp->_real)) == -1) {
		fclose(rfp);
		FREE(FILEPD_TYPE, fp);
		THREAD_UNLOCK;
		return(NULL);
	}
	if ((r = ghash_put(pd_filepds, &fp->_real)) == -1) {
		fclose(rfp);
		ghash_remove(pd_files, &fp->_real);
		FREE(FILEPD_TYPE, fp);
		THREAD_UNLOCK;
		return(NULL);
	}

	THREAD_UNLOCK;


	return (fp);
}


/*
 * Close _just_ the private housekeeping and leave the public file alone,
 * used by the pd_freopen hack and our private pd_fclose_p().
 */
void
pd_fcloseonly_p(FILEpd *fp)
{
	THREAD_UNLOCK;
	ghash_remove(pd_files, &fp->_real);
	ghash_remove(pd_filepds, &fp->_real);
	FREE(FILEPD_TYPE, fp);

	THREAD_LOCK;

	return;
}

/*
 * exit() calls _cleanup() through *__cleanup, set whenever we
 * open or buffer a file.  This chicanery is done so that programs
 * that do not use stdio need not link it all in.
 *
 * The name `_cleanup' is, alas, fairly well known outside stdio.
 */
void
_cleanup()
{
	/* (void) _fwalk(fclose); */
	(void) _fwalk(__sflush);		/* `cheating' */
}

/*
 * __sinit() is called whenever stdio's internal variables must be set up.
 */
void
__sinit()
{
	THREAD_LOCK;
	if (__sdidinit == 0) {
		pd_funopen_init();

		/* Make sure we clean up on exit. */
		__cleanup = (void (*)(void)) _cleanup;		/* conservative */
		__sdidinit = 1;
	}
	THREAD_UNLOCK;
}


/* Convert FILE to FILEfp and back, return NULL if not one of us */
FILEpd *
file2filepd(FILE *rfp)
{
	filepd_pair *fpp;
	filepd_pair key;

	/* If sdidinit hasn't been called we have no private files */
	if (rfp == NULL || !__sdidinit) {
		return(NULL);
	}
	key.rfp = rfp;
	if ((fpp = ghash_get(pd_files, &key)) == NULL) {
		return(NULL);
	}
	return(fpp->pdfp);
}

FILE *
filepd2file(FILEpd *pdfp)
{
	filepd_pair *fpp;
	filepd_pair key;

	/* If sdidinit hasn't been called we have no private files */
	if (pdfp == NULL || !__sdidinit) {
		return(NULL);
	}
	key.pdfp = pdfp;
	if ((fpp = ghash_get(pd_filepds, &key)) == NULL) {
		return(NULL);
	}
	return(fpp->rfp);
}

/* 
 * Init the private file/filepd hash tables.
 */
static int
pd_funopen_init(void)
{
	if (pd_files != NULL && pd_filepds != NULL && pd_null_fd >= 0) {
		return(0);
	}
	/* Create FILE hash table */
	if (pd_files == NULL) {
		pd_files = ghash_create(NULL, 0, 0, FILE_HASH_TYPE,
					FILE_hash, FILE_equal, NULL, NULL);
	}
	/* Create FILEpd hash table */
	if (pd_filepds == NULL) {
		pd_filepds = ghash_create(NULL, 0, 0, FILEPD_HASH_TYPE,
					  FILEpd_hash, FILEpd_equal, 
					  NULL, NULL);
	}
	/* Create common null FD */
	if (pd_null_fd < 0) {
		pd_null_fd = open(_PATH_DEVNULL, O_RDWR, 0);
	}
	
	/* 
	 * Check for success, but Leave anything allocated alone, 
	 * maybe we'll get lucky.
	 */
	if (pd_files == NULL || pd_filepds == NULL || pd_null_fd < 0) {
		return(-ENOMEM);
	}
	return(0);
}
	

/*
 * FILE hashing routines
 */
static int
FILE_equal(struct ghash *g,
	   const void *item1, const void *item2)
{
	const filepd_pair *const f1 = item1;
	const filepd_pair *const f2 = item2;

	return(f1->rfp == f2->rfp);
}

u_int32_t
FILE_hash(struct ghash *g, const void *item)
{
	const filepd_pair *const fi = item;

	/* Use the pointer minus the low order two bits for the hash */
	return (((u_int32_t) fi->rfp) >> 2);
}

/*
 * FILEpd hashing routines
 */
static int
FILEpd_equal(struct ghash *g,
	   const void *item1, const void *item2)
{
	const filepd_pair *const f1 = item1;
	const filepd_pair *const f2 = item2;

	return(f1->pdfp == f2->pdfp);
}

u_int32_t
FILEpd_hash(struct ghash *g, const void *item)
{
	const filepd_pair *const f1 = item;

	/* Use the pointer minus the low order two bits for the hash */
	return (((u_int32_t) f1->pdfp) >> 2);
}
