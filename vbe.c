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

/* VBE structs */
static VbeModeInfoBlock *vbe_mode_info 	= NULL;
static VbeInfoBlock	 	*vbe_info 		= NULL;

/* scale factor for font size */
static int _vbe_font_scale = 1;

/* convert pointer from a real mode segment:offset to a protected mode pointer */
//#define REAL_TO_PROTECTED(x)	((((x) & 0xFFFF0000) > 12) + ((x) & 0x0000FFFF))
#define REAL_TO_PROTECTED(x)	(x)

/* 
 * _vbe_init()
 * 
 *	Grab structures out of the bootstrap and clear the display memory
 */
void _vbe_init()
{
	/* get pointers to the vbe structs */
	vbe_info = (VbeInfoBlock*)VBE_INFO_ADDR;
	vbe_mode_info = (VbeModeInfoBlock*)VBE_MODE_INFO_ADDR;

	
	/* clear the display */
	_vbe_clear_display(0, 0, 0);


	// write some strings
	_vbe_font_scale = 1;
	_vbe_write_str(0,   0, 255, 255, 255, "VBE text test");
	_vbe_font_scale = 2;
	_vbe_write_str(0, 100, 255, 255, 255, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	_vbe_font_scale = 3;
	_vbe_write_str(0, 200, 255, 255, 255, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	
	char str[] = "VBE Initialization Complete\r\n";
	char *s;
	for( s = str; *s != '\0'; s++ )
	{
		_sio_writec(*s);
	}
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
		Uint8 *display = (Uint8*)(REAL_TO_PROTECTED(vbe_mode_info->PhysBasePtr));
		if( display != NULL )
		{	
			/* mode settings */
			Uint bytesPerLine = vbe_mode_info->LinBytesPerScanLine;
			Uint x_res = vbe_mode_info->XResolution;
			Uint y_res = vbe_mode_info->YResolution;

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
		Uint8 *display = (Uint8*)(REAL_TO_PROTECTED(vbe_mode_info->PhysBasePtr));
		if( display != NULL )
		{	
			/* mode settings */
			Uint x_res = vbe_mode_info->XResolution;
			Uint y_res = vbe_mode_info->YResolution;

			// pointer into the string
			const char *c;
			for( c = str; *c != '\0'; c++ )
			{
				_vbe_write_char(x_disp, y_disp, r, g, b, *c);

				// increment display position
				x_disp += _vbe_font_scale * (CHAR_WIDTH + 1);
				if( x_disp + (_vbe_font_scale * CHAR_WIDTH)  >= x_res )
				{
					x_disp = 0;
					y_disp += _vbe_font_scale * (CHAR_HEIGHT + 1);

					if( y_disp + (_vbe_font_scale * CHAR_HEIGHT) >= y_res )
					{
						y_disp = 0;
					}
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
	if( vbe_mode_info )
	{
		Uint8 *display = (Uint8*)(REAL_TO_PROTECTED(vbe_mode_info->PhysBasePtr));
		if( display != NULL )
		{	
			Uint8 *c_map = GET_CHAR_FONT_PTR(c);

			/* mode settings */
			Uint bytesPerLine = vbe_mode_info->LinBytesPerScanLine;
			Uint x_res = vbe_mode_info->XResolution;
			Uint y_res = vbe_mode_info->YResolution;

			int i, j;
			// For each pixel in the character
			for( i = 0; i < _vbe_font_scale * CHAR_WIDTH; i+=_vbe_font_scale )
			{
				for( j = 0; j < _vbe_font_scale * CHAR_HEIGHT; j+=_vbe_font_scale )
				{
					// make sure its a valid pixel location
					if( x+i+_vbe_font_scale-1 < x_res && y+j+_vbe_font_scale-1 < y_res )
					{
						// if this pixel is part of the character
						if( c_map[i / _vbe_font_scale] & BIT(j / _vbe_font_scale) )
						{
							int kx, ky;
							for( kx = 0; kx < _vbe_font_scale; kx++ )
							{
								for( ky = 0; ky < _vbe_font_scale; ky++ )
								{
									Uint8* pixel = ((&display[4*(x+(i+kx)) + 
												bytesPerLine*(y+(CHAR_HEIGHT-(j+ky)))]));
									pixel[0] = r;
									pixel[1] = g;
									pixel[2] = b;
								}
							}
						}
					}
				}
			}
		}
	}
}

