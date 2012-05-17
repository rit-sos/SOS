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

	sleep(3);

	status = windowing_init( WIN_AUTO_FLIP );
	if( status == FAILURE )
		exit();

	sleep(3);

	/* print some strings */
	windowing_print_str(0, 0, "Hello from window_test\n");
	windowing_print_str(0, 2, "This is a very very very very very very very very very very "
			"very very very very very very very very very long string\n");

	windowing_print_str(0, 4, "window_test exiting in ");

	int i;
	for( i = DELAY_TIME; i > 0; i-- )
	{
		// print timer starting at (23,4)
		char c;

		c = (i % 1000) / 100;
		windowing_print_char(23, 4, c+'0');

		c = (i % 100) / 10;
		windowing_print_char(24, 4, c+'0');

		c = (i % 10);
		windowing_print_char(25, 4, c+'0');

		sleep(1);
	}

	/* Test the line drawing */
	/*					x0				y0					x1				y1 */
	windowing_draw_line(0, 				0, 					WINDOW_WIDTH, 	WINDOW_HEIGHT,	255, 255, 255);
	windowing_draw_line(0, 				WINDOW_HEIGHT, 		WINDOW_WIDTH, 	0,				255, 255, 255);
	windowing_draw_line(WINDOW_WIDTH/2,	0, 					WINDOW_WIDTH/2,	WINDOW_HEIGHT,	255, 255, 255);
	windowing_draw_line(0, 				WINDOW_HEIGHT/2, 	WINDOW_WIDTH, 	WINDOW_HEIGHT/2,255, 255, 255);

	sleep(5);

	//windowing_cleanup();

	exit();
}

