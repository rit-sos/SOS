/*
** File:	windowing.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	Kernel level windowing functionality
*/
#ifndef WINDOWING_H
#define WINDOWING_H

#include "headers.h"
#include "types.h"
#include "graphics_font.h"
#include "vbe.h"

/*
 * Number of divisions in each dimension
 */
#define WIN_DIV_X		(2)
#define WIN_DIV_Y		(2)

/*
 * Number of characters that can fit in each section
 */
#define WIN_CHAR_RES_X	(SCREEN_WIDTH/WIN_DIV_X/(CHAR_WIDTH+1))
#define WIN_CHAR_RES_Y	(SCREEN_HEIGHT/WIN_DIV_Y/(CHAR_HEIGHT+1))

/*
 * Pixel resolutions of each window
 */
#define WINDOW_WIDTH	((SCREEN_WIDTH/WIN_DIV_X)-1)
#define WINDOW_HEIGHT	(SCREEN_HEIGHT/WIN_DIV_Y)

#define X_OFFSET(win)	( (((win)%WIN_DIV_X)*WIN_CHAR_RES_X) )
#define Y_OFFSET(win)	( (((win)/WIN_DIV_X)*WIN_CHAR_RES_Y) )

#define X_START(win)	( (((win)%WIN_DIV_X)*(WINDOW_WIDTH+1)) + ((win)%WIN_DIV_X) )
#define Y_START(win)	( (((win)/WIN_DIV_X)*WINDOW_HEIGHT) + ((win)/WIN_DIV_X) )

#ifdef __KERNEL__20113__
/*
 * _windowing_init()
 *
 * Initialize the windowing sytstem
 */
void _windowing_init(void);

/*
 * _windowing_get_window(Pid)
 *
 * Mark a window as being used by the specified process
 */
Window _windowing_get_window( Pid pid );

/*
 * _windowing_free_window(Window)
 *
 * Mark the specified window as being free
 */
void _windowing_free_window( Window win );

/*
 * _windowing_free_by_pid(Pid pid)
 *
 * Free windows from the specified pid
 */
void _windowing_free_by_pid( Pid pid );

/*
 * _windowing_write_char(Window, x, y, color, c)
 *
 * Write a character at (x,y) in the specified window
 */
void _windowing_write_char(Window win, Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char c );

/*
 * _windowing_write_str(Window, x, y, color, str)
 *
 * Write a string at (x,y) in the specified window
 */
void _windowing_write_str(Window win, Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char *str );

/* 
 * _windowing_get_char(Window, x, y)
 *
 * Get the character displayed at this position
 */
char _windowing_get_char(Window win, Uint x, Uint y);

/*
 * _windowing_char_scroll(Window, min_y, max_y, lines)
 *
 * Scroll lines upward on the screen
 */
void _windowing_char_scroll(Window win, Uint min_y, Uint max_y, Uint lines);

/*
 * _windowing_clear_display(win, color)
 *
 * Clear this window
 */
void _windowing_clear_display( Window win, Uint8 r, Uint8 g, Uint8 b );

/*
 * _windowing_draw_pixel( win, x, y, color )
 *
 * Draw the specified pixel in this window
 */
void _windowing_draw_pixel( Window win, Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b );

/**
 * draws a line from (x0, y0) to (x1, y1)
 */
void _windowing_draw_line ( Window win, Uint x0_u, Uint y0_u, Uint x1_u, Uint y1_u, 
		Uint8 r, Uint8 g, Uint8 b );

/**
 * Copy rect from user framebuffer to video memory
 */
void _windowing_copy_rect( Window win, Uint x0, Uint y0, Uint w, Uint h, Uint8 *buf );

/* 
 * get address of start of row
 */
Uint32* _windowing_get_row_start( Window win, Uint y );

#endif


#endif
