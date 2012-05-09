#ifndef _MMAN_H
#define _MMAN_H

/* this is a pretty important one */
#define PAGESIZE (0x1000)

#define PAGE_ALIGNED __attribute__((aligned(PAGESIZE)))

/* this should be somewhere above kernel and kernel data */
#ifndef USER_ENTRY
#error USER_ENTRY is not defined
#elif (USER_ENTRY < 0x00200000)
#error USER_ENTRY is below the end of kernel data
#endif

/* grow heap up from here (1GB) */
#define USER_HEAP	(0x40000000)
#define KERNEL_HEAP	(USER_HEAP)

/* grow stack down from here */
#define USER_STACK	(0xd0000000)

/* 2MB of 2-byte per-page refcounts */
#define REFCOUNT_BASE	((Uint16*)0x00a00000)

/*
** Page directory defines and structures
*/

#define PD_PRESENT		(0x00000001)
#define PD_WRITE		(0x00000002)
#define PD_USER			(0x00000004)
#define PD_CACHEWRT		(0x00000008)
#define PD_CACHEDIS		(0x00000010)
#define PD_ACCESSED		(0x00000020)
// UNUSED				(0x00000040)
#define PD_PGSIZE		(0x00000080)
// UNUSED				(0x00000f00)
#define PD_FRAME		(0xfffff000)

#ifndef __ASM__20113__
typedef union {
	struct {
		Uint32	present		:1;
		Uint32	write		:1;
		Uint32	user		:1;
		Uint32	cachewrt	:1;
		Uint32	cachedis	:1;
		Uint32	accessed	:1;
		Uint32	unused1		:1;
		Uint32	pgsize		:1;
		Uint32	unused0		:4;
		Uint32	frame		:20;
	};
	Uint32 dword;
} Pagedir_entry;
#endif

/*
** Page table defines and structures
*/

#define PT_PRESENT		(0x00000001)
#define PT_WRITE		(0x00000002)
#define PT_USER			(0x00000004)
#define PT_CACHEWRT		(0x00000008)
#define PT_CACHEDIS		(0x00000010)
#define PT_ACCESSED		(0x00000020)
#define PT_DIRTY		(0x00000040)
// RESERVED	(0)			(0x00000080)
#define PT_GLOBAL		(0x00000100)
// UNUSED				(0x00000e00)
#define PT_COW			(0x00000200)
#define PT_SHARED		(0x00000400)
#define PT_DISK			(0x00000800)
#define PT_FRAME		(0xfffff000)

#ifndef __ASM__20113__
typedef union {
	struct {
		Uint32	present		:1;
		Uint32	write		:1;
		Uint32	user		:1;
		Uint32	cachedis	:1;
		Uint32	cachewrt	:1;
		Uint32	accessed	:1;
		Uint32	dirty		:1;
		Uint32	reserved	:1;
		Uint32	global		:1;
		Uint32	cow			:1;
		Uint32	shared		:1;
		Uint32	disk		:1;
		Uint32	frame		:20;
	};
	Uint32 dword;
} Pagetbl_entry;

typedef Pagedir_entry Pagedir[1024];
typedef Pagedir_entry *Pagedir_ptr;
typedef Pagetbl_entry Pagetbl[1024];
typedef Pagetbl_entry *Pagetbl_ptr;
#endif

/*
** Mapping flags (pass to _mman_map_page and _mman_alloc)
*/
#define MAP_USER		(0x00000004)
#define MAP_WRITE		(0x00000002)
#define MAP_COW			(0x00000200)
#define MAP_SHARED		(0x00000400)
#define MAP_DISK		(0x00000800)
#define MAP_VIRT_ONLY	(0x80000000)

#define MAP_FLAGS_MASK	(0x00000e06)
#define MAP_PD_MASK		(0x00000006)

/*
** Page fault flags
*/
#define PF_PRESENT		(0x00000001)
#define PF_WRITE		(0x00000002)
#define PF_USER			(0x00000004)
#define PF_FLAGS_MASK	(0x00000007)

/*
** Memory management data structures
*/
#ifndef __ASM__20113__
typedef Uint32 Memmap[0x8000];
typedef Uint32 *Memmap_ptr;
#endif

/*
** Prototypes
*/

#ifndef __ASM__20113__
struct pcb;

#define MAX_TRANSFER_PAGES	16

/* exposed internal functions */
Uint32 _mman_set_cr3(Pagedir_entry *pgdir);
Uint32 _mman_get_cr0(void);
Uint32 _mman_get_cr2(void);
Uint32 _mman_get_cr3(void);
Uint32 _mman_enable_paging(void);
void _mman_pagefault_isr(int vec, int code);
Status _mman_pgdir_alloc(Pagedir_entry **pgdir);
Status _mman_map_alloc(Memmap_ptr *map);
Status _mman_map_free(Memmap_ptr map);

/* exposed semi-internal functions */
Status _mman_translate_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 *phys, Uint32 write);
Status _mman_translate_addr(Pagedir_entry *pgdir, void *virt, void **phys, Uint32 write);
Status _mman_map_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 phys, Uint32 flags);
Status _mman_unmap_page(Pagedir_entry *pgdir, Uint32 virt);
Status _mman_pgdir_copy(Pagedir_entry *dst, Memmap_ptr dstmap, Pagedir_entry *src, Memmap_ptr srcmap);
void _mman_info(void);

/* non-internal functions */
void _mman_init(void);
Status _mman_proc_init(struct pcb *pcb);
Status _mman_proc_copy(struct pcb *new, struct pcb *old);
Status _mman_proc_exit(struct pcb *pcb);
Status _mman_get_user_data(struct pcb *pcb, void *buf, void *virt, Uint32 size);
Status _mman_set_user_data(struct pcb *pcb, void *virt, void *buf, Uint32 size);
Status _mman_alloc(struct pcb *pcb, void **ptr, Uint32 size, Uint32 flags);
Status _mman_alloc_at(struct pcb *pcb, void *ptr, Uint32 size, Uint32 flags);
Status _mman_free(struct pcb *pcb, void *ptr, Uint32 size);

extern struct pcb *_current;

void _mman_restore_pgdir(void);
void _mman_kernel_mode(void);

#endif

#endif
