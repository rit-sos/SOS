/*
** File:	vga.c
**
** Author:	James Letendre
**
** Contributor: 
**
** Description:	Implementation of VGA drawing routines
*/

#define	__KERNEL__20113__

#include "headers.h"
#include "vga.h"
#include "c_io.h"
#include "startup.h"

/* register definitions */
#define VGA_ADDRESS_PORT		0x3CE
#define VGA_DATA_PORT			0x3CF

/* register offsets */
#define VGA_SET_RESET_REG   	0x00
#define VGA_EN_SET_RESET_REG	0x01
#define VGA_COLOR_COMPARE_REG	0x02
#define VGA_DATA_ROTATE_REG		0x03
#define VGA_READ_MAP_SEL_REG	0x04
#define VGA_GRAPHICS_MODE_REG	0x05
#define VGA_MISC_GRAPHICS_REG	0x06
#define VGA_COLOR_DONT_CARE_REG	0x07
#define VGA_BIT_MASK_REG		0x08

// for mode switch
// misc out (3c2h) value for various modes

#define R_COM  0x63 // "common" bits

#define R_W256 0x00
#define R_W320 0x00
#define R_W360 0x04
#define R_W376 0x04
#define R_W400 0x04

#define R_H200 0x00
#define R_H224 0x80
#define R_H240 0x80
#define R_H256 0x80
#define R_H270 0x80
#define R_H300 0x80
#define R_H360 0x00
#define R_H400 0x00
#define R_H480 0x80
#define R_H564 0x80
#define R_H600 0x80

#define SZ(x) (sizeof(x)/sizeof(x[0]))

static const Uint8 hor_regs [] = { 0x0,  0x1,  0x2,  0x3,  0x4, 0x5,  0x13 };

static const Uint8 width_256[] = { 0x5f, 0x3f, 0x40, 0x82, 0x4a, 0x9a, 0x20 };
static const Uint8 width_320[] = { 0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x28 };
static const Uint8 width_360[] = { 0x6b, 0x59, 0x5a, 0x8e, 0x5e, 0x8a, 0x2d };
static const Uint8 width_376[] = { 0x6e, 0x5d, 0x5e, 0x91, 0x62, 0x8f, 0x2f };
static const Uint8 width_400[] = { 0x70, 0x63, 0x64, 0x92, 0x65, 0x82, 0x32 };

static const Uint8 ver_regs  [] = { 0x6,  0x7,  0x9,  0x10, 0x11, 0x12, 0x15, 0x16 };

static const Uint8 height_200[] = { 0xbf, 0x1f, 0x41, 0x9c, 0x8e, 0x8f, 0x96, 0xb9 };
static const Uint8 height_224[] = { 0x0b, 0x3e, 0x41, 0xda, 0x9c, 0xbf, 0xc7, 0x04 };
static const Uint8 height_240[] = { 0x0d, 0x3e, 0x41, 0xea, 0xac, 0xdf, 0xe7, 0x06 };
static const Uint8 height_256[] = { 0x23, 0xb2, 0x61, 0x0a, 0xac, 0xff, 0x07, 0x1a };
static const Uint8 height_270[] = { 0x30, 0xf0, 0x61, 0x20, 0xa9, 0x1b, 0x1f, 0x2f };
static const Uint8 height_300[] = { 0x70, 0xf0, 0x61, 0x5b, 0x8c, 0x57, 0x58, 0x70 };
static const Uint8 height_360[] = { 0xbf, 0x1f, 0x40, 0x88, 0x85, 0x67, 0x6d, 0xba };
static const Uint8 height_400[] = { 0xbf, 0x1f, 0x40, 0x9c, 0x8e, 0x8f, 0x96, 0xb9 };
static const Uint8 height_480[] = { 0x0d, 0x3e, 0x40, 0xea, 0xac, 0xdf, 0xe7, 0x06 };
static const Uint8 height_564[] = { 0x62, 0xf0, 0x60, 0x37, 0x89, 0x33, 0x3c, 0x5c };
static const Uint8 height_600[] = { 0x70, 0xf0, 0x60, 0x5b, 0x8c, 0x57, 0x58, 0x70 };

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _vga_mode_switch
 *  Description: Initiate a mode switch for VGA
 * =====================================================================================
 */
Status _vga_mode_switch(int width, int height, int chain4) 
{
   const Uint8 *w,*h;
   Uint8 val;
   int a;

   switch(width) {
      case 256: w=width_256; val=R_COM+R_W256; break;
      case 320: w=width_320; val=R_COM+R_W320; break;
      case 360: w=width_360; val=R_COM+R_W360; break;
      case 376: w=width_376; val=R_COM+R_W376; break;
      case 400: w=width_400; val=R_COM+R_W400; break;
      default: return BAD_PARAM; // fail
   }
   switch(height) {
      case 200: h=height_200; val|=R_H200; break;
      case 224: h=height_224; val|=R_H224; break;
      case 240: h=height_240; val|=R_H240; break;
      case 256: h=height_256; val|=R_H256; break;
      case 270: h=height_270; val|=R_H270; break;
      case 300: h=height_300; val|=R_H300; break;
      case 360: h=height_360; val|=R_H360; break;
      case 400: h=height_400; val|=R_H400; break;
      case 480: h=height_480; val|=R_H480; break;
      case 564: h=height_564; val|=R_H564; break;
      case 600: h=height_600; val|=R_H600; break;
	  default: return BAD_PARAM; // fail
   }

   // chain4 not available if mode takes over 64k

   if(chain4 && (long)width*(long)height>65536L) return 1; 

   // here goes the actual modeswitch

   __outb(0x3c2,val);
   __outw(0x3d4,0x0e11); // enable regs 0-7

   for(a=0;a<SZ(hor_regs);++a) 
      __outw(0x3d4,(Uint16)((w[a]<<8)+hor_regs[a]));
   for(a=0;a<SZ(ver_regs);++a)
      __outw(0x3d4,(Uint16)((h[a]<<8)+ver_regs[a]));

   __outw(0x3d4,0x0008); // vert.panning = 0

   if(chain4) {
      __outw(0x3d4,0x4014);
      __outw(0x3d4,0xa317);
      __outw(0x3c4,0x0e04);
   } else {
      __outw(0x3d4,0x0014);
      __outw(0x3d4,0xe317);
      __outw(0x3c4,0x0604);
   }

   __outw(0x3c4,0x0101);
   __outw(0x3c4,0x0f02); // enable writing to all planes
   __outw(0x3ce,0x4005); // 256color mode
   __outw(0x3ce,0x0506); // graph mode & A000-AFFF

   __inb(0x3da);
   __outb(0x3c0,0x30); __outb(0x3c0,0x41);
   __outb(0x3c0,0x33); __outb(0x3c0,0x00);

   for(a=0;a<16;a++) {    // ega pal
      __outb(0x3c0,(Uint8)a); 
	  __outb(0x3c0,(Uint8)a); 
   } 
   
   __outb(0x3c0, 0x20); // enable video

   return SUCCESS;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _video_init
 *  Description: Initialize the VGA chip, put in graphics mode 
 * =====================================================================================
 */
void _vga_init ( void )
{
	// disable c_io output when in vga mode
	c_disable();

	// settings for mode 0x13
	_vga_mode_switch(VGA_WIDTH, VGA_HEIGHT, 1);
}

