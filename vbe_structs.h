/*
** File:	vbe_structs.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE protected mode interface 
*/
#ifndef VBE_STRUCTS_H
#define VBE_STRUCTS_H

#include "bios_probe.h"

typedef struct vbe_PMID
{
	char	ID[4];
	short	EntryPoint;
	short	PMInitialize;
	short	BIOSDataSel;
	short	A0000Sel;
	short	B0000Sel;
	short	B8000Sel;
	short	CodeSegSel;
	char	InProtectMode;
	char	checksum;
} Vbe_PMID;


#endif
