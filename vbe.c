/*
** File:	vbe.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE kernel space code
*/
#define	__KERNEL__20113__

#include "vbe.h"
#include "pcbs.h"
#include "windowing.h"
#include "vbe_structs.h"
#include "graphics_font.h"
#include "system.h"
#include "sio.h"
#include "mman.h"
#include "c_io.h"

// for vbe structure addresses
#include "bootstrap.h"

/* max characters possible at 1280x1024 */
#define N_CHARS	( (1280/(CHAR_WIDTH+1)) * (1024/(CHAR_HEIGHT+1)) )


/* VBE structs */
static VbeModeInfoBlock *vbe_mode_info 	= NULL;
static VbeInfoBlock	 	*vbe_info 		= NULL;

/* character display size */
static Uint _vbe_char_res_x = SCREEN_WIDTH/(CHAR_WIDTH+WIN_FONT_SCALE);
static Uint _vbe_char_res_y = SCREEN_HEIGHT/(CHAR_HEIGHT+WIN_FONT_SCALE);

/* storage for characters on display */
static char _vbe_screen_text[N_CHARS];

/* 
 * _vbe_init()
 * 
 *	Grab structures out of the bootstrap and clear the display memory
 */
void _vbe_init(void)
{
	/* get pointers to the vbe structs */
	vbe_info = (VbeInfoBlock*)VBE_INFO_ADDR;
	vbe_mode_info = (VbeModeInfoBlock*)VBE_MODE_INFO_ADDR;

	/* clear the display */
	_vbe_clear_display(0, 0, 0);

	/* clear the text storage */
	int i;
	for( i = 0; i < N_CHARS; i++ )
	{
		_vbe_screen_text[i] = ' ';
	}

	c_printf("VBE Initialization Complete\r\n");
}

/*
 * _vbe_get_height()
 * _vbe_get_width()
 *
 * Get the screen size
 */
Uint16 _vbe_get_height(void)
{
	if( vbe_mode_info )
	{
		return vbe_mode_info->YResolution;
	}
	return 0;
}

Uint16 _vbe_get_width(void)
{
	if( vbe_mode_info )
	{
		return vbe_mode_info->XResolution;
	}
	return 0;
}

/*
 * _vbe_framebuffer_addr
 * _vbe_framebuffer_size
 *
 * Get data about the linear frame buffer
 */
void* _vbe_framebuffer_addr(void)
{
	if( vbe_mode_info )
	{
		return (void*)vbe_mode_info->PhysBasePtr;
	}
	return NULL;
}

Uint _vbe_framebuffer_size(void)
{
	if( vbe_mode_info )
	{
		return vbe_mode_info->LinBytesPerScanLine * vbe_mode_info->YResolution;
	}
	return 0;
}

/* 
 * _vbe_clear_display(color)
 * 
 * clear the display to the specified color
 */
void _vbe_clear_display(Uint8 r, Uint8 g, Uint8 b)
{
	int i, j;
	/* make sure we have a valid pointer */
	if( vbe_mode_info )
	{
		Uint x_res = vbe_mode_info->XResolution;
		Uint y_res = vbe_mode_info->YResolution;

		for( i = 0; i < x_res; i++ )
		{
			for( j = 0; j < y_res; j++ )
			{
				_vbe_draw_pixel(i, j, r, g, b);
			}
		}
	}
}

/* 
 * _vbe_draw_pixel(x, y, color)
 * 
 * set the pixel to the specified color
 */
void _vbe_draw_pixel(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b)
{
	if( vbe_mode_info )
	{
		Uint8 *display = (Uint8*)(vbe_mode_info->PhysBasePtr);
		if( display != NULL )
		{	
			/* mode settings */
			Uint16 bytesPerLine = vbe_mode_info->LinBytesPerScanLine;
			Uint16 x_res = vbe_mode_info->XResolution;
			Uint16 y_res = vbe_mode_info->YResolution;

			if( x < x_res && y < y_res )
			{
				Uint32 *pixel = (Uint32*)(&display[4*x + bytesPerLine*y]);
				*pixel = b | g<<8 | r<<16;
			}

		}
		else
		{
			char str[] = "VBE Error: NULL Framebuffer\r\n";
			char *s;
			for( s = str; *s != '\0'; s++ )
			{
				_sio_writec(*s);
			}
		}
	}
}

/*
 * _vbe_write_str(x, y, color, str)
 *
 * Write the specified NUL terminated string to the screen starting at pixel (x,y)
 */
void _vbe_write_str(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char *str)
{
	// current location to display at
	Uint x_disp = x, y_disp = y;

	if( vbe_mode_info )
	{
		// pointer into the string
		const char *c;
		for( c = str; *c != '\0'; c++ )
		{
			_vbe_write_char(x_disp, y_disp, r, g, b, *c);

			// increment display position
			x_disp += 1;
			if( x_disp  >= _vbe_char_res_x )
			{
				x_disp = 0;
				y_disp += 1;

				if( y_disp >= _vbe_char_res_y )
				{
					y_disp = 0;
				}
			}
		}
	}
}

/*
 * _vbe_write_char(x, y, color, char)
 *
 * Write the specified character at the specified position
 */
void _vbe_write_char(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char c )
{
	// call with 0 offsets for x and y
	_vbe_write_char_win(x, y, 0, 0, r, g, b, c);
}

/*
 * _vbe_write_char_win(x, y, x_start, y_start, color, char)
 *
 * Write the specified character at the specified position from a starting pixel
 */
void _vbe_write_char_win(Uint x, Uint y, Uint x_start, Uint y_start, Uint8 r, Uint8 g, Uint8 b, const char c )
{
	Uint x_idx = x + (x_start/((CHAR_WIDTH + WIN_FONT_SCALE)));
	Uint y_idx = y + (y_start/((CHAR_HEIGHT + WIN_FONT_SCALE)));
	// First, lets only draw if needed
	if( _vbe_screen_text[x_idx + y_idx*_vbe_char_res_x] == c )
		return;

	// store this character in the screen map
	_vbe_screen_text[x_idx + y_idx*_vbe_char_res_x] = c;

	// move char position into screen coords
	x *= (CHAR_WIDTH+WIN_FONT_SCALE);
	y *= (CHAR_HEIGHT+WIN_FONT_SCALE);

	// offset for window
	x += x_start;
	y += y_start;

	if( vbe_mode_info )
	{
		/* mode settings */
		Uint16 x_res = vbe_mode_info->XResolution;
		Uint16 y_res = vbe_mode_info->YResolution;

		_draw_char( c, x, y, x_res, y_res, WIN_FONT_SCALE, r, g, b, _vbe_draw_pixel );
	}
}

/*
 * _vbe_char_scroll
 *
 * Scroll the text display, starting at scroll_min_y up lines number of lines
 */
void _vbe_char_scroll(Uint min_y, Uint max_y, Uint lines)
{
	int i, j;

	/* copy up as many lines as possible */
	for( j = min_y; j <= max_y - lines; j++ )
	{
		for( i = 0; i < _vbe_char_res_x; i++ )
		{
			_vbe_write_char(i, j, 255, 255, 255, _vbe_screen_text[i + (j+lines)*_vbe_char_res_x]);
		}
	}

	/* fill the rest with spaces */
	for( ; j <= max_y; j++ )
	{
		for( i = 0; i < _vbe_char_res_x; i++ )
		{
			_vbe_write_char(i, j, 255, 255, 255, ' ');
		}
	}
}

/*
 * _vbe_get_char_win( x, y, x_start, y_start ) 
 *
 * Get the character being displayed at this point, in the window
 */
char _vbe_get_char_win( Uint x, Uint y, Uint x_start, Uint y_start )
{
	Uint x_idx = x + (x_start/((CHAR_WIDTH + WIN_FONT_SCALE)));
	Uint y_idx = y + (y_start/((CHAR_HEIGHT + WIN_FONT_SCALE)));

	if( x_idx < _vbe_char_res_x && y_idx < _vbe_char_res_y )
		return _vbe_screen_text[x_idx + y_idx * _vbe_char_res_x];

	// invalid point
	return '\0';
}

/*
 * _vbe_get_char( x, y )
 *
 * Get the character being displayed at this point
 */
char _vbe_get_char( Uint x, Uint y )
{
	return _vbe_get_char_win(x, y, 0, 0);
}

Uint32* _vbe_get_row_start( Uint y )
{
	if( y > _vbe_get_height())
		return NULL;

	return (Uint32*)(_vbe_framebuffer_addr()) + y*_vbe_get_width();
}
