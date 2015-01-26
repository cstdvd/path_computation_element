
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_SYS_FS_MOUNT_H_
#define _PDEL_SYS_FS_MOUNT_H_

__BEGIN_DECLS

/*
 * Mount a filesystem.
 *
 * Flags are defined in the mount(2) man page.
 */
extern int	fs_mount(const char *where, int flags);

/*
 * Unmount a filesystem.
 *
 * Flags are defined in the mount(2) man page.
 */
extern int	fs_unmount(const char *where, int flags);

__END_DECLS

#endif	/* _PDEL_SYS_FS_MOUNT_H_ */

