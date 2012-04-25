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

typedef struct vbePMID
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
} VbePMID;

typedef struct vbeInfoBlock
{
	char			VESASignature[4];
	short			VESAVersion;
	char 			*OEMStringPtr;
	long			Capabilities;
	unsigned 		*VideoModePtr;
	short 			TotalMemory;
	char 			reserved[236];
	char			OEMData[256];
} VbeInfoBlock;

typedef struct vbeModeInfoBlock
{
	unsigned short	ModeAttributes;
	unsigned char	WinAAttributes;
	unsigned char	WinBAttributes;
	unsigned short	WinGranularity;
	unsigned short	WinSize;
	unsigned short	WinASegment;
	unsigned short	WinBSegment;
	void	 		(*WinFuncPtr)(void);
	unsigned short 	ytesPerScanLine;
	unsigned short	XResolution;
	unsigned short	YResolution;
	unsigned char	XCharSize;
	unsigned char	YCharSize;
	unsigned char	NumberOfPlanes;
	unsigned char	BitsPerPixel;
	unsigned char	NumberOfBanks;
	unsigned char	MemoryModel;
	unsigned char	BankSize;
	unsigned char	NumberOfImagePages;
	unsigned char	res1;
	unsigned char	RedMaskSize;
	unsigned char	RedFieldPosition;
	unsigned char	GreenMaskSize;
	unsigned char	GreenFieldPosition;
	unsigned char	BlueMaskSize;
	unsigned char	BlueFieldPosition;
	unsigned char	RsvdMaskSize;
	unsigned char	RsvdFieldPosition;
	unsigned char	DirectColorModeInfo;
	unsigned char	res2[216];
} VbeModeInfoBlock;

typedef enum vbeMemModels
{
	memPL	= 3,		/* planar model */
	memPK	= 4,		/* packed model */
	memRGB	= 6,		/* direct color RGB */
	memYUV	= 7			/* direct color YUV */
} VbeMemModels;

#endif
