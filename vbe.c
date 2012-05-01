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
#include "vbe_structs.h"
#include "graphics_font.h"
#include "system.h"
#include "sio.h"

// for vbe structure addresses
#include "bootstrap.h"

/* max characters possible at 1280x1024 */
#define N_CHARS	( (1280/(CHAR_WIDTH+1)) * (1024/(CHAR_HEIGHT+1)) )


/* VBE structs */
static VbeModeInfoBlock *vbe_mode_info 	= NULL;
static VbeInfoBlock	 	*vbe_info 		= NULL;

/* scale factor for font size */
static Uint _vbe_font_scale = 1;

/* character display size */
static Uint _vbe_char_res_x = 0;
static Uint _vbe_char_res_y = 0;

/* storage for characters on display */
static char _vbe_screen_text[N_CHARS];

void _vbe_set_font_scale( Uint scale )
{
	// This gives a good sized font
	_vbe_font_scale = scale;

	/* character resolution */
	_vbe_char_res_x = vbe_mode_info->XResolution / (_vbe_font_scale * (CHAR_WIDTH + 1));
	_vbe_char_res_y = vbe_mode_info->YResolution / (_vbe_font_scale * (CHAR_HEIGHT + 1));

}
	
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

	// write some strings
	/*
	_vbe_set_font_scale(1);
	_vbe_write_str(0,   0, 255, 255, 255, "VBE text test");
	_vbe_set_font_scale(2);
	_vbe_write_str(0, 3, 255, 255, 255, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	_vbe_set_font_scale(3);
	_vbe_write_str(0, 5, 255, 255, 255, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	*/

	// this gives readable size strings
	_vbe_set_font_scale(2);

	char str[] = "VBE Initialization Complete\r\n";
	char *s;
	for( s = str; *s != '\0'; s++ )
	{
		_sio_writec(*s);
	}
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
		return vbe_mode_info->PhysBasePtr;
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
				Uint8 *pixel = (&display[4*x + bytesPerLine*y]);
				pixel[0] = r;
				pixel[1] = b;
				pixel[2] = g;
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

#define BIT(x)	(1<<(x))
/*
 * _vbe_write_char(x, y, color, char)
 *
 * Write the specified character at the specified position
 */
void _vbe_write_char(Uint x, Uint y, Uint8 r, Uint8 g, Uint8 b, const char c )
{
	// store this character in the screen map
	_vbe_screen_text[x + y*_vbe_char_res_x] = c;

	// move char position into screen coords
	x *= (CHAR_WIDTH+1)*_vbe_font_scale;
	y *= (CHAR_HEIGHT+1)*_vbe_font_scale;

	if( vbe_mode_info )
	{
		Uint8 *display = (Uint8*)(vbe_mode_info->PhysBasePtr);
		if( display != NULL )
		{	
			Uint8 *c_map = GET_CHAR_FONT_PTR(c);

			/* mode settings */
			Uint16 bytesPerLine = vbe_mode_info->LinBytesPerScanLine;
			Uint16 x_res = vbe_mode_info->XResolution;
			Uint16 y_res = vbe_mode_info->YResolution;

			int i, j;
			int kx, ky;
			// For each pixel in the character
			for( i = 0; i < _vbe_font_scale * CHAR_WIDTH; i+=_vbe_font_scale )
			{
				for( j = 0; j < _vbe_font_scale * CHAR_HEIGHT; j+=_vbe_font_scale )
				{
					// make sure its a valid pixel location
					if( x+i+_vbe_font_scale-1 < x_res && y+j+_vbe_font_scale-1 < y_res )
					{
						for( kx = 0; kx < _vbe_font_scale; kx++ )
						{
							for( ky = 0; ky < _vbe_font_scale; ky++ )
							{
								if( c_map[i / _vbe_font_scale] & BIT(j / _vbe_font_scale) )
								{
									// if this pixel is part of the character, draw it
									_vbe_draw_pixel(x+i+kx, y+(CHAR_HEIGHT-(j+ky)), r, g, b);
								}
								else
								{
									// else clear to black
									_vbe_draw_pixel(x+i+kx, y+(CHAR_HEIGHT-(j+ky)), 0, 0, 0);
								}
							}
						}
					}
				}
			}
		}
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
			_vbe_screen_text[i + j*_vbe_char_res_x] = _vbe_screen_text[i+(j+lines)*_vbe_char_res_x];
		}
	}

	/* fill the rest with spaces */
	for( ; j <= max_y; j++ )
	{
		for( i = 0; i < _vbe_char_res_x; i++ )
		{
			_vbe_screen_text[i + j*_vbe_char_res_x] = ' ';
		}
	}

	_vbe_redraw_lines(min_y, max_y);
}

/*
 * _vbe_redraw_lines
 *
 * Redraw the characters between the specified lines
 */
void _vbe_redraw_lines(Uint min, Uint max)
{
	int i, j;
	for( j = min; j <= max; j++ )
	{
		for( i = 0; i < _vbe_char_res_x; i++ )
		{
			_vbe_write_char(i, j, 255, 255, 255, _vbe_screen_text[i + j*_vbe_char_res_x]);
		}
	}
}

