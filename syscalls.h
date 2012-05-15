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

#include "headers.h"
#include "queues.h"

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

/* windowing syscalls */
#define	SYS_s_windowing_get_window		14
#define	SYS_s_windowing_free_window		15
#define	SYS_s_windowing_print_str		16
#define	SYS_s_windowing_print_char		17
#define	SYS_s_windowing_clearscreen		18
#define	SYS_s_windowing_draw_line		19
#define	SYS_s_windowing_copy_rect		20

// number of "real" system calls

#define	N_SYSCALLS	21

// dummy system call code to test the syscall ISR

#define	SYS_bogus	0xbadc0de

// system call interrupt vector number

#define	INT_VEC_SYSCALL	0x80

// default contents of EFLAGS register

#define	DEFAULT_EFLAGS	(EFLAGS_MB1 | EFLAGS_IF)

#ifndef __ASM__20113__

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

#endif

#endif
