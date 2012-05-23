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

//#include "users.h"
#include "umap.h"

#define DELAY_LONG 100000000
//#define SPAWN_A
//#define SPAWN_MMAN
#define SPAWN_DISK
#define SPAWN_WINDOW_TEST

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
** SYSTEM PROCESSES
*/



/*
** Initial process; it starts the other top-level user processes.
*/

//void init( void ) {
void main(void) {
	int i;
	int c;

	unsigned int pid;
	unsigned int time;
	unsigned int status;

	//c_puts( "Init started\n" );
	puts("init: my main lives at ");
	putx((unsigned int)&main);
	puts("\n");

	fclose(CIO_FD); //flush CIO

	//puts("hello from ring 3 init\n");

	// we'll start the first three "manually"
	// by doing fork() and exec() ourselves

#ifdef SPAWN_A
	status = fork( &pid );
	if( status != SUCCESS ) {
		//prt_status( "init: can't fork() user A, status %s\n", status );
	} else if( pid == 0 ) {
		puts("CHILD");
		status = exec( user_a_ID );
		//prt_status( "init: can't exec() user A, status %s\n", status );
		exit();
	}
	puts("PARENT");
#endif

#ifdef SPAWN_MMAN
	status = fork( &pid );
	if( status != SUCCESS ) {
		//prt_status( "init: can't fork() user A, status %s\n", status );
	} else if( pid == 0 ) {
		status = exec( mman_test_ID );
		//prt_status( "init: can't exec() user A, status %s\n", status );
		exit();
	}
#endif

#ifdef SPAWN_WINDOW_TEST
	status = fork( &pid );
	if( status != SUCCESS ) {
		//prt_status( "init: can't fork() user window_test, status %s\n", status );
	} else if( pid == 0 ) {
		status = exec( window_test_ID );
		//prt_status( "init: can't exec() user window_test, status %s\n", status );
		exit();
	}
#endif

#ifdef SPAWN_DISK
	status = fork( &pid );
	if( status != SUCCESS ) {
	} else if( pid == 0 ) {
		status = exec(user_disk_ID );
		exit();
	}
#endif

#ifdef SPAWN_C
	status = fork( &pid );
	if( status != SUCCESS ) {
		//prt_status( "init: can't fork() user C, status %s\n", status );
	} else if( pid == 0 ) {
		status = exec( user_c );
		//prt_status( "init: can't exec() user C, status %s\n", status );
		exit();
	}
#endif

	// for most of the rest, we'll use spawn()

#ifdef SPAWN_D
	status = spawn( &pid, user_d );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user D, status %s\n", status );
	}
#endif

#ifdef SPAWN_E
	status = spawn( &pid, user_e );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user E, status %s\n", status );
	}
#endif

#ifdef SPAWN_F
	status = spawn( &pid, user_f );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user F, status %s\n", status );
	}
#endif

#ifdef SPAWN_G
	status = spawn( &pid, user_g );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user G, status %s\n", status );
	}
#endif

#ifdef SPAWN_H
	status = spawn( &pid, user_h );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user H, status %s\n", status );
	}
#endif

#ifdef SPAWN_J
	status = spawn( &pid, user_j );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user J, status %s\n", status );
	}
#endif

#ifdef SPAWN_K
	status = spawn( &pid, user_k );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user K, status %s\n", status );
	}
#endif

#ifdef SPAWN_L
	status = spawn( &pid, user_l );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user L, status %s\n", status );
	}
#endif

#ifdef SPAWN_M
	status = spawn( &pid, user_m );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user N, status %s\n", status );
	}
#endif

#ifdef SPAWN_N
	status = spawn( &pid, user_n );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user N, status %s\n", status );
	}
#endif

#ifdef SPAWN_P
	status = spawn( &pid, user_p );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user P, status %s\n", status );
	}
#endif

#ifdef SPAWN_Q
	status = spawn( &pid, user_q );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user Q, status %s\n", status );
	}
#endif

#ifdef SPAWN_R
	status = spawn( &pid, user_r );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user R, status %s\n", status );
	}
#endif

#ifdef SPAWN_S
	status = spawn( &pid, user_s );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user S, status %s\n", status );
	}
#endif

#ifdef SPAWN_T
	status = spawn( &pid, user_t );
	if( status != SUCCESS ) {
		//prt_status( "init: can't spawn() user T, status %s\n", status );
	}
#endif

	write(CIO_FD, '!' );

	/*
	** And now we start twiddling our thumbs
	*/

	status = get_time( &time );
	if( status != SUCCESS ) {
		//prt_status( "idle: get_time status %s\n", status );
	}
	//c_printf( "init => idle at time %08x\n", time );

	status = set_priority( PRIO_IDLE );
	if( status != SUCCESS ) {
		//prt_status( "idle: priority change status %s\n", status );
	}

	write(CIO_FD, '.' );

	for(;;) {
		for( i = 0; i < DELAY_LONG; ++i )
			continue;
		write(CIO_FD, '.' );
	}

	/*
	** SHOULD NEVER REACH HERE
	*/

	//c_printf( "*** IDLE IS EXITING???\n" );
	exit();

}
