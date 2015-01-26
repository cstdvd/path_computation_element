/*
 * PD All Test  
 */

#include "pdel/pd_all.h"
#ifdef BUILDING_PDEL
#include "pdel/pd_all_p.h"
#endif

#include <stdio.h>
#include <stdlib.h>
int
main (int argc, char *argv[])
{
	printf("All built.");
	exit(0);
}
