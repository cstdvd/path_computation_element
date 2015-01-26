
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _PDEL_STRUCTS_XML_H_
#define _PDEL_STRUCTS_XML_H_

#ifdef BUILDING_PDEL

#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

#ifdef NEED_FUNOPEN
#define PD_STDIO_OVERRIDE 1
#include "pdel/pd_stdio.h"
#endif

#endif

/*
 * Callback function type used by structs_xml_input()
 */
typedef void	structs_xmllog_t(int sev, const char *fmt, ...);

/* Special pre-defined loggers for structs_xml_input() */
#define STRUCTS_LOGGER_NONE	((structs_xmllog_t *)0)	    /* discard output */
#define STRUCTS_LOGGER_STDERR	((structs_xmllog_t *)1)	    /* log to stderr */
#define STRUCTS_LOGGER_ALOG	((structs_xmllog_t *)2)	    /* log to alog() */

/* Flags to structs_xml_input() */
#define STRUCTS_XML_UNINIT	0x0001	/* data object needs initialization */
#define STRUCTS_XML_LOOSE	0x0002	/* unknown tags, nested attrib. ok */
#define STRUCTS_XML_SCAN	0x0004	/* don't try to decode data structure */
#define STRUCTS_XML_COMB_TAGS	0x0008	/* allow combined tags */

/* Flags to structs_xml_output() */
#define STRUCTS_XML_FULL	0x0001	/* even output default values */

__BEGIN_DECLS

/*
 * Create a data structure instance from XML input.
 *
 * The XML document element must match "elem_tag" and may have attributes.
 * No other tags may have attributes.
 *
 * If "attrp" is not NULL, any attributes associated with "elem_tag"
 * are stored there in a string allocated with memory type "attr_mtype".
 * Attributes are stored in a single buffer like so:
 *
 *	name1 '\0' value1 '\0'
 *	name2 '\0' value2 '\0'
 *		...
 *	nameN '\0' valueN '\0'
 *	'\0'
 *
 * It is assumed that "data" points to enough space to hold an item
 * of type "type".
 *
 * If successful (only), "data" must eventually be freed e.g., by calling
 * structs_free(type, NULL, data).
 *
 * "flags" bits are defined above. STRUCTS_XML_UNINIT means that the item
 * is not initialized and must be initialized first; otherwise it is
 * assumed to be already initialized and only those subfields specified
 * in the XML will be changed. STRUCTS_XML_LOOSE means any XML tags that
 * are unrecognized or nested (non-top level) attributes cause a warning
 * to be emitted but are otherwise ignored and non-fatal. Without it, these
 * will cause a fatal error.
 *
 * Returns 0 if successful, otherwise -1 and sets errno.
 */
extern int	structs_xml_input(const struct structs_type *type,
			const char *elem_tag, char **attrp,
			const char *attr_mtype, FILE *fp, void *data,
			int flags, structs_xmllog_t *logger);

/*
 * Output a data structure as XML.
 *
 * The XML document element is an "elem_tag" element with attributes
 * supplied by "attrs" (if non NULL) as described above.
 *
 * If "elems" is non-NULL, it must point to a NULL terminated list of
 * elements to output. Only elements appearing in the list are output.
 *
 * If STRUCTS_XML_FULL is not included in flags, then elements are omitted
 * if they are equal to their initialized (i.e., default) values.
 *
 * Returns 0 if successful, otherwise -1 and sets errno.
 */
extern int	structs_xml_output(const struct structs_type *type,
			const char *elem_tag, const char *attrs,
			const void *data, FILE *fp, const char **elems,
			int flags);

__END_DECLS

#endif	/* _PDEL_STRUCTS_XML_H_ */

