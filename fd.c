
/*
**
** File:	fd.c
**
** Author:	Andrew LeCain
**
** Contributor:
**
** Description:	file descriptor module
*/
#define	__KERNEL__20113__

#include "headers.h"

#include "c_io.h"
#include "queues.h"
#include "fd.h"
/*
** PRIVATE DEFINITIONS
*/

//number of global file descriptors to allocate
#define N_FDS	16



/*
** PRIVATE DEFINITIONS
*/

/*
** PRIVATE DATA TYPES
*/

/*
** PRIVATE GLOBAL VARIABLES
*/

static Queue *_fd_free_queue;
/*
** PUBLIC GLOBAL VARIABLES
*/


Fd _fds[N_FDS];		// all fds in the system
Fd *_stdin;		// standard in
Fd *_stdout;		// standard out

/*
** PRIVATE FUNCTIONS
*/

/*
** PUBLIC FUNCTIONS
*/

/*
** _fd_alloc()
**
** allocates a new file descriptor
**
** returns new fd on success, NULL on failure
*/

Fd *_fd_alloc(void){
	Fd *fd;

	if( _q_remove( _fd_free_queue, (void **) &fd ) != SUCCESS ) {
		fd = NULL;
	} 

	return fd;

}


Status _fd_dealloc(Fd *toFree){
	Key key;

	if(toFree == NULL){
		return( BAD_PARAM ); 
	}

	key.i = 0;
	return( _q_insert( _fd_free_queue, (void *) toFree, key) );

}



void _fd_init(void){
	int i;
	Status status;

	// allocate fd queue

	status = _q_alloc( &_fd_free_queue, NULL );
	if( status != SUCCESS ) {
		_kpanic( "_fd_init", "File Descriptor queue alloc status %s", status );
	}

	//insert all but sio and cio into queue
	for( i = 2; i < N_FDS; ++i ) {
		status = _fd_dealloc( &_fds[i] );
		if( status != SUCCESS ) {
		}
	}

	//set up sio and cio	

	_fds[CIO_FD].getc=&c_getchar;
	_fds[CIO_FD].putc=&c_putchar;
	_fds[CIO_FD].flags= RW;

	// report that we have finished

	c_puts( " fds" );


} 
