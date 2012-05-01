#ifndef _MMAN_H
#define _MMAN_H

#define PAGE_ALIGNED __attribute__((aligned(0x1000)))

/* this should be somewhere above kernel and kernel data */
#ifndef USER_ENTRY
#error USER_ENTRY is not defined
#elif (USER_ENTRY < 0x00200000)
#error USER_ENTRY is below the end of kernel data
#endif

/* grow heap up from here (1GB) */
#define USER_HEAP	(0x40000000)

/* grow stack down from here */
#define USER_STACK	(0xfeff0000)

/*
** Core system data structures and defines
*/

/*
** Access bits (GDT entry)
*/
#define GDT_PRESENT		(0x00000080)
// privilege level (kernel = ring 0, user = ring 3)
#define GDT_PRIV_MASK	(0x00000060)
#define GDT_PRIV_USER	(0x00000060)
#define GDT_PRIV_KERNEL	(0x00000000)
// RESERVED	(1)			(0x00000010)
// data or code selector?
#define GDT_EX			(0x00000008)
#define GDT_EX_CODE		(0x00000008)
#define GDT_EX_DATA		(0x00000000)
// direction for data selectors
#define GDT_DIRECTION	(0x00000004)
#define GDT_DIR_DOWN	(0x00000004)
#define GDT_DIR_UP		(0x00000000)
// conformation for code selectors
#define GDT_CONFORM		(0x00000004)
// readable bit for code selectors
#define GDT_READ		(0x00000002)
// writable bit for data selectors
#define GDT_WRITE		(0x00000002)
#define GDT_ACCESSED	(0x00000001)

/*
** Flag bits (GDT entry)
*/
// byte or page granularity
#define GDT_GRAN		(0x00000008)
#define GDT_GRAN_4K		(0x00000008)
#define GDT_GRAN_BYTE	(0x00000000)
// 16-bit or 32-bit protected mode
#define GDT_SIZE		(0x00000004)
#define GDT_SIZE_32		(0x00000004)
#define GDT_SIZE_16		(0x00000000)
// RESERVED (0)			(0x00000002)
// RESERVED (0)			(0x00000001)

#define CLEAR_GDTENTRY(Entry) \
	(_kmemset((Entry), 0, sizeof(GDT_entry)))

#define FILL_GDTENTRY(Entry,Base,Limit,Flags,Access)      \
	((Entry)->base_lw    = ((Base)   & 0x0000ffff),       \
	 (Entry)->base_hw_lb = ((Base)   & 0x00ff0000) >> 16, \
	 (Entry)->base_hw_hb = ((Base)   & 0xff000000) >> 24, \
	 (Entry)->limit_lo   = ((Limit)  & 0x0000ffff),       \
	 (Entry)->limit_hi   = ((Limit)  & 0x00ff0000) >> 16, \
	 (Entry)->flags      = ((Flags)  & 0x00000003),       \
	 (Entry)->access     = ((Access) | 0x00000010))

#ifndef __ASM__20113__
/* GDT entry */
typedef union {
	struct {
		Uint16	base_lw;
		Uint16	limit_lo;
		Uint8	base_hw_hb;
		Uint8	flags    :4;
		Uint8	limit_hi :4;
		Uint8	access;
		Uint8	base_hw_lb;
	};
	Uint32 dword[2];
	Uint16 word[4];
} GDT_entry;

/* GDT descriptor */
typedef struct {
	Uint16	size;   // table size - 1 (bytes)
	Uint32	offset; // offset into ds of first entry
} GDTR;

typedef union {
	struct {
		Uint16	reserved0;
		Uint16	link;
		Uint32	esp0;
		Uint16	reserved1;
		Uint16	ss0;
		Uint32	esp1;
		Uint16	reserved2;
		Uint16	ss1;
		Uint32	esp2;
		Uint16	reserved3;
		Uint16	ss2;
		Uint32	cr3;
		Uint32	eflags;
		Uint32	eax;
		Uint32	ecx;
		Uint32	edx;
		Uint32	ebx;
		Uint32	esp;
		Uint32	ebp;
		Uint32	esi;
		Uint32	edi;
		Uint16	reserved4;
		Uint16	es;
		Uint16	reserved5;
		Uint16	cs;
		Uint16	reserved6;
		Uint16	ss;
		Uint16	reserved7;
		Uint16	ds;
		Uint16	reserved8;
		Uint16	fs;
		Uint16	reserved9;
		Uint16	gs;
		Uint16	reserved10;
		Uint16	ldtr;
		Uint16	iopb;
		Uint16	reserved11;
	};
	Uint16 word[50];
	Uint32 dword[25];
} TSS;
#endif


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
		Uint32	pgtbl_addr	:20;
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
//		Uint32	unused		:3;
		Uint32	cow			:1;
		Uint32	zero		:1;
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
//#define MAP_EXEC		(0x00000000)
#define MAP_COW			(0x00000200)
#define MAP_ZERO		(0x00000400)
#define MAP_DISK		(0x00000800)

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

/* exposed internal functions */
Uint32 _mman_set_cr3(Pagedir_entry *pgdir);
Uint32 _mman_get_cr2(void);
Uint32 _mman_enable_paging(void);
void _mman_pagefault_isr(int vec, int code);
Status _mman_pgdir_alloc(Pagedir_entry **pgdir);
Status _mman_map_alloc(Memmap_ptr *map);
Status _mman_map_free(Memmap_ptr map);

/* exposed semi-internal functions */
Status _mman_translate_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 *phys);
Status _mman_map_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 phys, Uint32 flags);
Status _mman_unmap_page(Pagedir_entry *pgdir, Uint32 virt);

/* non-internal functions */
void _mman_init(void *videoBuf, Uint size);
Status _mman_proc_init(struct pcb *pcb);
Status _mman_proc_exit(struct pcb *pcb);
Status _mman_alloc(void **ptr, Uint32 size, struct pcb *pcb, Uint32 flags);
Status _mman_free(void *ptr, Uint32 size, struct pcb *pcb);

#ifndef USE_TSS
extern struct pcb *_current;
void _mman_restore_pgdir(void);
void _mman_kernel_mode(void);
#endif

#endif

#endif
