/*
** File:	vbe.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE protected mode interface 
*/
#ifndef VBE_H
#define VBE_H

#include "headers.h"

#ifndef __ASM__20113__
/* 
 * _vbe_init()
 * 
 *	Grab structures out of the bootstrap and clear the display memory
 */
void _vbe_init(void);

/*
 * _vbe_framebuffer_addr
 * _vbe_framebuffer_size
 *
 * Get data about the linear frame buffer
 */
void* _vbe_framebuffer_addr(void);
Uint _vbe_framebuffer_size(void);

/* 
 * _vbe_clear_display(color)
 * 
 * clear the display to the specified color
 */
void _vbe_clear_display(Uint8 r, Uint8 g, Uint8 b);

/* 
 * _vbe_draw_pixel(x, y, color)
 * 
 * set the pixel to the specified color
 */
void _vbe_draw_pixel(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b);

/*
 * _vbe_write_str(x, y, color, str)
 *
 * Write string at specified position
 */
void _vbe_write_str(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char* str );

/*
 * _vbe_write_char(x, y, color, char)
 *
 * Write char at specified position
 */
void _vbe_write_char(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char c );

/*
 * _vbe_char_scroll
 *
 * Scroll the text display, starting at scroll_min_y up lines number of lines
 */
void _vbe_char_scroll(Uint scroll_min_y, Uint scroll_max_y, Uint lines);

#endif


#endif
