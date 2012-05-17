/*
** File:	u_windowing.c
**
** Author:	James Letendre
**
** Description:	User-level windowing library
*/
#include "u_windowing.h"
#include "graphics_font.h"

/*
 * Userspace frame buffer
 */
Uint32 *_user_buf = (void*)0; //[WINDOW_WIDTH*WINDOW_HEIGHT];

/* 
 * Window for this user
 */
Window _user_win = -1;

/*
 * Flags
 */
Uint _user_flags = 0;

/*
 * windowing_init:			Initialize user windowing
 *
 * returns:
 * 	SUCCESS if a window handle was obtained
 * 	FAILURE otherwise
 */
Status windowing_init( Uint flags )
{
	Status status = s_windowing_get_window( &_user_win );

	_user_flags = flags;

	if( status == SUCCESS )
	{
		_user_buf = malloc(WINDOW_WIDTH*WINDOW_HEIGHT*sizeof(Uint32));
	}
	return status;
}

/*
 * windowing_cleanup:		Cleans up windowing data for process exit
 */
void windowing_cleanup(void)
{
	if( _user_win > 0 )
	{
		/* clear display and then free the window */
		s_windowing_clearscreen( _user_win, 0, 0, 0 );
		s_windowing_free_window( _user_win );
	}
}

/*
 * windowinng_flip_screen:	Flip the user frame buffer to video memory
 */
void windowing_flip_screen(void)
{
	if( _user_win > 0 )
	{
		s_windowing_copy_rect( _user_win, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Uint8*)_user_buf );
	}
}

/*
 * windowing_draw_pixel:	Draw a pixel onto the user frame buffer
 */
void windowing_draw_pixel(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b)
{
	if( _user_win > 0 )
	{
		if( x < WINDOW_WIDTH && y < WINDOW_HEIGHT )
		{
			Uint32 *pixel = &_user_buf[x + WINDOW_WIDTH*y];
			*pixel = b | g<<8 | r<<16;
		}
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

#define abs(x)	( (x)>0 ? (x) : -(x) )
/*
 * windowing_draw_line:		Draw a line onto the user frame buffer
 */
void windowing_draw_line(Uint x0_u, Uint y0_u, Uint x1_u, Uint y1_u, Uint8 r, Uint8 g, Uint8 b)
{
	if( _user_win > 0 )
	{
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
				windowing_draw_pixel(x, y, r, g, b);
			else
				windowing_draw_pixel(y, x, r, g, b);

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
		if( _user_flags & WIN_AUTO_FLIP )
			windowing_flip_screen();
	}
}

/*
 * windowing_print_char:	Draw a character onto the user frame buffer
 */
void windowing_print_char(Uint x, Uint y, const char c)
{
	if( _user_win > 0 )
	{
		if( x < WINDOW_WIDTH && y < WINDOW_HEIGHT )
		{
			_draw_char( c, x*(CHAR_WIDTH+1), y*(CHAR_HEIGHT+1),
					WINDOW_WIDTH, WINDOW_HEIGHT, 1, 255, 255, 255, &windowing_draw_pixel );

			if( _user_flags & WIN_AUTO_FLIP )
				s_windowing_copy_rect( _user_win, x*(CHAR_WIDTH+1), y*(CHAR_HEIGHT+1), 
						CHAR_WIDTH+1, CHAR_HEIGHT+1, (Uint8*)_user_buf );
		}
	}
}

/*
 * windowing_print_str:	Draw a string onto the user frame buffer
 */
void windowing_print_str(Uint x, Uint y, const char *str)
{
	if( _user_win > 0 )
	{
		// current location to display at
		Uint x_disp = x, y_disp = y;

		// pointer into the string
		const char *c;
		for( c = str; *c != '\0'; c++ )
		{
			windowing_print_char(x_disp, y_disp, *c);

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
}

