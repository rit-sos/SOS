/*
** SCCS ID:	@(#)ulibc.c	1.1	4/5/12
**
** File:	ulibc.c
**
** Author:	4003-506 class of 20113
**
** Contributor:
**
** Description:	C implementations of user-level library functions
*/

#include "headers.h"

#include "pcbs.h"

/*
** PRIVATE DEFINITIONS
*/

/*
** PRIVATE DATA TYPES
*/

/*
** PRIVATE GLOBAL VARIABLES
*/

/*
** PUBLIC GLOBAL VARIABLES
*/

// unsigned int value strings
//
// this is identical to the kernel _kstatus_strings array,
// but is separate to simplify life for people implementing VM.

const char *ustatus_strings[] = {
	"success",		/* SUCCESS */
	"failure",		/* FAILURE */
	"bad parameter",	/* BAD_PARAM */
	"empty queue",		/* EMPTY_QUEUE */
	"not empty queue",	/* NOT_EMPTY_QUEUE */
	"allocation failed",	/* ALLOC_FAILED */
	"not found",		/* NOT_FOUND */
	"no queues",		/* NO_QUEUES */
	"bad priority",		/* BAD_PRIO */
	"already exists",	/* ALREADY EXISTS */
	"out of bounds",	/* OUT_OF_BOUNDS*/
	"end of file",		/* EOF */
};

/*
** PRIVATE FUNCTIONS
*/

/*
** PUBLIC FUNCTIONS
*/

/*
** prt_status - print a status value to the console
**
** the 'msg' argument should contain a %s where
** the desired status value should be printed
*/

void prt_status( char *msg, unsigned int stat ) {

	if( msg == NULL ) {
		return;
	}

	//c_printf( msg, ustatus(stat) );

	if( stat >= STATUS_SENTINEL ) {
		//c_printf( "bad code: %d", stat );
	}

}


void put_char_or_code(int fd, int ch ) {
	int i;
	if( ch >= ' ' && ch < 0x7f ) {
		write(fd, ch);
	} else {
		write(fd,'x');

		for (i = 0; i < 2; ++i, ch <<= 4) {
			if (((ch>>28) & 0x0f) >= 10) {
				write(fd,'a' + ((ch>>28) & 0x0f) - 10);
			} else {
				write(fd,'0' + ((ch>>28) & 0x0f));
			}
		}
	}
}

/*
 ** spawnp - create a new process running a different program
 **		at a specific priority
 **
 ** usage:  status = spawnp( &pid, prio, entry );
 **
 ** returns the PID of the child via the 'pid' parameter on
 ** success, and the status of the creation attempt
 */

//unsigned int spawnp( unsigned int *pid, unsigned int prio, void (*entry)(void) ) {
unsigned int spawnp(unsigned int *pid, unsigned int prio, unsigned int entry_id) {
	unsigned int new;
	unsigned int status, status2;

	// create the process
	status = fork( &new );

	// if that failed, return failure status
	if( status != SUCCESS ) {
		return( status );
	}

	// we have a child; it should do the exec(),
	// and the parent should see success

	if( new == 0 ) {	// we're the child
		// change the process priority
		status = set_priority( prio );
		if( status != SUCCESS ) {
			status2 = get_pid( &new );
			//c_printf( "Child pid %d", new );
			prt_status( ", set_priority() status %s\n", status );
			exit();
		}
		status = exec( entry_id );
		// if we got here, the exec() failed
		status2 = get_pid( &new );
		//c_printf( "Child pid %d", new );
		prt_status( ", exec() status %s\n", status );
		exit();
	}

	*pid = new;
	return( SUCCESS );

}

/*
 ** spawn - create a new process running a different program
 **		at standard priority
 **
 ** usage:  status = spawn( &pid, entry );
 **
 ** returns the PID of the child via the 'pid' parameter on
 ** success, and the status of the creation attempt
 */

//unsigned int spawn( unsigned int *pid, void (*entry)(void) ) {
unsigned int spawn(unsigned int *pid, unsigned int entry_id) {

	// take the easy way out

	return( spawnp(pid,PRIO_STD,entry_id) );
}

int strlen(const char *str) {
	int i = 0;
	while (str[i]) ++i;
	return i;
}

unsigned int puts(const char *str) {
	unsigned int status = SUCCESS;
	const char *p = str;

	while (*p && status == SUCCESS) {
		status = write(CIO_FD,*p++);
	}

	return status;
}

void putx(unsigned int x) {
	int i;

	write(CIO_FD,'0');
	write(CIO_FD,'x');

	if (x) {
		for (i = 0; i < 8; ++i, x <<= 4) {
			if (((x>>28) & 0x0f) >= 10) {
				write(CIO_FD,'a' + ((x>>28) & 0x0f) - 10);
			} else {
				write(CIO_FD,'0' + ((x>>28) & 0x0f));
			}
		}
	} else {
		write(CIO_FD,'0');
	}
}
