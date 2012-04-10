/*
** File:	vga_test.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VGA test routine, draw some pixels
*/

#include "headers.h"

void vga_test(void)
{
	Uint16 x, y;
	Uint8 color;
	Uint16 i;

	for( i = 0; i < 256; i++ )
	{
		for( x = 0; x < VGA_WIDTH; x++ )
		{
			for( y = 0; y < VGA_HEIGHT; y++ )
			{
				color = (x+y+i)%256;
		
				vga_draw_pixel(x, y, color);
			}
		}
		msleep(100);
	}

	write_str("VGA test completed\n");

	exit();
}
