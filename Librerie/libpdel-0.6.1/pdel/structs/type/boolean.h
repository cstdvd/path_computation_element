
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_BOOLEAN_H_
#define _PDEL_STRUCTS_TYPE_BOOLEAN_H_

/*********************************************************************
			BOOLEAN TYPES
*********************************************************************/

__BEGIN_DECLS

/*
 * Boolean value stored in a 'char' and output as "False" and "True"
 */
PD_IMPORT const struct structs_type	structs_type_boolean_char;

/*
 * Boolean value stored in an 'int' and output as "False" and "True"
 */
PD_IMPORT const struct structs_type	structs_type_boolean_int;

/*
 * Boolean value stored in a 'char' and output as "0" and "1"
 */
PD_IMPORT const struct structs_type	structs_type_boolean_char_01;

/*
 * Boolean value stored in an 'int' and output as "0" and "1"
 */
PD_IMPORT const struct structs_type	structs_type_boolean_int_01;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_BOOLEAN_H_ */

