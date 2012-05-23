/*
** File:	window_test.c
**
** Author:	James Letendre
**
** Description: Test of userspace windowing
*/

#include "headers.h"
#include "ulib.h"

#define DELAY_TIME	10

void main( void ) {
	Status status;

	sleep(1);

	status = windowing_init( WIN_AUTO_FLIP );
	if( status == FAILURE )
	{
		exit();
	}

	sleep(1);

	/* print some strings */
	windowing_print_str("Hello from window_test\n");
	windowing_print_str("This is a very very very very very very very very very very "
			"very very very very very very very very very long string\n");

	windowing_print_str("window_test exiting in ");

	windowing_flip_screen();

	int i;
	for( i = DELAY_TIME; i > 0; i-- )
	{
		// print timer starting at (23,4)
		char c;

		c = (i % 1000) / 100;
		windowing_set_char_pos(23, 3);
		windowing_print_char(c+'0');

		c = (i % 100) / 10;
		windowing_print_char(c+'0');

		c = (i % 10);
		windowing_print_char(c+'0');

		windowing_flip_screen();
		sleep(1);
	}

	/* Test the line drawing */
	/*					x0				y0					x1				y1 */
	windowing_draw_line(0, 				0, 					WINDOW_WIDTH, 	WINDOW_HEIGHT,	255, 255, 255);
	windowing_draw_line(0, 				WINDOW_HEIGHT, 		WINDOW_WIDTH, 	0,				255, 255, 255);
	windowing_draw_line(WINDOW_WIDTH/2,	0, 					WINDOW_WIDTH/2,	WINDOW_HEIGHT,	255, 255, 255);
	windowing_draw_line(0, 				WINDOW_HEIGHT/2, 	WINDOW_WIDTH, 	WINDOW_HEIGHT/2,255, 255, 255);
	windowing_flip_screen();

	sleep(5);

	int x, y;
	for( i = 0; ; i++ )
	{
		for( x = 0; x < WINDOW_WIDTH; x++ )
		{
			for( y = 0; y < WINDOW_HEIGHT; y++ )
			{
				windowing_draw_pixel(x, y, ((x+y+i)>>16) & 0xFF, ((x+y+i)>>8) & 0xFF, (x+y+i) & 0xFF);
			}
		}
		windowing_flip_screen();
	}
	windowing_cleanup();

	exit();
}

