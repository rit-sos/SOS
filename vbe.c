/*
** File:	vbe.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE protected mode interface 
*/
#define	__KERNEL__20113__

#include "vbe.h"
#include "vbe_structs.h"
#include "bios_probe.h"
#include "c_io.h"

/* 
 * Memory areas for VBE
 */
char BIOSDataSeg[600];
char BIOSStackSeg[1024];


/* 
 * _vbe_init()
 * 
 * Try to find the protected mode interface for vbe
 * if found initialize it and set up a video mode
 */
void _vbe_init()
{
	Vbe_PMID *pmid;
	int i;

	if( (pmid = bios_probe) != NULL )
	{
		// found it, inititalize now
		
		// clear the BIOS data area
		for( i = 0; i < sizeof(BIOSDataSel); i++ )
		{
			BIOSDataSel[i] = 0;
		}

		// set the data area
		pmid.BIOSDataSel = (BIOSDataSeg / 16);

		// codeSegSel
		pmid.CodeSegSel = (PMBios / 16);
		
		// in protected mode
		pmid.InProtectMode = 1;
	}
	else
	{
		// Failed
		c_printf("VBE probe failed\n");
	}
}

/*
 * Set the vbe mode to closest mode to the one specified
 */
void _vbe_modeset(Uint w, Uint h);


