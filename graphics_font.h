/*
** File:	graphics_font.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description: bitmap font for graphics mode text output
*/

#ifndef VGA_FONT_H
#define VGA_FONT_H

#include "headers.h"

// Return a pointer to the start of the character in the map
// GET_CHAR_FONT_PTR('a')[0] is the first col
// GET_CHAR_FONT_PTR('a')[1] is the second col, etc
#define GET_CHAR_FONT_PTR(i)	(&(font_map[5*i]))

#define _CHAR_WIDTH		(5)
#define _CHAR_HEIGHT	(8)

// 5 bytes per character, 127 characters
//extern Uint8 font_map[];

/*
 * draw the character onto the provided buffer
 */
void _draw_char( char c, Uint x, Uint y, Uint w, Uint h, Uint scale, Uint8 r, Uint8 g, Uint8 b, 
		void (*put_pixel)(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b) );

#endif
