/*
 ** SCCS ID:	@(#)syscalls.c	1.1	4/5/12
 **
 ** File:	syscalls.c
 **
 ** Author:	4003-506 class of 20113
 **
 ** Contributor:
 **
 ** Description:	System call module
 */
#define	__KERNEL__20113__

#include "headers.h"

#include "scheduler.h"
#include "sio.h"
#include "syscalls.h"
#include "system.h"
#include "kmap.h"
#include "c_io.h"
#include "fd.h"
#include "ata.h"
#include "windowing.h"
#include "mman.h"
#include "shm.h"

#include "startup.h"

/*
 ** PRIVATE DEFINITIONS
 */

/*
 ** PRIVATE DATA TYPES
 */

/*
 ** PRIVATE GLOBAL VARIABLES
 */

// system call jump table

static void (*_syscall_tbl[ N_SYSCALLS ])(Pcb *);

/*
 ** PUBLIC GLOBAL VARIABLES
 */

// queue of sleeping processes

Queue *_sleeping;

/*
 ** PRIVATE FUNCTIONS
 */

/*
** The following convenience functions are used to validate and move
** int-sized values into and out of user buffers. This is a surprisingly
** difficult thing to do, for a few reasons. The user's passed-in pointer
** might not be on the user stack. We have to make sure that the pointer
** is valid in the user's address space, that the user can write to that
** memory (if necessary), and that the read/write is aligned. Buffers
** that are larger or smaller than sizeof(int) or are not aligned should
** go through _mman_set_user_data() and _mman_get_user_data().
*/

Status _in_param(Pcb *pcb, Int32 index, /*out */ Uint32 *ret) {
	Uint32 *ptr = ((Uint32*)(pcb->context.esp)) + index;

	if (((Uint32)ptr) % 4 == 0) {
		return _mman_get_user_data(pcb, ret, ptr, sizeof(Uint32));
	} else {
		c_printf("[%04d] _in_param: unaligned ptr %08x\n", pcb->pid, ptr);
		return BAD_PARAM;
	}
}

Status _out_param(Pcb *pcb, Int32 index, Uint32 val) {
	Status status;
	void *ptr;

	// first get the pointer from the user stack
	if ((status = _in_param(pcb, index, (Uint32*)&ptr)) == SUCCESS) {
		if (((Uint32)ptr) % 4 == 0) {
			// then try to write to the pointed-to buffer
			status = _mman_set_user_data(pcb, ptr, &val, sizeof(Uint32));
		} else {
			c_printf("[%04d] _out_param: unaligned ptr %08x\n", pcb->pid, ptr);
			status = BAD_PARAM;
		}
	}

	return status;
}

/*
** Second-level syscall handlers
**
** All have this prototype:
**
**	static void _sys_NAME( Pcb * );
**
** Most syscalls return a Status value to the user calling function.
** Those which return additional information from the system have as
** their first user-level argument a pointer (called the "info pointer"
** below) to a variable into which the information is to be placed.
**
** When stack and info pointers are accessed by the syscalls, a bad pointer
** (i.e. not in user address space, not writable, etc.) is tantamount to a
** segmentation violation, and the offending process is killed.
*/

/*
 ** _sys_fork - create a new process
 **
 ** implements:	Status fork(Pid *pid);
 **
 ** returns:
 **	PID of new process via the info pointer (in parent)
 **	0 via the info pointer (in child)
 **	status of the creation attempt
 */

static void _sys_fork( Pcb *pcb ) {
	Pcb *new;
	Status status;

	c_printf("[%04x] entering _sys_fork\n", pcb->pid);

	// get a new pcb
	new = _pcb_alloc();
	if (!new) {
		RET(pcb) = FAILURE;
		return;
	}

	status = FAILURE;
	new->stack = NULL;
	new->pgdir = NULL;
	new->virt_map = NULL;

	// copy parent
	_kmemcpy((void*)new, (void*)pcb, sizeof(Pcb));

	// get a new stack
	new->stack = _stack_alloc();
	if (!new->stack) {
		goto Cleanup;
	}

	//copy parent
	_kmemcpy((void*)new->stack, (void*)pcb->stack, sizeof(Stack));

	status = _mman_proc_copy(new, pcb);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	status = _shm_copy(new, pcb);

	// fix unique fields
	new->pid = _next_pid++;
	new->ppid = pcb->pid;
	new->state = NEW;

	// return values
	// parent gets pid of child
	if ((status = _out_param(pcb, 1, new->pid)) != SUCCESS) {
		// if the parent can't get its return value then it's a segfault
		_cleanup(new);
		_sys_exit(pcb);
		return;
	}

	// child gets 0
	if ((status = _out_param(new, 1, 0)) != SUCCESS) {
		// if the child can't get its return value then either our memory
		// management is broken or the parent's stack was already wrong,
		// in which case we shouldn't be here...
		goto Cleanup;
	}

	// schedule the new process and return
	if ((status = _sched(new)) != SUCCESS) {
		goto Cleanup;
	}

	c_printf("_sys_fork: _sched(new) OK, pcb->pid=%d, new->pid=%d\n", pcb->pid, new->pid);
	//_pcb_dump(new);
	RET(pcb) = SUCCESS;
	RET(new) = SUCCESS;

	return;

Cleanup:
	c_printf("_sys_fork: cleanup: 0x%08x\n", status);
	_cleanup(new);
	RET(pcb) = status;
	return;
}

/*
 ** _sys_exit - terminate the calling process
 **
 ** implements:	void exit();
 **
 ** does not return
 */

void _sys_exit( Pcb *pcb ) {

	c_printf("*** _sys_exit : %d exiting ***\n", pcb->pid);

	c_printf("caller = %08x\n", *(Uint32*)(_get_ebp() + 4));

	// deallocate all the OS data structures

	_cleanup( pcb );

	// select a new current process

	_dispatch();

}

/*
 ** _sys_fopen- opens a new file 
 **
 ** implements:	Status fopen(Uint64 sectorstart, Uint16 length, int *fd);
 **
 ** returns:
 **	the character via the info pointer
 **	status of the operation
 */

static void _sys_fopen( Pcb *pcb ) {
	Uint64 sectorstart;
	Uint32 ss0, ss1;
	Uint32 length;
	Fd *fd;
	Status status;

	fd = NULL;

	if ((status = _in_param(pcb, 1, &ss0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &ss1)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &length)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// write test to see if we can return a value later, also pre-set
	// null fd for failure case
	if ((status = _out_param(pcb, 4, 0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	sectorstart = (((Uint64)ss1) << 32) | ((Uint64)ss0);
	
	//select the drive we want to write to.
	fd=_ata_fopen(_ata_primary,sectorstart,length,FD_RW);
	
	if(fd != NULL){
		if ((status = _out_param(pcb, 4, _fd_lookup(fd))) != SUCCESS) {
			_sys_exit(pcb);
			return;
		}
		RET(pcb) = SUCCESS;
	}else{
		RET(pcb) = FAILURE;
	}
}
/*
 ** _sys_fclose- close an open file 
 **
 ** implements:	Status fclose(int *fd);
 **
 ** returns:
 **	status of the operation
 */

static void _sys_fclose( Pcb *pcb ) {
	Uint32 fd;
	Status status;

	if ((status = _in_param(pcb, 1, &fd)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}
	if(fd == CIO_FD){
		_fd_flush_tx(&_fds[fd]);
		_fd_flush_rx(&_fds[fd]);
		status = SUCCESS;
	}else if (fd ==SIO_FD){
		_fd_flush_tx(&_fds[fd]);
		_fd_flush_rx(&_fds[fd]);
		status = SUCCESS;
	}else{
		status = _ata_fclose(&_fds[fd]);
	}
	RET(pcb)=status;
}
/*
 ** _sys_read - read a single character from the supplied FD 
 **
 ** implements:	Status read(int *buf);
 **
 ** blocks the calling routine if there is no character to return
 **
 ** returns:
 **	the character via the info pointer
 **	status of the operation
 */

static void _sys_read( Pcb *pcb ) {
	Key key;
	int ch;
	int fd;
	Status status;

	// do a test write to see if everything is valid
	if ((status = _out_param(pcb, 2, 0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// try to get the next character

	if ((status = _in_param(pcb, 1, (Uint32*)&fd)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if (_fds[fd].flags & FD_UNUSED){
		RET(pcb) = FAILURE;
		return;
	}

	ch =_fd_read(&_fds[fd]);

	// if there was a character, return it to the process;
	// otherwise, block the process until one comes in

	if( ch >= 0 ) {

		if ((status = _out_param(pcb, 2, ch & 0xff)) != SUCCESS) {
			_sys_exit(pcb);
			return;
		}
		RET(pcb) = SUCCESS;

	} else if (_fds[fd].flags & FD_EOF){
		RET(pcb) = EOF;
	} else {

		// no character; put this process on the
		// serial i/o input queue

		_current->state = BLOCKED;
		key.u = fd;

		status = _q_insert( _reading, (void *) _current, key );
		if( status != SUCCESS ) {
			_kpanic( "_sys_read", "insert status %s", status );
		}

		// select a new current process
		_dispatch();

	}
}

/*
 ** _sys_write - write a single character to the SIO
 **
 ** implements:	Status write(char buf);
 **
 ** returns:
 **	status of the operation
 */

static void _sys_write( Pcb *pcb ) {
	Key key;
	int fd;
	int ch;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&fd)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, (Uint32*)&ch)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// this is almost insanely simple, but it does separate
	// the low-level device access fromm the higher-level
	// syscall implementation

	status = _fd_write(&_fds[fd], ch);
	if (status != SUCCESS && status != EOF){

		// no character; block this process on the fd

		_current->state = BLOCKED;
		key.u = fd;

		status = _q_insert( _writing, (void *) _current, key );
		if( status != SUCCESS ) {
			_kpanic( "_sys_write", "insert status %s", status );
		}

		// select a new current process
		_dispatch();
	}


	RET(pcb) = status; 

}

/*
 ** _sys_msleep - put the current process to sleep for some length of time
 **
 ** implements:	Status msleep(Uint32 ms);
 **
 ** if the sleep time (in milliseconds) is 0, just preempts the process;
 ** otherwise, puts it onto the sleep queue for the specified length of
 ** time
 **
 ** returns:
 **	status of the sleep attempt
 */

static void _sys_msleep( Pcb *pcb ) {
	Key wakeup;
	Uint32 time;
	Status status;

	// retrieve the sleep time from the user

	if ((status = _in_param(pcb, 1, &time)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// if the user says "sleep for no seconds", we just preempt it;
	// otherwise, we put it on the sleep queue

	if( time == 0 ) {

		status = _sched( pcb );
		if( status != SUCCESS ) {
			RET(pcb) = FAILURE;
			c_printf( "msleep(%u), pid %u: can't schedule, status %s\n",
					time, pcb->pid, _kstatus(status) );
			return;
		}

	} else {

		// calculate the wakeup time
		wakeup.u = _system_time + time;

		// do the insertion
		status = _q_insert( _sleeping, (void *) pcb, wakeup );
		if( status != SUCCESS ) {
			RET(pcb) = FAILURE;
			c_printf( "msleep(%u), pid %u: can't sleep, status %s\n",
					time, pcb->pid, _kstatus(status) );
			return;
		}

	}

	// in either case, we return SUCCESS to the user and
	// dispatch a new process

	RET(pcb) = SUCCESS;

	_dispatch();

}

/*
 ** _sys_kill - terminate a process with extreme prejudice
 **
 ** implements:	Status kill(Pid pid);
 **
 ** returns:
 **	status of the termination attempt
 */

static void _sys_kill( Pcb *pcb ) {
	int i;
	Uint32 pid;
	Status status;

	// figure out who we are going to kill

	if ((status = _in_param(pcb, 1, &pid)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// locate the victim by brute-force scan of the PCB array

	for( i = 0; i < N_PCBS; ++i ) {

		// this is the victim IFF:
		//	the PIDs match, and
		//	the PCB is *not* marked as available

		if( _pcbs[i].pid == pid &&
				_pcbs[i].state != FREE ) {
			// got it!
			_pcbs[i].state = KILLED;
			RET(pcb) = SUCCESS;
			return;
		}
	}

	// we could not find the specified PID, or we found it
	// but it isn't currently alive

	RET(pcb) = NOT_FOUND;

}

/*
 ** _sys_get_priority - retrieve the priority of the current process
 **
 ** implements:	Status get_priority(Prio *prio);
 **
 ** returns:
 **	the process' priority via the info pointer
 **	SUCCESS
 */

static void _sys_get_priority( Pcb *pcb ) {
	Status status;

	if ((status = _out_param(pcb, 1, pcb->priority)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;

}

/*
 ** _sys_get_pid - retrieve the PID of the current process
 **
 ** implements:	Status get_pid(Pid *pid);
 **
 ** returns:
 **	the process' pid via the info pointer
 **	SUCCESS
 */

static void _sys_get_pid( Pcb *pcb ) {
	Status status;

	if ((status = _out_param(pcb, 1, pcb->pid)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;

}

/*
 ** _sys_get_ppid - retrieve the parent PID of the current process
 **
 ** implements:	Status get_ppid(Pid *pid);
 **
 ** returns:
 **	the process' parent's pid via the info pointer
 **	SUCCESS
 */

static void _sys_get_ppid( Pcb *pcb ) {
	Status status;

	if ((status = _out_param(pcb, 1, pcb->ppid)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;

}

/*
 ** _sys_get_time - retrieve the current system time
 **
 ** implements:	Status get_time(Time *time);
 **
 ** returns:
 **	the process' pid via the info pointer
 **	SUCCESS
 */

static void _sys_get_time( Pcb *pcb ) {
	Status status;

	if ((status = _out_param(pcb, 1, _system_time)) != SUCCESS) {
		c_printf("[%04x] _sys_get_time: _out_param: %08x (%s)\n", pcb->pid, status, _kstatus_strings[status]);
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;

}

/*
 ** _sys_get_state - retrieve the state of the current process
 **
 ** implements:	Status get_state(State *state);
 **
 ** returns:
 **	the process' state via the info pointer
 **	SUCCESS
 */

static void _sys_get_state( Pcb *pcb ) {
	Status status;

	if ((status = _out_param(pcb, 1, pcb->state)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;

}

/*
 ** _sys_set_priority - change the priority of the current process
 **
 ** implements:	Status set_priority(Prio prio);
 **
 ** returns:
 **	success of the change attempt
 */

static void _sys_set_priority( Pcb *pcb ) {
	Uint32 prio;
	Status status;

	// retrieve the desired priority
	if ((status = _in_param(pcb, 1, &prio)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// if the priority is valid, do the change;
	// otherwise, report the failure

	if( prio < N_PRIOS ) {
		pcb->priority = (Prio)prio;
		RET(pcb) = SUCCESS;
	} else {
		RET(pcb) = BAD_PARAM;
	}

}

/*
 ** _sys_set_time - change the current system time
 **
 ** implements:	Status set_time(Time time);
 **
 ** returns:
 **	SUCCESS
 */

static void _sys_set_time( Pcb *pcb ) {
	Time time;
	Status status;

	if ((status = _in_param(pcb, 1, &time)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// this is extremely simple, but disturbingly powerful

	_system_time = time;
	RET(pcb) = SUCCESS;

}

/*
 ** _sys_exec - replace a process with a different program
 **
 ** implements:	Status exec(void (*entry)(void));
 **
 ** returns:
 **	failure status of the replacement attempt
 **		(only if the attempt fails)
 */

static void _sys_exec( Pcb *pcb ) {
	Uint32 program;
	Status status;

	if ((status = _in_param(pcb, 1, &program)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// invoke the common code for process creation

	if (program < PROC_NUM_ENTRY) {
		// memory management cleanup
		status = _mman_proc_exit(pcb);
		if (status == SUCCESS) {
			status = _create_process( pcb, (Program)program );
		}
	} else {
		status = BAD_PARAM;
	}
	// we only need to assign this if the creation failed
	// for some reason - otherwise, this process never
	// "returns" from the syscall

	if( status != SUCCESS ) {
		RET(pcb) = status;
	}

}

/*
 ** Kernel entry point for user heap manager
 */
static void _sys_grow_heap(Pcb*);
static void _sys_get_heap_size(Pcb*);
static void _sys_get_heap_base(Pcb*);

/*
** _sys_grow_heap - grow the size of a user heap
**
** implements: Status grow_heap(unsigned int *new_size);
**
** returns: status and new size
*/
static void _sys_grow_heap(Pcb *pcb) {
	Status status;

	if((status = _heap_grow(pcb)) != SUCCESS) {
		RET(pcb) = status;
	} else {
		_sys_get_heap_size(pcb);
		return;
	}
}

/*
** _sys_get_heap_size - get the current size of a user heap
**
** implements: Status get_heap_size(unsigned int *size);
**
** returns: SUCCESS and size
*/
static void _sys_get_heap_size(Pcb *pcb) {
	Status status;

	if ((status = _out_param(pcb, 1, HEAP_CHUNK_SIZE * pcb->heapinfo.count)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;
}

/*
** _sys_get_heap_base - get a pointer to the base of the heap
**
** implements: Status get_heap_base(void *ptr);
**
** returns: SUCCESS and pointer to heap base
*/
static void _sys_get_heap_base(Pcb *pcb) {
	Status status;

	if ((status = _out_param(pcb, 1, USER_HEAP_BASE)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_windowing_get_window - request a window to draw to
 **
 ** implements:	Status windowing_get_window(Window *win);
 **
 ** returns:
 **		SUCCESS	if window was reservered
 **		FAILURE	otherwise
 */
static void _sys_windowing_get_window( Pcb *pcb ) {
	Window win;
	Status status;

	win = _windowing_get_window( pcb->pid );

	// return window
	if((status = _out_param(pcb, 1, win)) != SUCCESS) {
		c_printf("Can't return window\n");
		if( win != -1 )
			_windowing_free_window(win);
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = win != -1 ? SUCCESS : FAILURE;
}

/*
 ** _sys_windowing_free_window - free the specified window
 **
 ** implements:	Status windowing_free_window(Window win);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_windowing_free_window( Pcb *pcb ) {
	Status status;
	Uint32 win;

	if( (status = _in_param(pcb, 1, &win)) != SUCCESS ) {
		c_printf("_sys_windowing_free_window: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	_windowing_free_window( (Window)win );

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_windowing_print_str - display a string on the monitor
 **
 ** implements:	Status windowing_print_str(Window win, int x, int y, const char *);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_windowing_print_str( Pcb *pcb ) {
	Uint32 win, x, y;
	const char* str;


	if( _in_param(pcb, 1, &win) != SUCCESS )
	{
		c_printf("_sys_windowing_free_window: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	if( _in_param(pcb, 2, &x) != SUCCESS )
	{
		c_printf("_sys_windowing_free_window: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	if( _in_param(pcb, 3, &y) != SUCCESS )
	{
		c_printf("_sys_windowing_free_window: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	if( _in_param(pcb, 4, (Uint32*)&str) != SUCCESS )
	{
		c_printf("_sys_windowing_free_window: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	_windowing_write_str( (Window)win, x, y, 255, 255, 255, str );

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_vbe_print - display a string on the monitor
 **
 ** implements:	Status vbe_print(int x, int y, const char *);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_vbe_print( Pcb *pcb ) {
	c_printf("_sys_vbe_print: Hi James, I need a buffer size please.\n");
	RET(pcb) = FAILURE;
	return;

	/* ARG(pcb)[3] is a pointer */
	//	_vbe_write_str( ARG(pcb)[1], ARG(pcb)[2], 255, 255, 255, (const char *)ARG(pcb)[3] );
	//	RET(pcb) = SUCCESS;
}

/*
 ** _sys_vbe_print_char - display a character on the monitor
 **
 ** implements:	Status vbe_print_char(int x, int y, const char);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_vbe_print_char( Pcb *pcb ) {
	Uint32 x, y, c;
	Status status;

	if ((status = _in_param(pcb, 1, &x)) != SUCCESS) {
		c_printf("_sys_vbe_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &y)) != SUCCESS) {
		c_printf("_sys_vbe_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &c)) != SUCCESS) {
		c_printf("_sys_vbe_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	_vbe_write_char( x, y, 255, 255, 255, c & 0xff );

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_windowing_print_char - display a character on the monitor
 **
 ** implements:	Status windowing_print_char(Window, win, int x, int y, const char);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_windowing_print_char( Pcb *pcb ) {
	Uint32 win, x, y, c;
	Status status;


	if ((status = _in_param(pcb, 1, &win)) != SUCCESS) {
		c_printf("_sys_windowing_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &x)) != SUCCESS) {
		c_printf("_sys_windowing_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &y)) != SUCCESS) {
		c_printf("_sys_windowing_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 4, &c)) != SUCCESS) {
		c_printf("_sys_windowing_print_char: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	_windowing_write_char( (Window)win, x, y, 255, 255, 255, c & 0xFF );

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_vbe_clearscreen - Clear the display
 **
 ** implements:	Status vbe_clearscreen(char r, char g, char b);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_vbe_clearscreen( Pcb *pcb ) {
	Uint32 r, g, b;
	Status status;

	if ((status = _in_param(pcb, 1, &r)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &g)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &b)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	_vbe_clear_display( (Uint8)r, (Uint8)g, (Uint8)b );

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_windowing_clearscreen - Clear the display
 **
 ** implements:	Status windowing_clearscreen(Window win, char r, char g, char b);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_windowing_clearscreen( Pcb *pcb ) {
	Uint32 win, r, g, b;
	Status status;


	if ((status = _in_param(pcb, 1, &win)) != SUCCESS) {
		c_printf("_sys_windowing_clearscreen: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &r)) != SUCCESS) {
		c_printf("_sys_windowing_clearscreen: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &g)) != SUCCESS) {
		c_printf("_sys_windowing_clearscreen: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 4, &b)) != SUCCESS) {
		c_printf("_sys_windowing_clearscreen: Can't read param\n");
		_sys_exit(pcb);
		return;
	}
	_windowing_clear_display( (Window)win, (Uint8)r, (Uint8)b, (Uint8)g );

	RET(pcb) = SUCCESS;
}

/*
** _sys_write_buf - read a string from user program and print it out
**
** implements: Status write_buf(const char *str);
**
** returns: status
*/
static void _sys_write_buf(Pcb *pcb) {
	void *ptr, *buf;
	Uint32 size;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&ptr)) != SUCCESS) {
		c_printf("[%04x] _sys_write_buf: _in_param(1): %s\n", pcb->pid, _kstatus(status));
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &size)) != SUCCESS) {
		c_printf("[%04x] _sys_write_buf: _in_param(2): %s\n", pcb->pid, _kstatus(status));
		_sys_exit(pcb);
		return;
	}

	if (size > 0x100000) {
		c_printf("[%04x] _sys_write_buf: size=%d > 0x100000\n", pcb->pid, size);
		RET(pcb) = BAD_PARAM;
		return;
	}

	c_printf("[%04x] _sys_write_buf: ptr=%08x, size=%d\n", pcb->pid, ptr, size);

	buf = _kmalloc(size);

	if (!buf) {
		c_printf("[%04x] _sys_write_buf: _kmalloc returned NULL\n", pcb->pid);
		RET(pcb) = ALLOC_FAILED;
		return;
	}

	if ((status = _mman_get_user_data(pcb, buf, ptr, size)) != SUCCESS) {
		c_printf("[%04x] _sys_write_buf: _mman_get_user_data: %s\n", pcb->pid, _kstatus(status));
		RET(pcb) = status;
		_kfree(buf);
		return;
	}

	_sio_writes(buf, size);

	_kfree(buf);

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_windowing_draw_line - Draw a line in the window
 **
 ** implements:	Status windowing_draw_line(Window win, Uint x0, Uint y0, Uint x1, Uint y1, char r, char g, char b);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_windowing_draw_line( Pcb *pcb ) {
	Uint32 win, x0, y0, x1, y1, r, g, b;
	Status status;


	if ((status = _in_param(pcb, 1, &win)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &x0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &y0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 4, &x1)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 5, &y1)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 6, &r)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 7, &g)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 8, &b)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	_windowing_draw_line( (Window)win, x0, y0, x1, y1, (Uint8)r, (Uint8)g, (Uint8)b );

	RET(pcb) = SUCCESS;
}

/*
** _sys_sys_sum - sum an input array of integers.
** Used to test the implementation of _mman_get_user_data().
**
** implements: Status _sys_sum(int *buf, int count);
**
** returns: status, and prints the sum
*/
static void _sys_sys_sum(Pcb *pcb) {
	Int32 *buf, sum;
	void *ptr;
	Uint32 count, i;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&ptr)) != SUCCESS) {
		c_printf("[%04x] _sys_sys_sum: _in_param(1): %s\n", pcb->pid, _kstatus(status));
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &count)) != SUCCESS) {
		c_printf("[%04x] _sys_sys_sum: _in_param(2): %s\n", pcb->pid, _kstatus(status));
		_sys_exit(pcb);
		return;
	}

	buf = _kmalloc(count * sizeof(Int32));

	if (!buf) {
		c_printf("[%04x] _sys_sys_sum: _kmalloc returned NULL\n", pcb->pid);
		RET(pcb) = ALLOC_FAILED;
		return;
	}

	if ((status = _mman_get_user_data(pcb, buf, ptr, count*sizeof(Int32))) != SUCCESS) {
		c_printf("[%04x] _sys_sys_sum: _mman_get_user_data: %s\n", pcb->pid, _kstatus(status));
		RET(pcb) = status;
		_kfree(buf);
	}

	for (i = 0, sum = 0; i < count; i++) {
		sum += buf[i];
	}

	c_printf("[%04x] _sys_sys_sum: %d integers, total %d\n", pcb->pid, count, sum);

	_kfree(buf);

	RET(pcb) = SUCCESS;
}

/*
 ** _sys_windowing_copy_rect - Copy the userspace buffer into video memory
 **
 ** implements:	Status windowing_copy_rect(Window win, Uint x0, Uint y0, Uint w, Uint h, Uint *buf);
 **
 ** returns:
 **		SUCCESS
 */
static void _sys_windowing_copy_rect( Pcb *pcb ) {
	Uint32 win, x, y, w, h, u_buf;
	Status status;

	if( (status = _in_param(pcb, 1, &win)) != SUCCESS )
	{
		c_printf("_sys_windowing_copy_rect: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if( (status = _in_param(pcb, 2, &x)) != SUCCESS )
	{
		c_printf("_sys_windowing_copy_rect: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if( (status = _in_param(pcb, 3, &y)) != SUCCESS )
	{
		c_printf("_sys_windowing_copy_rect: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if( (status = _in_param(pcb, 4, &w)) != SUCCESS )
	{
		c_printf("_sys_windowing_copy_rect: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if( (status = _in_param(pcb, 5, &h)) != SUCCESS )
	{
		c_printf("_sys_windowing_copy_rect: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	if( (status = _in_param(pcb, 6, &u_buf)) != SUCCESS )
	{
		c_printf("_sys_windowing_copy_rect: Can't read param\n");
		_sys_exit(pcb);
		return;
	}

	Uint32 i;
	for( i = y; i < y+h; i++ )
	{
		Uint32 *ptr = _windowing_get_row_start(win, i)+x;
		if( ptr != NULL )
			if( (status = _mman_get_user_data(pcb, 
							ptr, (Uint32*)(u_buf)+i*WINDOW_WIDTH+x, 
							w*sizeof(Uint32))) != SUCCESS )
			{
				c_printf("_sys_windowing_copy_rect: %x %x %x %x %s\n", x, y, w, h, _kstatus(status));
				_sys_exit(pcb);
				return;
			}
	}

	RET(pcb) = SUCCESS;
}

/*
 * _sys_map_framebuffer
 *
 * Map the framebuffer into this processes address space
 */
static void _sys_map_framebuffer( Pcb *pcb ) {

	if( pcb == NULL )
		return;

	c_printf("_sys_map_framebuffer: start\n");
	if( _mman_alloc_framebuffer( pcb, (void*)_vbe_framebuffer_addr(), _vbe_framebuffer_size() ) != SUCCESS )
	{
		_sys_exit(pcb);
		return;
	}

	c_printf("_sys_map_framebuffer: pages mapped\n");
	if( _out_param(pcb, 1, (Uint32)_vbe_framebuffer_addr()) != SUCCESS )
	{
		_sys_exit(pcb);
		return;
	}

	c_printf("_sys_map_framebuffer: done\n");
	RET(pcb) = SUCCESS;
}


/*
** _sys_set_test: copy a large buffer into user space to test
** _mman_set_user_data().
**
** implements: Status _sys_set_test(int *buf, int count);
**
** returns: status and a large buffer of integers
*/
static void _sys_set_test(Pcb *pcb) {
	Int32 *buf;
	void *ptr;
	Uint32 count, i;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&ptr)) != SUCCESS) {
		c_printf("[%04x] _sys_set_test: _in_param(1): %s\n", pcb->pid, _kstatus(status));
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &count)) != SUCCESS) {
		c_printf("[%04x] _sys_set_test: _in_param(2): %s\n", pcb->pid, _kstatus(status));
		_sys_exit(pcb);
		return;
	}

	buf = _kmalloc(count * sizeof(Int32));

	if (!buf) {
		c_printf("[%04x] _sys_set_test: _kmalloc returned NULL\n", pcb->pid);
		RET(pcb) = ALLOC_FAILED;
		return;
	}

	for (i = 0; i < count; i++) {
		buf[i] = i;
	}

	if ((status = _mman_set_user_data(pcb, ptr, buf, count*sizeof(Int32))) != SUCCESS) {
		c_printf("[%04x] _sys_set_test: _mman_set_user_data: %s\n", pcb->pid, _kstatus(status));
		RET(pcb) = status;
		_kfree(buf);
	}

	_kfree(buf);

	RET(pcb) = SUCCESS;

}

/*
** _sys_shm_create - create a new shared memory mapping
**
** implements: Status shm_create(const char *name, unsigned int min_size,
**                               unsigned int flags, void **ptr);
**
** returns: status and pointer to new shared memory region
*/
static void _sys_shm_create(Pcb *pcb) {
	char *str;
	void *ptr;
	Uint32 length;
	Uint32 size;
	Uint32 flags;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&ptr)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &length)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 3, &size)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 4, &flags)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _out_param(pcb, 5, 0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if (length >= 255) {
		RET(pcb) = BAD_PARAM;
		return;
	}

	str = _kmalloc(length + 1);

	if (!str) {
		RET(pcb) = ALLOC_FAILED;
		return;
	}

	_kmemclr(str, length + 1);

	if ((status = _mman_get_user_data(pcb, str, ptr, length)) != SUCCESS) {
		_kfree(str);
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = _shm_create(pcb, str, size, flags, &ptr);

	_kfree(str);

	if ((status = _out_param(pcb, 5, (Uint32)ptr)) != SUCCESS) {
		_sys_exit(pcb);
	}
}

/*
** _sys_shm_open - open an existing shared memory region by name and
** map it into the calling process's address space.
**
** implements: Status shm_open(const char *name, void **ptr);
**
** returns: status and pointer to mapping
*/
static void _sys_shm_open(Pcb *pcb) {
	char *str;
	void *ptr;
	Uint32 length;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&ptr)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &length)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _out_param(pcb, 3, 0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if (length >= 255) {
		RET(pcb) = BAD_PARAM;
		return;
	}

	str = _kmalloc(length + 1);

	if (!str) {
		RET(pcb) = ALLOC_FAILED;
		return;
	}

	_kmemclr(str, length + 1);

	if ((status = _mman_get_user_data(pcb, str, ptr, length)) != SUCCESS) {
		_kfree(str);
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = _shm_open(pcb, str, &ptr);

	_kfree(str);

	if ((status = _out_param(pcb, 3, (Uint32)ptr)) != SUCCESS) {
		_sys_exit(pcb);
	}
}

/*
** _sys_shm_close - release a shared memory region
**
** implements: Status shm_close(const char *name);
**
** returns: status
*/
static void _sys_shm_close(Pcb *pcb) {
	char *str;
	void *ptr;
	Uint32 length;
	Status status;

	if ((status = _in_param(pcb, 1, (Uint32*)&ptr)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if ((status = _in_param(pcb, 2, &length)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	if (length >= 255) {
		RET(pcb) = BAD_PARAM;
		return;
	}

	str = _kmalloc(length + 1);

	if (!str) {
		RET(pcb) = ALLOC_FAILED;
		return;
	}

	RET(pcb) = _shm_close(pcb, str);

	_kfree(str);
}

/*
 ** PUBLIC FUNCTIONS
 */


/*
 ** _syscall_init()
 **
 ** initialize the system call module
 */

void _syscall_init( void ) {
	Status status;

	/*
	 ** Create the sleep queue.
	 **
	 ** It is sorted in ascending order by wakeup time.
	 */

	status = _q_alloc( &_sleeping, _comp_ascend_uint );
	if( status != SUCCESS ) {
		_kpanic( "_syscall_init", "sleepq alloc status %s\n", status );
	}

	/*
	 ** Set up the syscall jump table.  We do this here
	 ** to ensure that the association between syscall
	 ** code and function address is correct even if the
	 ** codes change.
	 */

	_syscall_tbl[ SYS_fork ]          = _sys_fork;
	_syscall_tbl[ SYS_exec ]          = _sys_exec;
	_syscall_tbl[ SYS_exit ]          = _sys_exit;
	_syscall_tbl[ SYS_msleep ]        = _sys_msleep;
	_syscall_tbl[ SYS_read ]          = _sys_read;
	_syscall_tbl[ SYS_write ]         = _sys_write;
	_syscall_tbl[ SYS_kill ]          = _sys_kill;
	_syscall_tbl[ SYS_get_priority ]  = _sys_get_priority;
	_syscall_tbl[ SYS_get_pid ]       = _sys_get_pid;
	_syscall_tbl[ SYS_get_ppid ]      = _sys_get_ppid;
	_syscall_tbl[ SYS_get_state ]     = _sys_get_state;
	_syscall_tbl[ SYS_get_time ]      = _sys_get_time;
	_syscall_tbl[ SYS_set_priority ]  = _sys_set_priority;
	_syscall_tbl[ SYS_set_time ]      = _sys_set_time;
	_syscall_tbl[ SYS_vbe_print ]     = _sys_vbe_print;
	_syscall_tbl[ SYS_vbe_print_char ]= _sys_vbe_print_char;
	_syscall_tbl[ SYS_vbe_clearscreen ]= _sys_vbe_clearscreen;
	_syscall_tbl[ SYS_fopen]          = _sys_fopen;
	_syscall_tbl[ SYS_fclose]         = _sys_fclose;
	_syscall_tbl[ SYS_grow_heap ]     = _sys_grow_heap;
	_syscall_tbl[ SYS_get_heap_size ] = _sys_get_heap_size;
	_syscall_tbl[ SYS_get_heap_base ] = _sys_get_heap_base;
	_syscall_tbl[ SYS_write_buf ]     = _sys_write_buf;
	_syscall_tbl[ SYS_sys_sum ]       = _sys_sys_sum;
	_syscall_tbl[ SYS_set_test ]      = _sys_set_test;
	_syscall_tbl[ SYS_shm_create ]    = _sys_shm_create;
	_syscall_tbl[ SYS_shm_open ]      = _sys_shm_open;
	_syscall_tbl[ SYS_shm_close ]     = _sys_shm_close;


	/* windowing */
	_syscall_tbl[ SYS_s_windowing_get_window ]		= _sys_windowing_get_window;
	_syscall_tbl[ SYS_s_windowing_free_window ]		= _sys_windowing_free_window;
	_syscall_tbl[ SYS_s_windowing_print_str ]		= _sys_windowing_print_str;
	_syscall_tbl[ SYS_s_windowing_print_char ]		= _sys_windowing_print_char;
	_syscall_tbl[ SYS_s_windowing_clearscreen ]		= _sys_windowing_clearscreen;
	_syscall_tbl[ SYS_s_windowing_draw_line ]		= _sys_windowing_draw_line;
	_syscall_tbl[ SYS_s_windowing_copy_rect ]		= _sys_windowing_copy_rect;
	_syscall_tbl[ SYS_s_map_framebuffer ]			= _sys_map_framebuffer;

	// report that we're done

	c_puts( " syscalls" );

}

/*
 ** _isr_syscall(vector,code)
 **
 ** Common handler for the system call module.  Selects
 ** the correct second-level routine to invoke based on
 ** the contents of EAX.
 **
 ** The second-level routine is invoked with a pointer to
 ** the PCB for the process.  It is the responsibility of
 ** that routine to assign all return values for the call.
 */

void _isr_syscall( int vector, int code ) {
	Uint num;

	// sanity check - verify that there is a current process

	if( _current == NULL ) {
		_kpanic( "_isr_syscall", "null _current", FAILURE );
	}

	// retrieve the syscall code from the process

	num = RET(_current);

	// verify that it's legal - if not, force an exit

	if( num >= N_SYSCALLS ) {
		c_printf( "syscall: pid %d called %d\n",
				_current->pid, num );
		num = SYS_exit;
	}

	// call the handler

	(*_syscall_tbl[num])( _current );

	// tell the PIC we're done

	__outb( PIC_MASTER_CMD_PORT, PIC_EOI );

}
