
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_NULL_H_
#define _PDEL_STRUCTS_TYPE_NULL_H_

/*********************************************************************
			NULL TYPE
*********************************************************************/

__BEGIN_DECLS

/*
 * This type is invalid; all methods return errors.
 *
 * Use to force zero length arrays, etc.
 */
PD_IMPORT const struct structs_type	structs_type_null;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_NULL_H_ */

