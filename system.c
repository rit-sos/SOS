/*
** SCCS ID:	@(#)system.c	1.1	4/5/12
**
** File:	system.c
**
** Author:	4003-506 class of 20113
**
** Contributor:
**
** Description:	Miscellaneous OS support functions
*/

#define	__KERNEL__20113__

#include "headers.h"

#include "system.h"
#include "clock.h"
#include "pcbs.h"
#include "bootstrap.h"
#include "syscalls.h"
#include "sio.h"
#include "scheduler.h"
#include "mman.h"
#include "fd.h"
#include "vbe.h"
#include "c_io.h"
#include "ata.h"

// need init() address
#include "kmap.h"

/*
** PUBLIC FUNCTIONS
*/

/*
** _put_char_or_code( ch )
**
** prints the character on the console, unless it is a non-printing
** character, in which case its hex code is printed
*/

void _put_char_or_code( int ch ) {

	if( ch >= ' ' && ch < 0x7f ) {
		c_putchar( ch );
	} else {
		c_printf( "\\x%02x", ch );
	}

}


/*
** _get_proc_address
**
** Get the address of a program image from a handle.
**
** Note: Address is in kernel virtual address space.
** Program will actually start at USER_ENTRY in its own address space.
*/

Status _get_proc_address(Program program, void(**entry)(void), Uint32 *size) {
	if (!entry) {
		return BAD_PARAM;
	}

	if (program >= PROC_NUM_ENTRY) {
		*entry = NULL;
		return NOT_FOUND;
	}

	*entry = PROC_IMAGE_MAP[program];
	//*size = PROC_IMAGE_SIZE[program];
	*size = 0x8000;
	return SUCCESS;
}


/*
** _cleanup(pcb)
**
** deallocate a pcb and associated data structures
*/

void _cleanup( Pcb *pcb ) {
	Status status;

	if( pcb == NULL ) {
		return;
	}

	if ((status = _mman_proc_exit(pcb)) != SUCCESS) {
		_kpanic("_cleanup", "_mman_proc_exit(pcb)", status);
	}

	if( pcb->stack != NULL ) {
		status = _stack_dealloc( pcb->stack );
		if( status != SUCCESS ) {
			_kpanic( "_cleanup", "stack dealloc status %s\n", status );
		}
	}

	pcb->state = FREE;
	status = _pcb_dealloc( pcb );
	if( status != SUCCESS ) {
		_kpanic( "_cleanup", "pcb dealloc status %s\n", status );
	}

}


/*
** _create_process(pcb,entry)
**
** initialize a new process' data structures (PCB, stack)
**
** returns:
**	success of the operation
*/

Status _create_process( Pcb *pcb, Program entry ) {
	Context *context;
	Stack *stack;
	Status status;

	// don't need to do this if called from _sys_exec(), but
	// we are called from other places, so...

	if( pcb == NULL ) {
		return( BAD_PARAM );
	}

	// if the PCB doesn't already have a stack, we
	// need to allocate one

	stack = pcb->stack;
	if( stack == NULL ) {
		stack = _stack_alloc();
		if( stack == NULL ) {
			return( ALLOC_FAILED );
		}
		pcb->stack = stack;
	}

	pcb->program = entry;

	// Do per-process VM setup
	if ((status = _mman_proc_init(pcb)) != SUCCESS) {
		return status;
	}

	// clear the stack

	_kmemclr( (void *) stack, sizeof(Stack) );

	/*
	** Set up the initial stack contents for a (new) user process.
	**
	** We reserve one longword at the bottom of the stack as
	** scratch space.
	**
	** Above that we place an context_t area that is initialized with
	** the standard initial register contents.
	**
	** The low end of the stack (high addr) will contain these values:
	**
	**      esp ->  ?       <- context save area
	**              ...     <- context save area
	**              ?       <- context save area
	**              filler  <- last word in stack
	**
	** When this process is dispatched, the context restore
	** code will pop all the saved context information off
	** the stack, leaving the "return address" on the stack
	** as if the main() for the process had been "called" from
	** the exit() stub.
	*/

	// next, set up the process context

	context = &pcb->context;

	// initialize all the fields that should be non-zero, starting
	// with the segment registers

	context->cs = GDT_USER_CODE;
	context->ss = GDT_USER_DATA;
//	context->ds = GDT_USER_DATA;
//	context->es = GDT_USER_DATA;
//	context->fs = GDT_USER_DATA;
//	context->gs = GDT_USER_DATA;

	// EFLAGS must be set up to re-enable IF when we switch
	// "back" to this context

	context->eflags = DEFAULT_EFLAGS;
//	context->eflags = DEFAULT_EFLAGS & ~EFLAGS_IF;

	// EIP must contain the entry point of the process; in
	// essence, we're pretending that this is where we were
	// executing when the interrupt arrived

	context->eip = USER_ENTRY;

	// Initial esp should be the user-mode address of the beginning of
	// the stack, minus the 4 byte buffer, minus the 4 byte return address.
	context->esp = USER_STACK - 8;

	return( SUCCESS );

}

//static void tss_info(void) {
//	c_printf("TSS info: esp0=%08x  test=%08x  (%08x)\n"
//	         "          ss0=%04x (%04x) cs=%04x (%04x) ds=%04x (%04x) ss=%04x (%04x)\n",
//	         ((Uint32*)(TSS_START+BOOT_ADDRESS))[1], ((Uint32*)(TSS_ESP0))[0], TARGET_STACK,
//	         ((Uint32*)(TSS_START+BOOT_ADDRESS))[2], GDT_DATA,
//	         ((Uint32*)(TSS_START+BOOT_ADDRESS))[19], GDT_CODE | 3,
//	         ((Uint32*)(TSS_START+BOOT_ADDRESS))[21], GDT_DATA | 3,
//	         ((Uint32*)(TSS_START+BOOT_ADDRESS))[20], GDT_DATA | 3);
//}

/*
** _init - system initialization routine
**
** Called by the startup code immediately before returning into the
** first user process.
*/

void _init( void ) {
	Pcb *pcb;
	Status status;

	/*
	** BOILERPLATE CODE - taken from basic framework
	**
	** Initialize interrupt stuff.
	*/

	__init_interrupts();	// IDT and PIC initialization

	/*
	** Console I/O system.
	*/

	_vbe_init();
	c_io_init();
	c_setscroll( 0, 7, 99, 99 );
	c_puts_at( 0, 6, "================================================================================" );

	/*
	** 20113-SPECIFIC CODE STARTS HERE
	*/

	/*
	** Initialize various OS modules
	*/

	c_printf("VBE Framebuffer: 0x%x\n", _vbe_framebuffer_addr());
	c_puts( "Module init: " );

	_q_init();		// must be first
	_pcb_init();
	_stack_init();
	_syscall_init();
	_sched_init();
	_clock_init();
	_mman_init(_vbe_framebuffer_addr(), _vbe_framebuffer_size());
	_heap_init();
	_fd_init();
	_sio_init();
	_ata_init();

	c_puts( "\n" );

	/*
	** Create the initial system ESP
	**
	** This will be the address of the next-to-last
	** longword in the system stack.
	*/

	*((Uint32*)(TSS_ESP0)) = (Uint32)(((Uint32 *) ( (&_system_stack) + 1)) - 2);

	//tss_info();

	/*
	** Install the ISRs
	*/

	__install_isr( INT_VEC_TIMER, _isr_clock );
	__install_isr( INT_VEC_SYSCALL, _isr_syscall );
	__install_isr( INT_VEC_SERIAL_PORT_1, _isr_sio );
	__install_isr(INT_VEC_GENERAL_PROTECTION, __gp_isr);

	/*
	** Create the initial process
	**
	** Code mostly stolen from _sys_fork(); if that routine
	** changes, SO MUST THIS!!!
	**
	** First, get a PCB and a stack
	*/

	pcb = _pcb_alloc();
	if( pcb == NULL  ) {
		_kpanic( "_init", "first pcb alloc failed\n", FAILURE );
	}

	pcb->stack = _stack_alloc();
	if( pcb->stack == NULL ) {
		_kpanic( "_init", "first stack alloc failed\n", FAILURE );
	}

	/*
	** Next, set up various PCB fields
	*/

	pcb->pid  = _next_pid++;
	pcb->ppid = pcb->pid;
	pcb->priority = PRIO_HIGH;	// init() should finish first

	/*
	** Set up the initial process context.
	*/

	status = _create_process( pcb, init_ID );
	if( status != SUCCESS ) {
		_kpanic( "_init", "create init process status %s\n", status );
	}

	/*
	** Make it the first process
	*/

	_current = pcb;

	/*
	** Turn on the SIO receiver (the transmitter will be turned
	** on/off as characters are being sent)
	*/

	_sio_enable( SIO_RX );

	/*
	** END OF 20113-SPECIFIC CODE
	**
	** Finally, report that we're all done.
	*/

	c_puts( "System initialization complete.\n" );
}

void __gp_isr(int vector, int code) {
	c_printf("*** GENERAL PROTECTION FAULT ***\n pid=%04x vec=0x%08x code=0x%08x\n", _current->pid, vector, code);
//	_kpanic("(#GP)", "", 0);
	_sys_exit(_current);
}
