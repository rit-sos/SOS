
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

#include "io.h"
#include "headers.h"
#include "queues.h"

/*
** General (C and/or assembly) definitions
*/


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

typedef struct buffer{
	char buff[FD_BUF_SIZE];
	int in;
	int out;
}Buffer;


typedef struct Fd{
	Buffer inbuffer;
	Buffer outbuffer;
	Rwflags flags;
	void (*startRead)(struct Fd *fd);	//request more data from the driver.. If we can't, then NULL
	void (*startWrite)(struct Fd *fd);	//request that the device start writing. If we can't, then NULL

}Fd;

/*
** Globals
*/


extern Queue *_reading;
extern Queue *_writing;

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
Fd *_fd_alloc(Rwflags flags);

/*
** _fd_dealloc(toFree)
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

/*
** _fd_write(file, char)
**
** write to a file descriptor.. Non-blocking, lossy
**
** returns the status 
*/
Status _fd_write(Fd *fd, int c);

/*
 ** _fd_getTx(file)
 **
 ** get the next character to be transmitted.
 **
 ** returns status 
 */
int _fd_getTx(Fd *fd);

/*
 ** _fd_read(file)
 **
 ** read from a file descriptor.. Non-blocking
 **
 ** returns the read character 
 */
int _fd_read(Fd *fd);

/*
 ** _fd_available(file)
 **
 ** determine how much there is currently in the pipe to read from a file descriptor.
 **
 ** returns the number of queued characters
 */
int _fd_available(Fd *fd);


/*
 ***************************************
 ** Functions for device to supply data
 **************************************
 */

/*
 ** _fd_readDone(file)
 **
 ** read from device successful. Puts character into read buffer and unblocks any read blocked processes.
 ** To be called from the device
 **
 */
void _fd_readDone(Fd *fd, int c);

/*
 ** _fd_writeDone(file)
 **
 ** Write to device successful. Requests another character from the write buffer and unblocks any write blocked processes.
 ** To be called from the device
 **
 ** Returns the next character to be written, or -1
 **
 */
int _fd_writeDone(Fd *fd);

#endif

#endif
