
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

#define	FD_BUF_SIZE	1024	

/*
** Types
*/

typedef enum rwflags{
	FD_R=0x01,
	FD_W=0x02,
	FD_RW=0x03,
	FD_EOF=0x04,
	FD_UNUSED=0x08
}Flags;

typedef struct buffer{
	char buff[FD_BUF_SIZE];
	int in;
	int out;
}Buffer;


typedef struct Fd{
	Buffer	*inbuffer;
	Buffer	*outbuffer;
	Flags flags;
	Uint	read_index;
	Uint	write_index;
	Status (*startRead)(struct Fd *fd);	//request more data from the driver.. If we can't, then NULL
	Status (*startWrite)(struct Fd *fd);	//request that the device start writing. If we can't, then NULL
	void *device_data;			//pointer to device specific data assocaited with this filestream
	Uint16 txwatermark,rxwatermark;		//watermarks to trigger flushes
}Fd;

/*
** Globals
*/


extern Queue *_reading;
extern Queue *_writing;

extern Fd _fds[];		// all fds in the system

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
Fd *_fd_alloc(Flags flags);

/*
** _fd_dealloc(toFree)
**
** deallocates a new file descriptor
**
** returns the status of inserting to the free FD queue
*/
Status _fd_dealloc(Fd *toFree);

/*
** _fd_init(void)
**
** initializes the file descriptor module
**
*/
void _fd_init(void);


/*
** _fd_lookup(Fd *fd)
**
**
** returns the index of the fd
*/
int _fd_lookup(Fd *fd);
/*
** _fd_read(file, char)
**
** read from a a file descriptor.
**
** returns the status 
*/
int _fd_read(Fd *fd);

/*
** _fd_write(file, char)
**
** write to a file descriptor.. Non-blocking, lossy
**
** returns the status 
*/
Status _fd_write(Fd *fd, char c);

/*
 ** _fd_available(file)
 **
 ** determine how much there is currently in the pipe to read from a file descriptor.
 **
 ** returns the number of queued characters
 */
int _fd_available(Fd *fd);

/*
 ** _fd_flush_rx(file)
 **
 ** Flush the rx buffer 
 **
 */
void _fd_flush_rx(Fd *fd);
/*
 ** _fd_flush_tx(file)
 **
 ** Flush the tx buffer 
 **
 */
void _fd_flush_tx(Fd *fd);
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
void _fd_readDone(Fd *fd);
/*
 ** _fd_readBack(file, character)
 **
 ** Puts character into read buffer.
 ** To be called from the device
 */
void _fd_readBack(Fd *fd, char c);

/*
 ** _fd_writeDone(file)
 **
 ** Write to device successful. Requests another character from the write buffer and unblocks any write blocked processes.
 ** To be called from the device
 **
 ** 
 **
 */
void _fd_writeDone(Fd *fd);

/*
 ** _fd_getTx(file)
 **
 ** get the next character to be transmitted.
 **
 ** returns status 
 */
int _fd_getTx(Fd *fd);


#endif

#endif
