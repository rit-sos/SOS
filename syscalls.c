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

#include "fd.h"
#include "pcbs.h"
#include "scheduler.h"
#include "sio.h"
#include "syscalls.h"
#include "system.h"
#include "kmap.h"

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
//	c_printf("[%04d] _in_param: esp=%08x ptr=%08x\n", pcb->pid, pcb->context.esp, ptr);

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

//	c_printf("out_param: pid=%d val=%d\n", pcb->pid, val);

	// first get the pointer from the user stack
	if ((status = _in_param(pcb, index, (Uint32*)&ptr)) == SUCCESS) {
		if (((Uint32)ptr) % 4 == 0) {
			// then try to write to the pointed-to buffer
			status = _mman_set_user_data(pcb, ptr, &val, sizeof(Uint32));
//			c_printf("_mman_set_user_data: %s\n", _kstatus(status));
		} else {
			c_printf("[%04d] _out_param: unaligned ptr %08x\n", pcb->pid, ptr);
			status = BAD_PARAM;
		}
	}

//	c_printf("[%04x] out_param: %08x\n", pcb->pid, status);

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
** _sys_read - read a single character from the SIO
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
	Status status;

	// do a test write to see if everything is valid
	if ((status = _out_param(pcb, 1, 0)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	// try to get the next character

	//ch = _sio_readc();
	ch = _fds[SIO_FD].getc();

	// if there was a character, return it to the process;
	// otherwise, block the process until one comes in

	if( ch >= 0 ) {

		if ((status = _out_param(pcb, 1, ch)) == SUCCESS) {
			RET(pcb) = SUCCESS;
		} else {
			_sys_exit(pcb);
		}

		return;

	} else {

		// no character; put this process on the
		// serial i/o input queue

		_current->state = BLOCKED;

		key.u = _current->pid;
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
	int ch;
	Status status;

	//c_printf("[%04d] _sys_write\n", pcb->pid);

	// this is almost insanely simple, but it does separate
	// the low-level device access fromm the higher-level
	// syscall implementation

	if ((status = _in_param(pcb, 1, (Uint32*)&ch)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	//c_printf("[%04d] got ch %02x\n", pcb->pid, ch);

	//_sio_writec( ch );
	_fds[SIO_FD].putc(ch);
	RET(pcb) = SUCCESS;

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

static void _sys_grow_heap(Pcb *pcb) {
	Status status;

	if((status = _heap_grow(pcb)) != SUCCESS) {
		RET(pcb) = status;
	} else {
		_sys_get_heap_size(pcb);
		return;
	}
}

static void _sys_get_heap_size(Pcb *pcb) {
	Status status;

	if ((status = _out_param(pcb, 1, HEAP_CHUNK_SIZE * pcb->heapinfo.count)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;
}

static void _sys_get_heap_base(Pcb *pcb) {
	Status status;

	if ((status = _out_param(pcb, 1, USER_HEAP_BASE)) != SUCCESS) {
		_sys_exit(pcb);
		return;
	}

	RET(pcb) = SUCCESS;
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
	_syscall_tbl[ SYS_grow_heap ]     = _sys_grow_heap;
	_syscall_tbl[ SYS_get_heap_size ] = _sys_get_heap_size;
	_syscall_tbl[ SYS_get_heap_base ] = _sys_get_heap_base;

//	these are syscalls we elected not to implement
//	_syscall_tbl[ SYS_set_pid ]    = _sys_set_pid;
//	_syscall_tbl[ SYS_set_ppid ]   = _sys_set_ppid;
//	_syscall_tbl[ SYS_set_state ]  = _sys_set_state;

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
