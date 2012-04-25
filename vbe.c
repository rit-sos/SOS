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

// for GDT offsets
#include "bootstrap.h"


/* 
 * Memory areas for VBE
 */
char BIOSDataSeg[600];
char BIOSStackSeg[1024];

/* VBE mode info */
VbeModeInfoBlock vbe_mode_info;
/* 
 * _vbe_init()
 * 
 * Try to find the protected mode interface for vbe
 * if found initialize it and set up a video mode
 */
void _vbe_init()
{
	VbePMID *pmid;
	int i;

	if( (pmid = (VbePMID*)bios_probe()) != NULL )
	{
		// found it, inititalize now
		
		// First, set up the GDT entries
		GDT_Pointer gdt;
		_vbe_getGDT( &gdt );

		void *gdt_base = (void*)gdt.base;

		// print the gdt
		c_printf("GDT: 0x%08x\n", gdt.base);
		c_printf("Limit: 0x%08x\n", gdt.limit);

		// edit the GDT entries

		// first the segements for PMBIOS
		GDT_Entry *entry = (GDT_Entry*)(gdt_base + GDT_VBE_BIOS_DATA);
		BASE(entry, PMBios);

		entry = (GDT_Entry*)(gdt_base + GDT_VBE_BIOS_CODE);
		BASE(entry, PMBios);

		// now the segment for BIOS data
		entry = (GDT_Entry*)(gdt_base + GDT_VBE_DATA);
		BASE(entry, BIOSDataSeg);

		// now the segment for the BIOS stack
		entry = (GDT_Entry*)(gdt_base + GDT_VBE_STACK);
		BASE(entry, BIOSStackSeg);

		// clear the BIOS data area
		for( i = 0; i < sizeof(BIOSDataSeg); i++ )
		{
			BIOSDataSeg[i] = 0;
		}

		// set the data area
		pmid->BIOSDataSel = GDT_VBE_DATA;

		// A0000, B0000, B8000
		pmid->A0000Sel = GDT_VBE_A0000;
		pmid->B0000Sel = GDT_VBE_B0000;
		pmid->B8000Sel = GDT_VBE_B8000;

		// codeSegSel
		pmid->CodeSegSel = GDT_VBE_BIOS_DATA;
		
		// in protected mode
		pmid->InProtectMode = 1;

		c_printf("VBE: Calling init 0x%08x\n", pmid->PMInitialize);

		// now call initialize function
		_vbe_call_init( pmid->PMInitialize );

		c_printf("VBE: Initialized\n");

		// now request info
		VbeInfoBlock vbe_info;
		vbe_info.VESASignature[0] = 'V';
		vbe_info.VESASignature[1] = 'B';
		vbe_info.VESASignature[2] = 'E';
		vbe_info.VESASignature[3] = '3';

		if( _vbe_call_func( pmid->EntryPoint, 0x4F00, 0x00, 0x00, &vbe_info, sizeof(VbeInfoBlock) ) != 0x4F00 )
			c_printf("VBE: error getting VESA info\n");

		// got info
		c_printf("Got info: Sig = %c%c%c%c\n", vbe_info.VESASignature[0], vbe_info.VESASignature[1], 
											   vbe_info.VESASignature[2], vbe_info.VESASignature[4] );

		// TODO
		for(;;);
		
	}
	else
	{
		// Failed
		c_printf("VBE probe failed\n");
	}
}

void _vbe_call_init( Uint offset )
{
	_vbe_call_func( offset, 0, 0, 0, NULL, 0 );
}

int _vbe_call_func(	Uint offset, Uint16 ax, Uint16 bx, Uint16 cx, void *data, Uint data_size )
{
	// setup the gdt
	GDT_Pointer gdt;
	_vbe_getGDT( &gdt );

	void *gdt_base = (void*)gdt.base;

	GDT_Entry *entry = (GDT_Entry*)(gdt_base+GDT_VBE_PARAM);
	BASE(entry, data);
	LIMIT(entry, data_size);

	return __vbe_call_func_(offset, ax, bx, cx, GDT_VBE_PARAM);
}

/*
 * Set the vbe mode to closest mode to the one specified
 */
void _vbe_modeset(Uint w, Uint h);


