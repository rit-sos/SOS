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
 * windowing_draw_pixel:	Draw a pixel onto the user frame buffer
 */
void windowing_draw_pixel(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b);

/*
 * windowing_draw_line:		Draw a line onto the user frame buffer
 */
void windowing_draw_line(Uint x0, Uint y0, Uint x1, Uint y1, Uint8 r, Uint8 g, Uint8 b);

/*
 * windowing_print_char:	Draw a character onto the user frame buffer
 */
void windowing_print_char(Uint x, Uint y, const char c);

/*
 * windowing_print_str:	Draw a string onto the user frame buffer
 */
void windowing_print_str(Uint x, Uint y, const char *c);

