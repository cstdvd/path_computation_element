/*
 * pd_mem.h
 *
 * PD memory releated library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_MEM_H__
#define __PDEL_PD_MEM_H__

/* Public Test */
#define PD_MEM_INCLUDED 1

#ifndef PD_BASE_INCLUDED
#include "pdel/pd_base.h"	/* picks up pd_port.h */
#endif

/*
 * Mapping flags.
 */


#define PD_MAP_SHARED	   0x0010	/* changes are shared */
#define PD_MAP_PRIVATE	   0x0020	/* changes are private */

#ifndef WIN32
#include <sys/mman.h>

/*
 * Protections are chosen from these bits, or-ed together
 */
#define PD_PROT_NONE	   PROT_NONE	/* no permissions */
#define PD_PROT_READ	   PROT_READ	/* pages can be read */
#define PD_PROT_WRITE	   PROT_WRITE	/* pages can be written */
#define PD_PROT_EXEC	   PROT_EXEC	/* pages can be executed */

typedef long			PD_HANDLE;
#define PD_INVALID_HANDLE_VALUE	(-1)
#else

#include <wtypes.h>
#include <winbase.h>
#include <io.h>
#include <winnt.h>

/*
 * Protections are chosen from these bits, or-ed together
 */
#define PD_PROT_NONE	   0x00	   /* no permissions */
#define PD_PROT_READ	   0x01	   /* pages can be read */
#define PD_PROT_WRITE	   0x02	   /* pages can be written */
#define PD_PROT_EXEC	   0x04	   /* pages can be executed */

typedef HANDLE			PD_HANDLE;
#define PD_INVALID_HANDLE_VALUE	INVALID_HANDLE_VALUE

#endif


/*
 * Bits in the flags in this mask will be passed through to the 
 * underlying OS function (mmap or CreateFileMapping) for (non-portable)
 * access to OS specific features.
 */
#define PD_PROT_OSMASK (~0x7)

/*
 * An opaque handle on a file mapping.  We need a handle because on 
 * some platforms we need more than the address to manage the mapping.
 */
struct pd_filemap;

typedef struct pd_filemap *pd_mmap;

__BEGIN_DECLS

/*
 * Create a file memory mapping.  
 * The mapping must start at the beginning of the 
 * file for portability (Win32).  For similar portability reasons
 * PROT_WRITE will imply PROT_READ.
 *
 * Name is ignored on some platforms and can be passed as NULL by default, 
 * but on Win32 it's used as the mapping name.
 *
 * len of 0 means whole file, mode of 0 maps to 600.
 */
pd_mmap
pd_mmap_fd(int fd, size_t len, off_t off, u_int32_t flags, 
	  const char *name);

pd_mmap
pd_mmap_file(const char *path, size_t len, off_t off, u_int32_t flags, 
	    int mode);

/*
 * Undo a mapping (the file is left open if pd_mapfd
 */
void
pd_mmap_cleanup(pd_mmap *mapptr, int do_close);

/*
 * Sync the mapping to disk.  Set bytes to 0 to flush everything.
 *
 * Set bytes and offset to 0 to flush everything.
 * If do_sync is true the flush will be synchronous if possible.
 * Some platforms are always sync or async.
 */
int
pd_mmap_sync(pd_mmap mh, off_t offset, size_t bytes, int do_async);

/*
 * Get info about the mapping.
 */
char *
pd_mmap_getaddr(pd_mmap mapping);

size_t
pd_mmap_getsize(pd_mmap mapping);

int
pd_mmap_getfd(pd_mmap mapping);

/* Return the mapping handle on Win32, returns -1 if not valid or not Win32 */
PD_HANDLE
pd_mmap_getmh(pd_mmap mapping);

__END_DECLS

#endif
