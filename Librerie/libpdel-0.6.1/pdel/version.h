
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_VERSION_H_
#define _PDEL_VERSION_H_

/*
 * Numerically increasing version number. Each 'component' of the
 * version (major, minor, patchlevel) is allocated three digits.
 *
 * The leading '1' is to make the number be decimal and should be ignored.
 */
#define PDEL_VERSION		1000006001

/*
 * Version number as a string.
 */
#define PDEL_VERSION_STRING	"0.6.1"

#define PDEL_LIB_MAJOR		0
#define PDEL_LIB_MINOR		6
#define PDEL_LIB_PATCH		1	

/*
 * Shared lib versions.
 */
#define PDEL_SHLIB_MAJOR	5
#define PDEL_SHLIB_MINOR	1

#endif	/* _PDEL_VERSION_H_ */

