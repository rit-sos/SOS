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

typedef struct vbeInfoBlock
{
	Uint8			VESASignature[4];
	Uint16			VESAVersion;
	Uint32			*OEMStringPtr;
	Uint32			Capabilities;
	Uint32	 		*VideoModePtr;
	Uint16 			TotalMemory;
	Uint8 			reserved[236];
	Uint8			OEMData[256];
} VbeInfoBlock;

typedef struct vbeModeInfoBlock
{
	Uint16	ModeAttributes;
	Uint8	WinAAttributes;
	Uint8	WinBAttributes;
	Uint16	WinGranularity;
	Uint16	WinSize;
	Uint16	WinASegment;
	Uint16	WinBSegment;
	Uint32	WinFncPtr;
	Uint16 	BytesPerScanLine;
	Uint16	XResolution;
	Uint16	YResolution;
	Uint8	XCharSize;
	Uint8	YCharSize;
	Uint8	NumberOfPlanes;
	Uint8	BitsPerPixel;
	Uint8	NumberOfBanks;
	Uint8	MemoryModel;
	Uint8	BankSize;
	Uint8	NumberOfImagePages;
	Uint8	res1;
	Uint8	RedMaskSize;
	Uint8	RedFieldPosition;
	Uint8	GreenMaskSize;
	Uint8	GreenFieldPosition;
	Uint8	BlueMaskSize;
	Uint8	BlueFieldPosition;
	Uint8	RsvdMaskSize;
	Uint8	RsvdFieldPosition;
	Uint8	DirectColorModeInfo;
	Uint32	PhysBasePtr;
	Uint8	res2[6];
	Uint16	LinBytesPerScanLine;
	Uint8	BnkNumberOfImagePages;
	Uint8	LinNumberOfImagePages;
	Uint8	LinRedMaskSize;
	Uint8	LinRedFieldPosition;
	Uint8	LinGreenMaskSize;
	Uint8	LinGreenFieldPosition;
	Uint8	LinBlueMaskSize;
	Uint8	LinBlueFieldPosition;
	Uint8	LinRsvdMaskSize;
	Uint8	LinRsvdFieldPosition;
	Uint32	MaxPixelClock;
	Uint8	res3[189];
} VbeModeInfoBlock;

typedef enum vbeMemModels
{
	memPL	= 3,		/* planar model */
	memPK	= 4,		/* packed model */
	memRGB	= 6,		/* direct color RGB */
	memYUV	= 7			/* direct color YUV */
} VbeMemModels;

#endif
