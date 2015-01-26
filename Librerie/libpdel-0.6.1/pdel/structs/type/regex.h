
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_TYPE_REGEX_H_
#define _PDEL_STRUCTS_TYPE_REGEX_H_

/*********************************************************************
			REGULAR EXPRESSION TYPE
*********************************************************************/

/*
 * Support for regular expressions (see regex(3)).
 *
 * The data is a "struct structs_regex". The "reg" argument is compiled
 * and filled in automatically.
 */

struct structs_regex {
	const char	*pat;			/* ascii pattern */
	regex_t		reg;			/* compiled pattern */
};

__BEGIN_DECLS

extern structs_equal_t		structs_regex_equal;
extern structs_ascify_t		structs_regex_ascify;
extern structs_binify_t		structs_regex_binify;
extern structs_uninit_t		structs_regex_free;

__END_DECLS

/*
 * Macro arguments:
 *	[const char *]		Memory allocation type for byte buffer
 *	[int]			"cflags" argument to regcomp(3)
 */
#define STRUCTS_REGEX_TYPE(mtype, flags) {				\
	sizeof(struct structs_regex),					\
	"regex",							\
	STRUCTS_TYPE_PRIMITIVE,						\
	structs_region_init,						\
	structs_ascii_copy,						\
	structs_regex_equal,						\
	structs_regex_ascify,						\
	structs_regex_binify,						\
	structs_string_encode,						\
	structs_string_decode,						\
	structs_regex_free,						\
	{ { (void *)(mtype) }, { (void *)(flags) }, { NULL } }		\
}

#define STRUCTS_REGEX_MTYPE	"structs_regex"

__BEGIN_DECLS

/* Regex with mtype STRUCTS_REGEX_MTYPE and flags REG_EXTENDED */
PD_IMPORT const struct structs_type	structs_type_regex;

/* Regex with mtype STRUCTS_REGEX_MTYPE and flags REG_EXTENDED | REG_ICASE */
PD_IMPORT const struct structs_type	structs_type_regex_icase;

__END_DECLS

#endif	/* _PDEL_STRUCTS_TYPE_REGEX_H_ */

