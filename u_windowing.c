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
 * string position
 */
Uint x_disp = 0, y_disp = 0;

/*
 * characters on screen
 */
char _user_chars[ WIN_CHAR_RES_X * WIN_CHAR_RES_Y ];

/*
 * Userspace frame buffer
 */
Uint32 *_user_buf = NULL;

/* 
 * Window for this user
 */
Window _user_win = -1;

/*
 * Flags
 */
Uint _user_flags = 0;

/*
 * framebuffer addr
 */
Uint32* _framebuffer = NULL;

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
		s_map_framebuffer( &_framebuffer );
		_user_buf = malloc(WINDOW_WIDTH*WINDOW_HEIGHT*sizeof(Uint32));

		int i, j;
		for( i = 0; i < WIN_CHAR_RES_X; i++ )
		{
			for( j = 0; j < WIN_CHAR_RES_Y; j++ )
			{
				// space by default
				_user_chars[i + WIN_CHAR_RES_X*j] = ' ';
			}
		}
	}

	windowing_set_char_pos( 0, 0 );
	return status;
}

/*
 * windowing_cleanup:		Cleans up windowing data for process exit
 */
void windowing_cleanup(void)
{
	if( _user_win >= 0 )
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
	if( _user_flags & WIN_AUTO_FLIP )
		return;

	windowing_flip_rect( 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT );

	int i, j;
	for( i = 0; i < WIN_CHAR_RES_X; i++ )
	{
		for( j = 0; j < WIN_CHAR_RES_Y; j++ )
		{
			_draw_char( _user_chars[i+WIN_CHAR_RES_X*j] , i*(CHAR_WIDTH+WIN_FONT_SCALE), 
					j*(CHAR_HEIGHT+WIN_FONT_SCALE), WINDOW_WIDTH, WINDOW_HEIGHT, 
					WIN_FONT_SCALE, 255, 255, 255, &windowing_draw_pixel );
		}
	}
}

/*
 * windowinng_flip_rect:	Flip part of the user frame buffer to video memory
 */
void windowing_flip_rect( Uint x, Uint y, Uint w, Uint h )
{
	int i, j;
	if( !(_user_flags & WIN_AUTO_FLIP) &&  _user_win >= 0 && 
			x < WINDOW_WIDTH && y < WINDOW_HEIGHT && 
			x+w <= WINDOW_WIDTH && y+h <= WINDOW_HEIGHT )
	{
		if( _user_flags & WIN_SYSCALL )
		{
			s_windowing_copy_rect( _user_win, x, y, w, h, (Uint8*)_user_buf );
		}
		else
		{
			Uint fx = x+X_START(_user_win);
			Uint fy = y+Y_START(_user_win);

			for( i = 0; i < w; i++ )
			{
				for( j = 0; j < h; j++ )
				{
					_framebuffer[ fx+i + SCREEN_WIDTH*(fy+j) ] = 
						_user_buf[ x+i + WINDOW_WIDTH*(y+j) ];
				}
			}
		}
	}
}

/*
 * windowing_clear_screen:	Clear the frame buffer to the specified value
 */
void windowing_clear_screen(Uint8 r, Uint8 g, Uint8 b)
{
	int x, y;
	if( _user_win >= 0 )
	{
		for( x = 0; x < WINDOW_WIDTH; x++ )
		{
			for( y = 0; y < WINDOW_HEIGHT; y++ )
			{
				windowing_draw_pixel( x, y, r, g, b );
			}
		}
	}
}

/*
 * windowing_draw_pixel:	Draw a pixel onto the user frame buffer
 */
void windowing_draw_pixel(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b)
{
	if( _user_win >= 0 )
	{
		if( x < WINDOW_WIDTH && y < WINDOW_HEIGHT )
		{
			Uint32 *pixel = &_user_buf[x + WINDOW_WIDTH*y];
			*pixel = b | g<<8 | r<<16;

			Uint fx = x+X_START(_user_win);
			Uint fy = y+Y_START(_user_win);

			if( _user_flags & WIN_AUTO_FLIP )
				_framebuffer[ fx + SCREEN_WIDTH*(fy) ] = 
					_user_buf[ x + WINDOW_WIDTH*(y) ];

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

#define abs(x)		( (x)>0 ? (x) : -(x) )
#define min(x, y)	( (x)<(y) ? (x) : (y) )
/*
 * windowing_draw_line:		Draw a line onto the user frame buffer
 */
void windowing_draw_line(Uint x0_u, Uint y0_u, Uint x1_u, Uint y1_u, Uint8 r, Uint8 g, Uint8 b)
{
	if( _user_win >= 0 )
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
	}
}

/*
 * windowing_print_char:	Draw a character onto the user frame buffer
 */
void windowing_print_char(const char c)
{
	if( _user_win >= 0 )
	{
		if( x_disp < WINDOW_WIDTH && y_disp < WINDOW_HEIGHT )
		{
			if( _user_chars[x_disp + WIN_CHAR_RES_X*y_disp] != c ) 
			{
				_user_chars[x_disp + WIN_CHAR_RES_X*y_disp] = c;

				_draw_char( c, x_disp*(CHAR_WIDTH+WIN_FONT_SCALE), y_disp*(CHAR_HEIGHT+WIN_FONT_SCALE),
						WINDOW_WIDTH, WINDOW_HEIGHT, WIN_FONT_SCALE, 255, 255, 255, &windowing_draw_pixel );
			}

			// increment display position
			x_disp += 1;
			if( x_disp  >= WIN_CHAR_RES_X || c == '\r' || c == '\n' )
			{
				x_disp = 0;
				y_disp += 1;

				if( y_disp >= WIN_CHAR_RES_Y )
				{
					y_disp = WIN_CHAR_RES_Y - 1;

					// scroll text up
					int i;
					for( i = 0; i < WIN_CHAR_RES_X*WIN_CHAR_RES_Y - WIN_CHAR_RES_X; i++ )
					{
						_user_chars[i] = _user_chars[i+WIN_CHAR_RES_X];
						_draw_char( c, (i%WIN_CHAR_RES_X)*(CHAR_WIDTH+WIN_FONT_SCALE), 
								(i/WIN_CHAR_RES_X)*(CHAR_HEIGHT+WIN_FONT_SCALE),
								WINDOW_WIDTH, WINDOW_HEIGHT, WIN_FONT_SCALE, 
								255, 255, 255, &windowing_draw_pixel );
					}
					for( i = 0; i < WIN_CHAR_RES_X; i++ )
					{
						_user_chars[i + WIN_CHAR_RES_X*y_disp] = ' ';
						_draw_char( c, i*(CHAR_WIDTH+WIN_FONT_SCALE), 
								y_disp*(CHAR_HEIGHT+WIN_FONT_SCALE),
								WINDOW_WIDTH, WINDOW_HEIGHT, WIN_FONT_SCALE, 
								255, 255, 255, &windowing_draw_pixel );
					}

					windowing_flip_screen();
				}
			}
		}
		else
		{
			// reset
			x_disp = y_disp = 0;
		}
	}
}

/*
 * windowing_print_str:	Draw a string onto the user frame buffer
 */
void windowing_print_str(const char *str)
{
	if( _user_win >= 0 )
	{
		// pointer into the string
		const char *c;
		for( c = str; *c != '\0'; c++ )
		{
			windowing_print_char(*c);
		}
	}
}

/*
 * windowing_set_char_pos:	Set the position for characters
 */
void windowing_set_char_pos( Uint x, Uint y )
{
	x_disp = x;
	y_disp = y;

	if( x_disp > WIN_CHAR_RES_X ) x_disp = WIN_CHAR_RES_X - 1;
	if( y_disp > WIN_CHAR_RES_Y ) y_disp = WIN_CHAR_RES_Y - 1;
}

/*
 * windowing_move_char_pos:	Set the position for characters
 */
void windowing_move_char_pos( int x, int y )
{
	if( (int)x_disp + x > WIN_CHAR_RES_X )	x_disp = WIN_CHAR_RES_X - 1;
	else if( (int)x_disp + x < 0 ) 			x_disp = 0;
	else									x_disp += x;

	if( (int)y_disp + y > WIN_CHAR_RES_Y )	y_disp = WIN_CHAR_RES_Y - 1;
	else if( (int)y_disp + y < 0 ) 			y_disp = 0;
	else									y_disp += y;
}
