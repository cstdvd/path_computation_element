/*
 * pd_base.h
 *
 * PD common declarations, library and utility functions.
 *
 * @COPYRIGHT@
 *
 * Author: Mark Gooderum <markpdel@jumpweb.com> 
 */

#ifndef __PDEL_PD_BASE_H__
#define __PDEL_PD_BASE_H__

/* Public Test */
#define PD_BASE_INCLUDED 1

/* This will give us ctype.h, sys/types.h and sys/param.h */
#ifndef PD_PORT_INCLUDED
#include "pdel/pd_port.h"
#endif

/**********************************************************************
 *
 *		PUBLIC Constants
 *
 **********************************************************************/

/**********************************************************************
 *
 *		PUBLIC Data Structures
 *
 **********************************************************************/

/**********************************************************************
 *
 *		PUBLIC Macros
 *
 **********************************************************************/

/*
 * Min/Max.
 */

#define PD_MIN(x, y)	((x) < (y) ? (x) : (y))
#define PD_MAX(x, y)	((x) > (y) ? (x) : (y))

/*
 * NULL safe string macro for printfs and such.
 */

#define PDSAFESTR(x)	((x) ? (x) : "(NULL)")
#define PDSAFESTRE(x)	((x) ? (x) : "")

/*
 * (Ordinal) Bit mask accessor macros/functions.
 * 
 * Hint: Think select()
 */

#define PDBIT(bit) (1LL << (bit))
#define PDB_SET(set, bit) ((set) |= PDBIT(bit))
#define PDB_CLR(set, bit) ((set) &= ~PDBIT(bit))
#define PDB_ISSET(set, bit) ((set) & PDBIT(bit))
#define PDB_ZERO(set) ((set) = 0)
#define PDB_FILLSET(set) ((set) = ~0)
#define PDB_BITVAL(set) pd_bitval(set)

/*
 *	Minorly	Nasty Macros
 */

#ifdef __GNUC__
/*
 * Semi-ugly.  Some versions of GCC has a "feature" with the comma operator
 * such that return(x, 0) is treated as type of x and gives an error 
 * in pointer return values from function.
 */
#define PDNULL	((void *) 0)
#define PDRET(x)	((__typeof__(x)) (x))
#else
#define	PDNULL	0
#define PDRET(x)	(x)
#endif

/*
 * A structure to provide initialization parameteres to libpdel.  
 * Defaults are to initialize everything. This structure has very little
 * for now but allows future expansion without changing the API.
 */
typedef struct pd_config {
	u_int32_t	version;	/* Version of structure		*/
	u_int32_t	flags;		/* Flags for behavior		*/
	int32_t		thread_wait;	/* msec to wait f/threads on cleanup */
} pd_config;

extern const pd_config	pd_default_config;

/*
 * Versions of the PD config structure and config defaults.  The library
 * will honor these defaults at compile time. 
 */

#define PDC_CUR_VERSION		0
#ifndef PDC_DEF_THREADWAIT
#define PDC_DEF_THREADWAIT	(-1)
#endif
#ifndef PDC_DEF_FLAGS
#define PDC_DEF_FLAGS		0
#endif
/*
 * Flags for the pd_config structure.
 *
 * For now NONET only on Win32 means don't call WASStartup().
 * For now NOBINARY only on Win32 means don't force files to binary default.
 */
#define PDC_NO_NET	PDBIT(1)	/* Don't initialize network	*/
#define PDC_NO_BINARY	PDBIT(2)	/* Don't force binary files	*/

/* Init a configuration structure */
void PD_CONFIG_INIT(pd_config *pdc);

#define PD_CONFIG_INIT(x)	(*pdc = pd_default_config) 

/**********************************************************************
 *
 *		PUBLIC Prototypes
 *
 **********************************************************************/

__BEGIN_DECLS

/*
 * Initialize the PDEL library.  NULL pdc for reasonable defaults.
 *
 */
int
pd_init(const pd_config *pdc);

/*
 * Cleanup the PDEL library.  (Eventually) terminate service threads, 
 * free internal memory if possible, etc.
 */
int
pd_cleanup(const pd_config *pdc);

/*
 * Return the bit number set in a mask, multiple bits returns
 * the low-order bit.
 */
int
pd_bitval(unsigned long long bitmask);

__END_DECLS


#endif
