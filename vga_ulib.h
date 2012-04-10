/*
** File:	vga_ulib.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VGA drawing routines
*/

#ifndef _VGA_ULIB_H
#define _VGA_ULIB_H

#include "headers.h"

#define VGA_WIDTH	320
#define VGA_HEIGHT	200

/*
 * vga_draw_pixel
 *
 * 	Draw the pixel at the specified location on the screen
 */
void vga_draw_pixel(Uint16 x, Uint16 y, Uint8 color );



#endif
