/*
** File:	graphics_font.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	bitmap font for text output in graphics mode
*/

#include "graphics_font.h"

// forward declaration
Uint8 font_map[];

#define BIT(x)	(1<<(x))
void _draw_char( char c, Uint x, Uint y, Uint w, Uint h, Uint scale, Uint8 r, Uint8 g, Uint8 b, 
		void (*put_pixel)(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b) )
{
	Uint8 *c_map = GET_CHAR_FONT_PTR(c);

	// shift all fonts over 1 pixel
	x++;

	int i, j;
	int kx, ky;
	// For each pixel in the character
	for( i = 0; i < scale * CHAR_WIDTH; i+=scale )
	{
		for( j = 0; j < scale * CHAR_HEIGHT; j+= scale )
		{
			// make sure its a valid pixel location
			if( x+i+scale <= w && y+j+scale <= w )
			{
				// scale it to the set scale factor
				for( kx = 0; kx < scale; kx++ )
				{
					for( ky = 0; ky < scale; ky++ )
					{
						// draw
						if( c_map[i/scale] & BIT(j/scale) )
						{
							// if this pixel is part of the character, draw it
							put_pixel(x+i+kx, y+(CHAR_HEIGHT-(j-ky)), r, g, b);
						}
						else
						{
							// else clear to black
							put_pixel(x+i+kx, y+(CHAR_HEIGHT-(j-ky)), 0, 0, 0);
						}
					}
				}
			}
		}
	}
}


// 5x8 characters
// 5 bytes per character, 128 characters in ascii
// characters are defined column wise
Uint8 font_map[] = 
{
	//C1    C2    C3    C4    C5
	0x00, 0x00, 0x00, 0x00, 0x00,		// NUL
	0x00, 0x00, 0x00, 0x00, 0x00,		// start of heading
	0x00, 0x00, 0x00, 0x00, 0x00,		// start of text
	0x00, 0x00, 0x00, 0x00, 0x00,		// end of text
	0x00, 0x00, 0x00, 0x00, 0x00,		// end of transmission
	0x00, 0x00, 0x00, 0x00, 0x00,		// enquiry
	0x00, 0x00, 0x00, 0x00, 0x00,		// ACK
	0x00, 0x00, 0x00, 0x00, 0x00,		// BELL
	0x00, 0x00, 0x00, 0x00, 0x00,		// \b
	0x00, 0x00, 0x00, 0x00, 0x00,		// \t
	0x00, 0x00, 0x00, 0x00, 0x00,		// \n
	0x00, 0x00, 0x00, 0x00, 0x00,		// vertical tab
	0x00, 0x00, 0x00, 0x00, 0x00,		// new page
	0x00, 0x00, 0x00, 0x00, 0x00,		// \r
	0x00, 0x00, 0x00, 0x00, 0x00,		// shift out
	0x00, 0x00, 0x00, 0x00, 0x00,		// shift in
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		
	0x00, 0x00, 0x00, 0x00, 0x00,		// space
	0x00, 0x00, 0xF3, 0x00, 0x00,		// !
	0x00, 0xC0, 0x00, 0xC0, 0x00,		// "
	0x24, 0xFF, 0x24, 0xFF, 0x24,		// #
	0x00, 0x00, 0x00, 0x00, 0x00,		// $
	0x00, 0x00, 0x00, 0x00, 0x00,		// %
	0x00, 0x00, 0x00, 0x00, 0x00,		// &
	0x00, 0x00, 0x00, 0x00, 0x00,		// '
	0x00, 0x7E, 0x81, 0x00, 0x00,		// (
	0x00, 0x00, 0x81, 0x7E, 0x00,		// )
	0x00, 0x00, 0x00, 0x00, 0x00,		// *
	0x00, 0x10, 0x38, 0x10, 0x00,		// +
	0x00, 0x00, 0x00, 0x00, 0x00,		// '
	0x00, 0x10, 0x10, 0x10, 0x00,		// -
	0x00, 0x00, 0x01, 0x00, 0x00,		// .
	0x01, 0x06, 0x18, 0x60, 0x80,		// /
	0x7E, 0x81, 0x99, 0x81,	0x7E,		// 0
	0x00, 0x41, 0xFF, 0x01, 0x00,		// 1
	0x4F, 0x91, 0x91, 0x91, 0x71,		// 2
	0x42, 0x91, 0x91, 0x91, 0x6E,		// 3
	0xF0, 0x10, 0x10, 0x10, 0xFF,		// 4
	0xF1, 0x91, 0x91, 0x91, 0x86,		// 5
	0xFF, 0x91, 0x91, 0x91, 0x9F,		// 6
	0x80, 0x80, 0x80, 0x80, 0xFF,		// 7
	0x6E, 0x91, 0x91, 0x91, 0x6E,		// 8
	0xF1, 0x91, 0x91, 0x91, 0xFF,		// 9
	0x00, 0x00, 0x24, 0x00, 0x00,		// :
	0x00, 0x02, 0x26, 0x00, 0x00,		// ;
	0x00, 0x00, 0x00, 0x00, 0x00,		// <
	0x00, 0x24, 0x24, 0x24, 0x24,		// =
	0x00, 0x00, 0x00, 0x00, 0x00,		// >
	0x00, 0x00, 0x00, 0x00, 0x00,		// ?
	0x00, 0x00, 0x00, 0x00, 0x00,		// @
	0x7F, 0x90, 0x90, 0x90, 0x7F,		// A
	0xFF, 0x91, 0x91, 0x91, 0x6E,		// B
	0x7E, 0x81, 0x81, 0x81, 0x42,		// C
	0xFF, 0x81, 0x81, 0x81, 0x7E,		// D
	0xFF, 0x91, 0x91, 0x91, 0x91,		// E
	0xFF, 0x90, 0x90, 0x90, 0x80,		// F
	0x7E, 0x81, 0x89, 0x89, 0x6E,		// G
	0xFF, 0x10, 0x10, 0x10, 0xFF,		// H
	0x00, 0x81, 0xFF, 0x81, 0x00,		// I
	0x02, 0x01, 0x81, 0xFF, 0x80,		// J
	0xFF, 0x18, 0x24, 0x42, 0x81,		// K
	0xFF, 0x01, 0x01, 0x01, 0x01,		// L
	0xFF, 0x40, 0x20, 0x40, 0xFF,		// M
	0xFF, 0x60, 0x18, 0x06, 0xFF,		// N
	0x7E, 0x81, 0x81, 0x81, 0x7E,		// O
	0xFF, 0x90, 0x90, 0x90, 0x60,		// P
	0x7E, 0x81, 0x85, 0x83, 0x7F,		// Q
	0xFF, 0x98, 0x94, 0x92, 0x61,		// R
	0x62, 0x91, 0x91, 0x91, 0x4E,		// S
	0x80, 0x80, 0xFF, 0x80, 0x80,		// T
	0xFE, 0x01, 0x01, 0x01, 0xFE,		// U
	0xE0, 0x1C, 0x03, 0x1C, 0xE0,		// V
	0xFE, 0x01, 0x0F, 0x01, 0xFE,		// W
	0xC3, 0x3C, 0x18, 0x3C, 0xC3,		// X
	0xC0, 0x30, 0x0F, 0x30, 0xC0,		// Y
	0x87, 0x8D, 0x99, 0xB1, 0xE1,		// Z
	0x00, 0x00, 0xFF, 0x81, 0x00,		// [
	0x80, 0x60, 0x18, 0x06, 0x01,		/* \\ */
	0x00, 0x81, 0xFF, 0x00, 0x00,		// ]
	0x00, 0x40, 0x80, 0x40, 0x00,		// ^
	0x00, 0x01, 0x01, 0x01, 0x00,		// _
	0x00, 0x00, 0x00, 0x00, 0x00,		// `
	0x7F, 0x90, 0x90, 0x90, 0x7F,		// A
	0xFF, 0x91, 0x91, 0x91, 0x6E,		// B
	0x7E, 0x81, 0x81, 0x81, 0x42,		// C
	0xFF, 0x81, 0x81, 0x81, 0x7E,		// D
	0xFF, 0x91, 0x91, 0x91, 0x91,		// E
	0xFF, 0x90, 0x90, 0x90, 0x80,		// F
	0x7E, 0x81, 0x89, 0x89, 0x6E,		// G
	0xFF, 0x10, 0x10, 0x10, 0xFF,		// H
	0x00, 0x81, 0xFF, 0x81, 0x00,		// I
	0x02, 0x01, 0x81, 0xFF, 0x80,		// J
	0xFF, 0x18, 0x24, 0x42, 0x81,		// K
	0xFF, 0x01, 0x01, 0x01, 0x01,		// L
	0xFF, 0x40, 0x20, 0x40, 0xFF,		// M
	0xFF, 0x60, 0x18, 0x06, 0xFF,		// N
	0x7E, 0x81, 0x81, 0x81, 0x7E,		// O
	0xFF, 0x90, 0x90, 0x90, 0x60,		// P
	0x7E, 0x81, 0x85, 0x83, 0x7F,		// Q
	0xFF, 0x98, 0x94, 0x92, 0x61,		// R
	0x62, 0x91, 0x91, 0x91, 0x4E,		// S
	0x80, 0x80, 0xFF, 0x80, 0x80,		// T
	0xFE, 0x01, 0x01, 0x01, 0xFE,		// U
	0xE0, 0x1C, 0x03, 0x1C, 0xE0,		// V
	0xFF, 0x01, 0x0F, 0x01, 0xFF,		// W
	0xC3, 0x3C, 0x18, 0x3C, 0xC3,		// X
	0xC0, 0x30, 0x0F, 0x30, 0xC0,		// Y
	0x87, 0x8D, 0x99, 0xB1, 0xE1,		// Z
	0x00, 0x00, 0x00, 0x00, 0x00,		// {
	0x00, 0x00, 0xE7, 0x00, 0x00,		// |
	0x00, 0x00, 0x00, 0x00, 0x00,		// }
	0x00, 0x00, 0x00, 0x00, 0x00,		// ~
	0x00, 0x00, 0x00, 0x00, 0x00,		// DEL
};

