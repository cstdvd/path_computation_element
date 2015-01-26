
/*
 * pd_mem.h
 *
 * PD memory releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>	/* NULL */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "pdel/pd_mem.h"	/* Will have gotten sys/mman for us */
#include "util/typed_mem.h"

/***************************************************
 *
 * Public Global Data
 *
 ***************************************************/

/***************************************************
 *
 * Private data types.
 *
 ***************************************************/

#define PD_MM_TYPE "pd_mmap"
#define PD_MMN_TYPE "pd_mmap_name"

typedef struct pd_filemap {
	int		fd;
	u_int32_t	flags;	/* our flags	*/
#ifdef WIN32
	HANDLE		fh;	/* File HANDLE		*/
	HANDLE		mh;	/* Mapping HANDLE	*/
	DWORD		mprot;	/* fProtect flags	*/
	DWORD		macc;	/* dwDesiredAccess val	*/
	char		*name;	/* Mapping Name		*/
#else
	u_int32_t	mprot;	/* mmap prot	*/
	u_int32_t	mflags;	/* mmap flags	*/
#endif
	char		*addr;
	size_t		fsize;
	size_t		msize;
	off_t		offset;
} pd_filemap;

/***************************************************
 *
 * Public APIs
 *
 ***************************************************/

/*
 * Create a file memory mapping.  
 * The mapping must start at the beginning of the 
 * file for portability (Win32).
 */
pd_mmap
pd_mmap_fd(int fd, size_t len, off_t off, u_int32_t pflags, const char *name)
{
	pd_mmap		mmd;
	struct stat	sb;
	int		bn;
	const static char	zero = '\0';

	/*
	 * We'll need this in a bit but do it now for easier error handling.
	 */
	if (fd < 0) {
#ifdef WIN32
		errno = EINVAL;
		return(NULL);
#endif
	} else {
		if (fstat(fd, &sb) != 0) {
			return(NULL);
		}
	}

	/*
	 * Get our tracking memory.
	 */
	mmd = CALLOC(PD_MM_TYPE, sizeof(*mmd), 1);
	if (mmd == NULL) {
		return(NULL);
	}

	mmd->fd = fd;
	mmd->offset = off;
	mmd->flags = pflags;
	mmd->msize = len;
	mmd->fsize = sb.st_size;

	/*
	 * If size is > the filesize we extend the file to match.
	 * We seek to one byte short of what we need and then write 1
	 * byte at the end to force at least file allocation if not 
	 * block allocation.
	 */
	bn = len > sb.st_size;
	if (bn > 0 && fd >= 0) {
		lseek(fd, SEEK_SET, len -1);
		if (write(fd, &zero, 1) < 1) {
			goto fail;
		}
		mmd->fsize = len;
	}
#ifdef WIN32


	/*
	 * In Win32 we use CreateFileMapping() which requires
	 * the file handle.
	 */
	mmd->fh = (HANDLE) _get_osfhandle(fd);
	if (mmd->fh == INVALID_HANDLE_VALUE) {
		goto fail;
	}

	/*
	 * Setup our flags.
	 */
	if (mmd->flags & PD_MAP_PRIVATE) {
		mmd->mprot = PAGE_WRITECOPY;
		mmd->macc = FILE_MAP_COPY;
	} else if (mmd->flags & PD_PROT_WRITE) {
		mmd->mprot = PAGE_READWRITE;
		mmd->macc = FILE_MAP_WRITE;
	} else if (mmd->flags & (PD_PROT_READ | PD_PROT_EXEC)) {
		mmd->mprot = PAGE_READONLY;
		mmd->macc = FILE_MAP_READ;
	}
	/*
	 * Support explicit execute mapping for compat with 
	 * Windows execution protection.
	 */
#ifdef FILE_MAP_EXECUTE
	if (mmd->flags &  PD_PROT_EXEC) {
		mmd->macc = FILE_MAP_EXECUTE;
	}
#endif
	mmd->mprot |= (mmd->flags & PD_PROT_OSMASK);
	if (name != NULL) {
		mmd->name = STRDUP(PD_MMN_TYPE, name);
		if (mmd->name) {
			goto fail;
		}
	}
	mmd->mh = CreateFileMapping(mmd->fh, NULL, mmd->mprot, 0, 0, 
				    mmd->name);
	if (mmd->mh == INVALID_HANDLE_VALUE) {
		goto fail;
	}
	mmd->addr = MapViewOfFile(mmd->mh, mmd->macc, 0, 
				  mmd->offset, mmd->msize);
	if (mmd->addr == NULL) {
		goto fail;
	}
#else
	/* Manually handle the 0 means whole file option */
	if (0 == len) {
		mmd->msize = mmd->fsize;
	} else {
		mmd->msize = len;
	}

	/*
	 * Setup our flags.
	 */
	if (mmd->flags & PD_PROT_WRITE) {
		mmd->mprot = PROT_READ | PROT_WRITE;
	} else 	if (mmd->flags & (PD_PROT_READ | PD_PROT_EXEC)) {
		mmd->mprot = PROT_READ;
	} else {
		mmd->mprot = PROT_NONE;
	}
	mmd->mflags = (mmd->flags & PD_PROT_OSMASK);
	mmd->mflags |= fd < 0 ? MAP_ANON : MAP_SHARED;
	mmd->addr = mmap(NULL, mmd->msize, mmd->mprot, mmd->flags, 
			 mmd->fd, mmd->offset);
	if (mmd->addr == NULL) {
		goto fail;
	}
#endif
	return(mmd);

 fail:
	pd_mmap_cleanup(&mmd, 0);
	return(NULL);
}

pd_mmap
pd_mmap_file(const char *path, size_t len, off_t off, 
	     u_int32_t pflags, int mode)
{
	int	fflags = 0;
	int	fd;
	pd_mmap	pm;

	/*
	 * Many arches don't support write without read and will give an 
	 * error, so just make this standard behavior.
	 */
	if (pflags & PD_PROT_WRITE) {
		fflags |= O_RDWR;
	} else 	if (pflags & (PD_PROT_READ | PD_PROT_EXEC)) {
		fflags |= O_RDONLY;
	} else {
		errno = EINVAL;
		return(NULL);
	}

#ifdef O_NOINHERIT
	fflags |= O_NOINHERIT;
#endif
	if ((fd = open(path, O_CREAT | fflags, mode)) == -1)
	{
		return(NULL);
	}
#ifndef WIN32
	(void)fcntl(fd, F_SETFD, 1);
#endif

	pm = pd_mmap_fd(fd, len, off, pflags, path);

	if (pm == NULL) {
		close(fd);
		return(NULL);
	}
	return(pm);
}


/*
 * Undo a mapping (the file is left open if pd_mapfd
 */
void
pd_mmap_cleanup(pd_mmap *mmdp, int do_close)
{
	pd_mmap		mmd;

	if (mmdp == NULL) {
		return;
	}

	mmd = *mmdp;
#ifdef WIN32
	if (mmd->addr != NULL) {
		UnmapViewOfFile(mmd->addr);
		mmd->addr = NULL;
	}
	if (mmd->mh != NULL) {
		CloseHandle(mmd->mh);
		mmd->mh = 0;
	}
	if (mmd->name != NULL) {
		FREE(PD_MMN_TYPE, mmd->name);
		mmd->name = NULL;
	}
	mmd->fh = INVALID_HANDLE_VALUE;
#else
	if (mmd->addr != NULL) {
		munmap(mmd->addr, mmd->msize);

		mmd->addr = NULL;
	}
#endif
	if (do_close && mmd->fd >= 0) {
		close(mmd->fd);
		mmd->fd = -1;
	}
	FREE(PD_MM_TYPE, mmd);
	*mmdp = NULL;
	return;
}

/*
 * Sync the mapping to disk.  
 *
 * Set bytes and offset to 0 to flush everything.
 * If do_async is true the flush will be synchronous if possible.
 * Some platforms are always sync or async.
 */
int
pd_mmap_sync(pd_mmap mh, off_t offset, size_t bytes, int do_async)
{
#ifndef WIN32
	int flags = 0;
#endif
	if (mh->addr == NULL) {
		errno = EINVAL;
		return(-1);
	}
	if (bytes == 0) {
		if (mh->msize == 0) {
			bytes = mh->fsize;
		} else {
			bytes = mh->msize;
		}
	}
#ifdef WIN32
	return(!FlushViewOfFile(mh->addr + offset, bytes));
#else
#ifdef MAP_SYNC
	flags = do_async ? MAP_ASYNC : MAP_SYNC;
#endif
	return(msync(mh->addr + offset, bytes, flags));
#endif
}

/*
 * Get info about the mapping.
 */
char *
pd_mmap_getaddr(pd_mmap mapping)
{
	if (mapping != NULL) {
		return(mapping->addr);
	}
	return(NULL);
}

size_t
pd_mmap_getsize(pd_mmap mh)
{
	if (mh != NULL) {
		if (mh->msize == 0) {
			return(mh->fsize);
		} else {
			return(mh->msize);
		}
	}
	return(0);
}


int
pd_mmap_getfd(pd_mmap mapping)
{
	if (mapping != NULL) {
		return(mapping->fd);
	}
	return(-1);
}

PD_HANDLE
pd_mmap_getmh(pd_mmap mapping)
{
#ifdef WIN32
	if (mapping != NULL && mapping->mh != INVALID_HANDLE_VALUE) {
		return(mapping->mh);
	}
#endif
	return(PD_INVALID_HANDLE_VALUE);
}

