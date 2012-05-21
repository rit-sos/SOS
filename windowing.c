/*
** File:	windowing.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	Kernel level windowing functionality
*/

#define __KERNEL__20113__

#include "headers.h"
#include "windowing.h"
#include "vbe.h"

#define abs(x)			( (x) > 0 ? (x) : (-(x)) )


/* common precondition check for all functions */
#define IF_WINDOW_INVALID(win) \
	if( win == -1 || win_owners[win] == -1 ) 

// Pid of the owningn process of each window
Int32 win_owners[WIN_DIV_X * WIN_DIV_Y];

/*
 * _windowing_init()
 *
 * Initialize the windowing sytstem
 */
void _windowing_init(void)
{
	int i, j;

	// mark windows as unowned
	for( i = 0; i < WIN_DIV_X * WIN_DIV_Y; i++ )
	{
		win_owners[i] = -1;
	}

	// draw the dividers
	_vbe_clear_display(0, 0, 0);

	// Horizontal
	for( i = 0; i < _vbe_get_width(); i++ )
	{
		for( j = 1; j < WIN_DIV_Y ; j++ )
		{
			_vbe_draw_pixel(i, j * _vbe_get_height() / WIN_DIV_Y, 127, 127, 127);
			_vbe_draw_pixel(i, (j * _vbe_get_height() / WIN_DIV_Y)-1, 127, 127, 127);
		}
	}

	// Vertical
	for( j = 0; j < _vbe_get_height(); j++ )
	{
		for( i = 1; i < WIN_DIV_X ; i++ )
		{
			_vbe_draw_pixel(i * _vbe_get_width() / WIN_DIV_X, j, 127, 127, 127);
		}
	}
}


/*
 * _windowing_get_window(Pid)
 *
 * Mark a window as being used by the specified process
 */
Window _windowing_get_window( Pid pid )
{
	int i;
	for( i = 0; i < WIN_DIV_X * WIN_DIV_Y; i++ )
	{
		if( win_owners[i] == -1 )
		{
			win_owners[i] = pid;
			_windowing_clear_display(i, 0, 0, 0);
			return i;
		}
	}
	return -1;
}

/*
 * _windowing_free_window(Window)
 *
 * Mark the specified window as being free
 */
void _windowing_free_window( Window win )
{
	_windowing_clear_display(win, 0, 0, 0);
	win_owners[win] = -1;
}

/*
 * _windowing_free_by_pid(Pid pid)
 *
 * Free windows from the specified pid
 */
void _windowing_free_by_pid( Pid pid )
{
	int i;
	for( i = 0; i < WIN_DIV_X * WIN_DIV_Y; i++ )
	{
		if( win_owners[i] == pid )
		{
			_windowing_free_window(i);
		}
	}
}

/*
 * _windowing_write_char(Window, x, y, color, c)
 *
 * Write a character at (x,y) in the specified window
 */
void _windowing_write_char(Window win, Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char c )
{
	IF_WINDOW_INVALID(win)
		return;

	// pass it off to vbe offset to be in the window
	_vbe_write_char_win(x, y, X_START(win), Y_START(win), r, g, b, c);
}
/*
 * _windowing_write_str(Window, x, y, color, str)
 *
 * Write a string at (x,y) in the specified window
 */
void _windowing_write_str(Window win, Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char *str )
{
	IF_WINDOW_INVALID(win)
		return;

	// current location to display at
	Uint x_disp = x, y_disp = y;

	// pointer into the string
	const char *c;
	for( c = str; *c != '\0'; c++ )
	{
		_windowing_write_char(win, x_disp, y_disp, r, g, b, *c);

		// increment display position
		x_disp += 1;
		if( x_disp  >= WIN_CHAR_RES_X )
		{
			x_disp = 0;
			y_disp += 1;

			if( y_disp >= WIN_CHAR_RES_Y )
			{
				y_disp = 0;
			}
		}
	}
}

/* 
 * _windowing_get_char(Window, x, y)
 *
 * Get the character displayed at this position
 */
char _windowing_get_char(Window win, Uint x, Uint y)
{
	IF_WINDOW_INVALID(win)
		return '\0';

	return _vbe_get_char_win(x, y, X_START(win), Y_START(win));
}

/*
 * _windowing_char_scroll(Window, min_y, max_y, lines)
 *
 * Scroll lines upward on the screen
 */
void _windowing_char_scroll(Window win, Uint min_y, Uint max_y, Uint lines)
{
	IF_WINDOW_INVALID(win)
		return;

	int i, j;

	// boundry conditions
	if( max_y > WIN_CHAR_RES_Y ) max_y = WIN_CHAR_RES_Y;
	if( lines > max_y - min_y )	  lines = max_y - min_y;

	/* copy up as many lines as possible */
	for( j = min_y; j <= max_y - lines; j++ )
	{
		for( i = 0; i < WIN_CHAR_RES_X; i++ )
		{
			_windowing_write_char(win, i, j, 255, 255, 255, _windowing_get_char(win, i, j+lines));
		}
	}

	/* fill the rest with spaces */
	for( ; j <= max_y; j++ )
	{
		for( i = 0; i < WIN_CHAR_RES_X; i++ )
		{
			_windowing_write_char(win, i, j, 255, 255, 255, ' ');
		}
	}
}

/*
 * _windowing_clear_display(win, color)
 *
 * Clear this window
 */
void _windowing_clear_display( Window win, Uint8 r, Uint8 g, Uint8 b )
{
	IF_WINDOW_INVALID(win)
		return;

	int i, j;

	for( i = 0; i < WINDOW_WIDTH; i++ )
	{
		for( j = 0; j < WINDOW_HEIGHT; j++ )
		{
			_vbe_draw_pixel(i + X_START(win), j + Y_START(win), r, g, b);
		}
	}
}

/*
 * _windowing_draw_pixel( win, x, y, color )
 *
 * Draw the specified pixel in this window
 */
void _windowing_draw_pixel( Window win, Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b )
{
	IF_WINDOW_INVALID(win)
		return;

	if( x < WINDOW_WIDTH && y < WINDOW_HEIGHT )
	{
		_vbe_draw_pixel(x + X_START(win), y + Y_START(win), r, g, b);
	}
}

/**************************************
 * swap the two numbers
 *************************************/
inline void swap( int *x, int *y )
{
	int t = *x;
	*x = *y;
	*y = t;
}

/**
 * draws a line from (x0, y0) to (x1, y1)
 */
void _windowing_draw_line ( Window win, Uint x0_u, Uint y0_u, Uint x1_u, Uint y1_u, Uint8 r, Uint8 g, Uint8 b )
{
	IF_WINDOW_INVALID(win)
		return;

	int x0 = x0_u;
	int x1 = x1_u;
	int y0 = y0_u;
	int y1 = y1_u;

	int column = 0;
	int negative = 0;

	int dx, dy;
	int d;
	int NE, E;
	int x, y;

	// go column wise
	if( abs(y1-y0) > abs(x1-x0) )
	{
		column = 1;

		swap(&x0, &y0);
		swap(&x1, &y1);
	}

	// swap end points
	if( y0 > y1 )
	{
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	// is slope negative
	negative = x1 < x0;

	// now draw the line
	dy = y1-y0;
	dx = abs(x1-x0);

	NE = 2*(dy-dx);
	E = 2*dy;

	d = 2*dy-dx;

	y = y0;
	for( x = x0 ; negative ? x >= x1 : x <= x1; negative ? x-- : x++ ) 
	{
		// draw point
		if( !column )
			_windowing_draw_pixel(win, x, y, r, g, b);
		else
			_windowing_draw_pixel(win, y, x, r, g, b);

		if( d > 0 )
		{
			// choose NE pixel
			y++;
			d+=NE;
		}
		else
		{
			// choose E pixel
			d+=E;
		}

	}
}

/**
 * Copy rect from user framebuffer to video memory
 */
void _windowing_copy_rect( Window win, Uint x0, Uint y0, Uint w, Uint h, Uint8 *buf )
{
	IF_WINDOW_INVALID(win)
		return;

	Uint x, y;
	for( x = x0; x < x0+w; x++ )
	{
		for( y = y0; y < y0+h; y++ )
		{
			_windowing_draw_pixel( win, x, y, 
					buf[4*(x+WINDOW_WIDTH*y) + 2],
					buf[4*(x+WINDOW_WIDTH*y) + 1],
					buf[4*(x+WINDOW_WIDTH*y) + 0] 
					);
		}
	}

	return;
}

/**
 * get start of the row for this user
 */
Uint32* _windowing_get_row_start( Window win, Uint y )
{
	if( y > WINDOW_HEIGHT )
		return NULL;
	return _vbe_get_row_start( y+Y_START(win) ) + X_START(win);
}

