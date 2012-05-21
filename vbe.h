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

#define SCREEN_WIDTH	(1280)
#define SCREEN_HEIGHT	(1024)

#if !defined(__ASM__20113__) && defined(__KERNEL__20113__)
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
 * _vbe_get_height()
 * _vbe_get_width()
 *
 * Get the screen size
 */
Uint16 _vbe_get_height(void);
Uint16 _vbe_get_width(void);

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
 * _vbe_write_char_win(x, y, x_start, y_start, color, char)
 *
 * Write char at specified position, in the window
 */
void _vbe_write_char_win(Uint x, Uint y, Uint x_start, Uint y_start, Uint8 r, Uint8 g, Uint8 b, const char c );

/*
 * _vbe_char_scroll
 *
 * Scroll the text display, starting at scroll_min_y up lines number of lines
 */
void _vbe_char_scroll(Uint scroll_min_y, Uint scroll_max_y, Uint lines);

/*
 * _vbe_get_char( x, y )
 *
 * Get the character being displayed at this point
 */
char _vbe_get_char( Uint x, Uint y );

/*
 * _vbe_get_char_win( x, y, x_start, y_start )
 *
 * Get the character being displayed at this point
 */
char _vbe_get_char_win( Uint x, Uint y, Uint x_start, Uint y_start );

/* 
 * get address of start of row
 */
Uint32* _vbe_get_row_start( Uint y );

#endif


#endif
