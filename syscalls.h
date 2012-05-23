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
#define SYS_shm_create		25
#define SYS_shm_open		26
#define SYS_shm_close		27

/* windowing syscalls */
#define	SYS_s_windowing_get_window		28
#define	SYS_s_windowing_free_window		29
#define	SYS_s_windowing_print_str		30
#define	SYS_s_windowing_print_char		31
#define	SYS_s_windowing_clearscreen		32
#define	SYS_s_windowing_draw_line		33
#define	SYS_s_windowing_copy_rect		34
#define	SYS_s_map_framebuffer			35

// number of "real" system calls

#define	N_SYSCALLS	36

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

/*
** Wrapper around _mman_get_user_data to read a 4-byte (and 4-byte-aligned)
** value from the user stack.
**
** Example: get the first parameter to a system call
**   status = _in_param(pcb, 1, &val);
*/
Status _in_param(Pcb *pcb, Int32 index, Uint32 *buf);

/*
** Wrapper around _mman_get_user_data and _mman_set_user_data to read a
** pointer to a return value and assign into it. For example, the user
** would call:
**   status = read(&ch);
** and the syscall handler would call:
**   status = _out_param(pcb, 1, ch);
** to send the character to the user.
*/
Status _out_param(Pcb *pcb, Int32 index, Uint32 data);

#endif

#endif
