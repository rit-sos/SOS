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

#include "pcbs.h"
#include "scheduler.h"
#include "c_io.h"
#include "queues.h"
#include "fd.h"
/*
** PRIVATE DEFINITIONS
*/

//number of global file descriptors to allocate
#define N_FDS	16



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

Queue *_reading;
Queue *_writing;

Fd _fds[N_FDS];		// all fds in the system
Fd *_stdin;		// standard in
Fd *_stdout;		// standard out

/*
 ** PRIVATE FUNCTIONS
 */

inline int buffer_empty(Buffer *b){
	return b->in == b->out;
}
inline int buffer_full(Buffer *b){
	return (b->in+1)%FD_BUF_SIZE == b->out;
}

inline void buffer_put(Buffer *b,unsigned char c){
	b->buff[b->in++]=c;
	b->in %=FD_BUF_SIZE;
}
inline unsigned char buffer_get(Buffer *b){
	unsigned char c =  b->buff[b->out++];
	b->out %= FD_BUF_SIZE;
	return c;
}

inline int buffer_size(Buffer *b){
	int sz = (b->in - b->out);
	if (sz < 0 ) sz+= FD_BUF_SIZE;
	return sz;
}


/*
 ** PUBLIC FUNCTIONS
 */

/*
 ** _fd_alloc()
 **
 ** allocates a new file descriptor, and requested queues
 **
 ** returns new fd on success, NULL on failure
 */

Fd *_fd_alloc(Flags flags){
	Fd *fd;

	if( _q_remove( _fd_free_queue, (void **) &fd ) != SUCCESS ) {
		fd = NULL;
	}else{
		//set up the file descriptor queues and buffers	
		fd->flags=flags;

		if (flags != FD_W){
			//TODO: dynamically allocate read

		}
		if (flags != FD_R){
			//TODO: dynamically allocate write
		}

	}
	fd->read_index=0;	
	fd->write_index=0;	
	fd->inbuffer.in=0;	
	fd->inbuffer.out=0;	

	return fd;

}


Status _fd_dealloc(Fd *toFree){
	Key key;
	if(toFree == NULL){
		return( BAD_PARAM ); 
	}
	toFree->flags = FD_UNUSED;

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

	//set up cio	

	_fds[CIO_FD].startRead=NULL;
	_fds[CIO_FD].startWrite=&c_startWrite;
	_fds[CIO_FD].flags= FD_RW;
	_fds[CIO_FD].rxwatermark= 1;
	_fds[CIO_FD].txwatermark= 1;

	status = _q_alloc(&_reading,&_comp_ascend_uint);
	if( status != SUCCESS ) {
		_kpanic("_fd_init", "couldn't allocate read queue", status);
	}
	status = _q_alloc(&_writing,&_comp_ascend_uint);
	if( status != SUCCESS ) {
		_kpanic("_fd_init", "couldn't allocate write queue",status);
	}
	// report that we have finished

	c_puts( " fds" );


}

/*
 ** _fd_write(file, char)
 **
 ** write to a file descriptor.
 **
 ** returns status 
 */
Status _fd_write(Fd *fd, char c){
	Status status;
	//if outbuffer is not full
	status = SUCCESS;

	if(!buffer_full(&fd->outbuffer)){
		
		buffer_put(&fd->outbuffer,c);
		fd->write_index++;
		
		if (fd->startWrite!= NULL && buffer_size(&fd->outbuffer) >= fd->txwatermark){ //if we need to request a write
			//start the write
			status = fd->startWrite(fd);
		}	

		if(fd->flags & FD_EOF || status == EOF){
			return EOF;
		}

		return SUCCESS;
	}else{
		return FAILURE;
	}
}


/*
 ** _fd_read(file)
 **
 ** read from a file descriptor.. Non-blocking
 **
 ** returns the read character, or -1 if none was read. 
 */
int _fd_read(Fd *fd){
	if (fd->flags & FD_UNUSED){
		return -1;
	}	

	Status status;
	if (fd->startRead!= NULL && _fd_available(fd) <= fd->rxwatermark){ //if we need to request a read
		status = fd->startRead(fd);
	}

	if(status == EOF){
		fd->flags |= FD_EOF;
		return -1;
	}

	//if inbuffer is not empty
	if(!buffer_empty(&fd->inbuffer)){
		fd->read_index++;
		return (int) buffer_get(&fd->inbuffer);
	}else{
		return -1;
	}
}

/*
 ** _fd_available(file)
 **
 ** determine how much there is currently in the pipe to read from a file descriptor.
 **
 ** returns the number of queued characters
 */
int _fd_available(Fd *fd){
	return buffer_size(&fd->inbuffer);
}
/*
 ** _fd_flush_rx(file)
 **
 ** Flush the rx buffer 
 **
 */
void _fd_flush_rx(Fd *fd){
	int i, chars;
	while(! buffer_empty(&fd->inbuffer)){
		buffer_get(&fd->inbuffer);	
	}
}
/*
 ** _fd_flush_tx(file)
 **
 ** Flush the tx buffer 
 **
 */
void _fd_flush_tx(Fd *fd){
	int i, chars;
	chars = buffer_size(&fd->outbuffer);
	for (i=0;i< chars; i++){
		buffer_get(&fd->outbuffer);	
	}
}


/*
 ****************************************************************************
 *			Callbacks
 ****************************************************************************
 */


/*
 ** _fd_readDone(file)
 **
 ** read from device successful. Puts character into read buffer and unblocks any read blocked processes.
 ** To be called from the device
 **
 */
void _fd_readDone(Fd *fd){

	Pcb *pcb;
	Key key;
	Status status;
	int *ptr;
	char c;

	key.u = fd-_fds;
	//unblock process waiting on reading 

	while(!_q_empty(_reading)){
		status = _q_remove_by_key( _reading, (void **) &pcb, key );
		if (status==SUCCESS){
			pcb->state=READY;
			//put the newest char into the blocked process
			ptr = (int *) (ARG(pcb)[2]);
			c=buffer_get(&fd->inbuffer);
			*ptr = c & 0xff;
			RET(pcb) = SUCCESS;
			//schedule the previously blocked process
			_sched(pcb);
			//return here so we don't put the car in the buffer.
			return;
		}
		if (status==NOT_FOUND){
			break;
		}
	}
}

void _fd_readBack(Fd *fd, char c){
	if(!buffer_full(&fd->inbuffer)){
		buffer_put(&fd->inbuffer,c);
	}else{
		//uh.. Crap. We're not reading fast enough.
		c_printf("Dropped from %x ",fd-_fds); 
	}
}


/*
 ** _fd_writeDone(file)
 **
 ** Write to device successful. Requests another character from the write buffer and unblocks any write blocked processes.
 ** To be called from the device
 **
 ** Returns the next character to be written, or -1
 **
 */
int _fd_writeDone(Fd *fd){
	Pcb *pcb;
	Key key;
	Status status;
	int c;

	key.u = fd-_fds;


	//if outbuffer is not empty
	if(!buffer_empty(&fd->outbuffer)){
		c = buffer_get(&fd->outbuffer);
	}else{
		c = -1;
	}

	//unblock process waiting on write
	if(!_q_empty(_writing)){
		status = _q_remove_by_key( _writing, (void **) &pcb, key );
		if (status==SUCCESS){
			//attempt to put the blocked process's character into the queue
			status=_fd_write(fd,ARG(pcb)[2]);

			if (status != SUCCESS){
				c_puts("Second chance write failed!");
			}

			pcb->state=READY;
			//schedule process
			_sched(pcb);

		}else if (status==NOT_FOUND){
			//this just means that the thing in the queue wasn't blocking on this FD.	
		}
	}

	return c;
}
/*
 ** _fd_getTx(file)
 **
 ** get the next character to be transmitted.
 **
 ** returns status 
 */
int _fd_getTx(Fd *fd){
	int c;
	c=-1;
	//if outbuffer is not empty
	if(!buffer_empty(&fd->outbuffer)){
		c = buffer_get(&fd->outbuffer);
	}
	return c;
}
