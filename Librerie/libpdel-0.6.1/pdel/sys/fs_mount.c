
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <ufs/ufs/ufsmount.h>

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <fstab.h>

#include <openssl/ssl.h>

#include "sys/fs_mount.h"

/*
 * Mount a filesystem.
 */
int
fs_mount(const char *where, int flags)
{
	struct fstab *fstab;
	struct ufs_args ufs_args;
	void *mnt_args;
	int errno_save;
	int rtn = -1;

	/* Get /etc/fstab entry for mount point */
	fstab = getfsfile(where);

	/* Check for error */
	if ((fstab = getfsfile(where)) == NULL) {
		errno = ENOENT;
		goto done;
	}

	/* Set up args */
	memset(&ufs_args, 0, sizeof(ufs_args));
	ufs_args.fspec = fstab->fs_spec;
	mnt_args = &ufs_args;

	/* Try to mount */
	if (mount(fstab->fs_vfstype, where, flags, mnt_args) == 0)
		rtn = 0;

done:
	/* Clean up */
	errno_save = errno;
	endfsent();
	errno = errno_save;
	return (rtn);
}

/*
 * Unmount a filesystem.
 */
int
fs_unmount(const char *where, int flags)
{
	if (unmount(where, flags) == -1)
		return (-1);
	return (0);
}


