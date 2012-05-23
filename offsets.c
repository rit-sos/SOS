/*
** offsets.c
**
** Used to generate offsets.h, the offsets of the members of the pcb and
** context, for use in isr_stubs.S.
*/

#define _HEADERS_H
#define _CLOCK_H
#define _PROCESSES_H
#define _MMAN_H
#define __KERNEL__20113__

#define USER_ENTRY 0x10000

typedef unsigned int Uint32;
typedef int Int32;
typedef unsigned short Uint16;
typedef short Int16;
typedef unsigned char Uint8;
typedef char Int8;

typedef void Stack;
typedef void *Memmap_ptr, *Pagedir_ptr;
typedef Uint16 Pid;
typedef Uint32 Time;
typedef Uint32 Status;

#include "heaps.h"
#include "pcbs.h"
#include <stddef.h>
#include <stdio.h>

int main(int argc, char **argv) {
	printf("#ifdef __ASM__20113__\n");
	putchar('\n');
	printf("/* PCB offsets */\n");
	printf("#define PCB_CONTEXT		(%d)\n", offsetof(Pcb, context));
	printf("#define PCB_HEAPINFO	(%d)\n", offsetof(Pcb, heapinfo));
	printf("#define PCB_SHMINFO		(%d)\n", offsetof(Pcb, shminfo));
	printf("#define PCB_STACK		(%d)\n", offsetof(Pcb, stack));
	printf("#define PCB_VIRT_MAP	(%d)\n", offsetof(Pcb, virt_map));
	printf("#define PCB_PGDIR		(%d)\n", offsetof(Pcb, pgdir));
	printf("#define PCB_WAKEUP		(%d)\n", offsetof(Pcb, wakeup));
	printf("#define PCB_PID			(%d)\n", offsetof(Pcb, pid));
	printf("#define PCB_PPID		(%d)\n", offsetof(Pcb, ppid));
	printf("#define PCB_STATE		(%d)\n", offsetof(Pcb, state));
	printf("#define PCB_PRIORITY	(%d)\n", offsetof(Pcb, priority));
	printf("#define PCB_QUANTUM		(%d)\n", offsetof(Pcb, quantum));
	printf("#define PCB_PROGRAM		(%d)\n", offsetof(Pcb, program));
	putchar('\n');
	printf("/* context offsets */\n");
//	printf("#define CTX_DUMMY_SS	(%d)\n", offsetof(Context, dummy_ss));
//	printf("#define CTX_GS			(%d)\n", offsetof(Context, gs));
//	printf("#define CTX_FS			(%d)\n", offsetof(Context, fs));
//	printf("#define CTX_ES			(%d)\n", offsetof(Context, es));
//	printf("#define CTX_DS			(%d)\n", offsetof(Context, ds));
	printf("#define CTX_EDI			(%d)\n", offsetof(Context, edi));
	printf("#define CTX_ESI			(%d)\n", offsetof(Context, esi));
	printf("#define CTX_EBP			(%d)\n", offsetof(Context, ebp));
	printf("#define CTX_DUMMY_ESP	(%d)\n", offsetof(Context, dummy_esp));
	printf("#define CTX_EBX			(%d)\n", offsetof(Context, ebx));
	printf("#define CTX_EDX			(%d)\n", offsetof(Context, edx));
	printf("#define CTX_ECX			(%d)\n", offsetof(Context, ecx));
	printf("#define CTX_EAX			(%d)\n", offsetof(Context, eax));
	printf("#define CTX_VECTOR		(%d)\n", offsetof(Context, vector));
	printf("#define CTX_CODE		(%d)\n", offsetof(Context, code));
	printf("#define CTX_EIP			(%d)\n", offsetof(Context, eip));
	printf("#define CTX_CS			(%d)\n", offsetof(Context, cs));
	printf("#define CTX_EFLAGS		(%d)\n", offsetof(Context, eflags));
	printf("#define CTX_ESP			(%d)\n", offsetof(Context, esp));
	printf("#define CTX_SS			(%d)\n", offsetof(Context, ss));
	putchar('\n');
	printf("#endif\n");
	return 0;
}
