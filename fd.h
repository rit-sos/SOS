
/*
**
** File:	fd.h
**
** Author:	Andrew LeCain
**
** Contributor:
**
** Description: file descriptor module	
*/

#ifndef _FD_H
#define _FD_H

#include "headers.h"

/*
** General (C and/or assembly) definitions
*/

#define SIO_FD 		0	//define default file descriptors for serial I/O 
#define CIO_FD	 	1	//define default file descriptors for console I/O


#ifndef __ASM__20113__

/*
** Start of C-only definitions
*/


/*
** Types
*/

typedef enum rwflags{
	R,
	W,
	RW
}Rwflags;

typedef struct fd{
	int (*getc)(void);
	void (*putc)(int c);
	Rwflags flags;
}Fd;

/*
** Globals
*/

extern Fd _fds[];		// all fds in the system
extern Fd _next_fd;		// next free fd

/*
** Prototypes
*/



/*
** _fd_alloc()
**
** allocates a new file descriptor
**
** returns new fd on success, NULL on failure
*/
Fd *_fd_alloc(void);

/*
** _fd_dealloc()
**
** deallocates a new file descriptor
**
** returns the status of inserting to the free FD queue
*/
Status _fd_dealloc(Fd *toFree);

/*
** _init_fd()
**
** initializes the file descriptor module
**
*/
void _init_fd(void);

#endif

#endif
