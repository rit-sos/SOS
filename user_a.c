/*
** SCCS ID:	@(#)users.c	1.1	4/5/12
**
** File:	user.c
**
** Author:	4003-506 class of 20133
**
** Contributor:
**
** Description:	User routines.
*/

#include "headers.h"

#define DELAY_STD	2500000

/*
** USER PROCESSES
**
** Each is designed to test some facility of the OS:
**
**	User(s)		Tests/Features
**	=======		===============================================
**	A, B, C		Basic operation
**	D		Spawns Z and exits
**	E, F, G		Sleep for different lengths of time
**	H		Doesn't call exit()
**	J		Tries to spawn 2*N_PCBS copies of Y
**	K		Spawns several copies of X, sleeping between
**	L		Spawns several copies of X, preempting between
**	M		Spawns W three times at low priority, reporting PIDs
**	N		Like M, but spawns X at high priority and W at low
**	P		Iterates three times, printing system time
**	Q		Tries to execute a bogus system call (bad code)
**	R		Reading and writing
**	S		Loops forever, sleeping 30 seconds at a time
**	T		Loops, fiddles with priority
**
**	W, X, Y, Z	Print characters (spawned by other processes)
**
** Output from user processes is always alphabetic.  Uppercase 
** characters are "expected" output; lowercase are "erroneous"
** output.
**
** More specific information about each user process can be found in
** the header comment for that function (below).
**
** To spawn a specific user process, uncomment its SPAWN_x
** definition in the user.h header file.
*/

/*
** Prototypes for all one-letter user main routines
*/

void main( void );

/*
** Users A, B, and C are identical, except for the character they
** print out via write().  Each prints its ID, then loops 30
** times delaying and printing, before exiting.  They also verify
** the status return from write().
*/

void main( void ) {
	int i, j;
	Status status;

	puts("hello from user_a, my main is at ");
	putx((Uint32)&main);
	puts("\n");

	status = write( 'A' );
	if( status != SUCCESS ) {
		//prt_status( "User A, write 1 status %s\n", status );
	}
	for( i = 0; i < 30; ++i ) {
//	for (;;) {
		for( j = 0; j < DELAY_STD; ++j )
			continue;
		status = write( 'A' );
		if( status != SUCCESS ) {
			//prt_status( "User A, write 2 status %s\n", status );
		}
	}

	//c_puts( "User A exiting\n" );
//	exit();

	status = write( 'a' );	/* shouldn't happen! */
	if( status != SUCCESS ) {
		//prt_status( "User A, write 3 status %s\n", status );
	}

}
