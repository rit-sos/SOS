/*
** SCCS ID:	@(#)system.h	1.1	4/5/12
**
** File:	system.h
**
** Author:	4003-506 class of 20113
**
** Contributor:
**
** Description:	Basic system definitions
*/

#ifndef _SYSTEM_H
#define _SYSTEM_H

#include "headers.h"

/*
** General (C and/or assembly) definitions
*/

#ifndef __ASM__20113__

#include "mman.h"
#include "heaps.h"
#include "pcbs.h"

/*
** Prototypes
*/

/*
** _put_char_or_code( ch )
**
** prints the character on the console, unless it is a non-printing
** character, in which case its hex code is printed
*/

void _put_char_or_code( int ch );

/*
** _get_proc_address
**
** Get the address of a program image from a handle.
**
** Note: Address is in kernel virtual address space.
** Program will actually start at USER_ENTRY in its own address space.
*/

Status _get_proc_address(Program program, void(**entry)(void), Uint32 *size);


/*
** _cleanup(pcb)
**
** deallocate a pcb and associated data structures
*/

void _cleanup( Pcb *pcb );

/*
** _create_process(pcb,entry)
**
** initialize a new process' data structures (PCB, stack)
**
** returns:
**      success of the operation
*/

Status _create_process( Pcb *pcb, Program entry );

/*
** _init - system initialization routine
**
** Called by the startup code immediately before returning into the
** first user process.
*/

void _init( void );

#endif

#endif
