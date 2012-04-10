/*
** File:	vga_ulib.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VGA drawing routines
*/

#include "vga_ulib.h"
#include "headers.h"

#define VGA_BASE	0xA0000
#define VGA(x,y)	( ((Uint8*)VGA_BASE)[(x)+VGA_WIDTH*(y)] )

/*
 * vga_draw_pixel
 *
 * 	Draw the pixel at the specified location on the screen
 */
void vga_draw_pixel(Uint16 x, Uint16 y, Uint8 color )
{
	// only draw to valid memory
	if( x >= VGA_WIDTH || y >= VGA_HEIGHT ) return;

	VGA(x,y) = color;
}

