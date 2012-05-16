/*
** SCCS ID:	@(#)syscalls.h	1.1	4/5/12
**
** File:	syscalls.h
**
** Author:	4003-506 class of 20113
**
** Contributor:
**
** Description:	System call module definitions
*/

#ifndef _SYSCALLS_H
#define _SYSCALLS_H

#ifdef __KERNEL__20113__
#include "headers.h"
#include "queues.h"
#endif

#include <x86arch.h>

/*
** General (C and/or assembly) definitions
*/

// system call codes

#define	SYS_fork		0
#define	SYS_exec		1
#define	SYS_exit		2
#define	SYS_msleep		3
#define	SYS_read		4
#define	SYS_write		5
#define	SYS_kill		6
#define	SYS_get_priority	7
#define	SYS_get_pid		8
#define	SYS_get_ppid		9
#define	SYS_get_state		10
#define	SYS_get_time		11
#define	SYS_set_priority	12
#define	SYS_set_time		13
#define	SYS_vbe_print		14
#define	SYS_vbe_print_char	15
#define	SYS_vbe_clearscreen	16
#define	SYS_fopen		17	
#define	SYS_fclose		18	
#define SYS_grow_heap		19
#define SYS_get_heap_size	20
#define SYS_get_heap_base	21
#define SYS_write_buf		22
#define SYS_sys_sum			23
#define SYS_set_test		24

/* windowing syscalls */
#define	SYS_s_windowing_get_window		25
#define	SYS_s_windowing_free_window		26
#define	SYS_s_windowing_print_str		27
#define	SYS_s_windowing_print_char		28
#define	SYS_s_windowing_clearscreen		29
#define	SYS_s_windowing_draw_line		30
#define	SYS_s_windowing_copy_rect		31

// number of "real" system calls

#define	N_SYSCALLS	32

// dummy system call code to test the syscall ISR

#define	SYS_bogus	0xbadc0de

// system call interrupt vector number

#define	INT_VEC_SYSCALL	0x80

// default contents of EFLAGS register

#define	DEFAULT_EFLAGS	(EFLAGS_MB1 | EFLAGS_IF)

#if !defined(__ASM__20113__) && defined(__KERNEL__20113__)

/*
** Start of C-only definitions
*/

/*
** Types
*/

/*
** Globals
*/

// queue of sleeping processes

extern Queue *_sleeping;

/*
** Prototypes
*/

/*
** _isr_syscall()
**
** system call ISR
*/

void _isr_syscall( int vector, int code );

/*
** _syscall_init()
**
** initializes all syscall-related data structures
*/

void _syscall_init( void );

/*
** Export _sys_exit() for use in other ISRs
*/
void _sys_exit(Pcb*);

/*
** Export _in_param and _out_param for other ISRs
*/
Status _in_param(Pcb *pcb, Int32 index, Uint32 *buf);
Status _out_param(Pcb *pcb, Int32 index, Uint32 data);

#endif

#endif
