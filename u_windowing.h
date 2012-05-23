/*
** File:	u_windowing.h
**
** Author:	James Letendre
**
** Description:	User-level windowing library
*/
#include "windowing.h"

/* flags for windowing_init */

/* automatically flip the buffer to video memory after each operation */
#define WIN_AUTO_FLIP	(1<<0)
#define WIN_FLIP_PIXEL	(1<<1)
#define WIN_SYSCALL		(1<<2)

/*
 * windowing_init:			Initialize user windowing
 *
 * returns:
 * 	SUCCESS if a window handle was obtained
 * 	FAILURE otherwise
 */
Status windowing_init( Uint flags );

/*
 * windowing_cleanup:		Cleans up windowing data for process exit
 */
void windowing_cleanup(void);

/*
 * windowinng_flip_screen:	Flip the user frame buffer to video memory
 */
void windowing_flip_screen(void);

/*
 * windowinng_flip_rect:	Flip part of the user frame buffer to video memory
 */
void windowing_flip_rect( Uint x, Uint y, Uint w, Uint h );

/*
 * windowing_clear_screen:	Clear the frame buffer to the specified value
 */
void windowing_clear_screen(Uint8 r, Uint8 g, Uint8 b);

/*
 * windowing_draw_pixel:	Draw a pixel onto the user frame buffer
 */
void windowing_draw_pixel(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b);

/*
 * windowing_draw_line:		Draw a line onto the user frame buffer
 */
void windowing_draw_line(Uint x0, Uint y0, Uint x1, Uint y1, Uint8 r, Uint8 g, Uint8 b);

/*
 * windowing_set_char_pos:	Set the position for characters
 */
void windowing_set_char_pos( Uint x, Uint y );

/*
 * windowing_move_char_pos:	Set the position for characters
 */
void windowing_move_char_pos( int x, int y );

/*
 * windowing_print_char:	Draw a character onto the user frame buffer
 */
void windowing_print_char(const char c);

/*
 * windowing_print_str:	Draw a string onto the user frame buffer
 */
void windowing_print_str(const char *c);

