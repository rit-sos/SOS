/*
** mman.h
**
** Memory management definition, structures, and routines.
** Implementations in mmanc.c and mmans.S
**
** Corey Bloodstein (cmb4247)
*/

#ifndef _MMAN_H
#define _MMAN_H

/* this is a pretty important one */
#define PAGESIZE (0x1000)

#define PAGE_ALIGNED __attribute__((aligned(PAGESIZE)))

/* this should be somewhere above kernel and kernel data */
#ifndef USER_ENTRY
#error USER_ENTRY is not defined
#elif (USER_ENTRY < 0x00c00000)
#error USER_ENTRY is below the end of kernel data
#endif

/* grow heap up from here (1GB) */
#define USER_HEAP	(0x40000000)
#define KERNEL_HEAP	(USER_HEAP)

/* grow stack down from here */
#define USER_STACK	(0xd0000000)

/* 4MB of 4-byte per-page refcounts */
#define REFCOUNT_BASE	((Uint32*)0x00800000)

/* maximum number of pages allowed in a (get|set)_user_data */
#define MAX_TRANSFER_PAGES	16

/*
** Macros for manipulating memory maps
*/
#define CHECK_PAGE_BIT(M,X)	((M)[(X)>>5] &  (0x80000000 >> ((X) & 0x1f)) != 0)
#define CLEAR_PAGE_BIT(M,X)	((M)[(X)>>5] &= ~(0x80000000 >> ((X) & 0x1f)))
#define SET_PAGE_BIT(M,X)	((M)[(X)>>5] |= (0x80000000 >> ((X) & 0x1f)))

/*
#define CLEAR_PAGE_BIT(M,X)		{											\
	if (((M) == _phys_map || (M) == _virt_map) && (X) < 0x0c00) {			\
		_mman_panic((M), (X));												\
	}																		\
	((M)[(X)>>5] &= ~(0x80000000 >> ((X) & 0x1f))); }

static void _mman_panic(Uint32 *map, Uint32 page) {
	Uint32 *ptr;														\
	c_printf("how did we get here? map=%08x, page=%08x, stack follows:\n",map,page); \
	for (ptr = (Uint32*)_get_ebp(); ptr; ptr = (Uint32*)*ptr) {
		c_printf("> %08x <\n", ptr[1]);	
		break;
	}
	for(;;);
	_kpanic("mman", "bad bad", FAILURE);
}
*/

/*
** Macros for manipulating physical page reference counts
*/
#define PAGE_REFCOUNT(X)	(((Uint32*)REFCOUNT_BASE)[(X)])
#define PAGE_ADDREF(X)		(SET_PAGE_BIT(_phys_map,(X)),++(PAGE_REFCOUNT((X))))
#define PAGE_RELEASE(X)		(--(PAGE_REFCOUNT((X))))

/*
#define PAGE_ADDREF(X)			(											\
	SET_PAGE_BIT(_phys_map,(X)),											\
	++(PAGE_REFCOUNT((X))),													\
	(void)(((X) < 0xc00 && (X) != 0x24 && (X) != 0x23 && (X) != 0x3d)		\
		? c_printf("page=%08x ref=%d\n",(X),PAGE_REFCOUNT((X))) : 0),		\
	PAGE_REFCOUNT((X)))

#define PAGE_RELEASE(X)			(											\
	(X) == 0x22 ? _mman_panic(_phys_map,(X)),0 : --(PAGE_REFCOUNT((X))))
*/

#define PAGE_ADDREF_V(P,X,W)	{											\
	Uint32 l_ppg;															\
	if ((status = _mman_translate_page((P), (X), &l_ppg, (W))) != SUCCESS)	\
		goto Cleanup;														\
	PAGE_ADDREF(l_ppg); }

#define PAGE_RELEASE_V(P,X,W)	{											\
	Uint32 l_ppg;															\
	if ((status = _mman_translate_page((P), (X), &l_ppg, (W))) != SUCCESS)	\
		goto Cleanup;														\
	PAGE_RELEASE(l_ppg); }

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
#define MAP_ZERO		(0x40000000)
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
extern struct pcb *_current;

extern Pagedir _kpgdir PAGE_ALIGNED;
extern Memmap_ptr _phys_map PAGE_ALIGNED;

/*
** Internal helper functions
*/

/*
** (Assembly routine)
** 
** Set the cr3 register to a new value.
**
** The cr3 register is used to give the MMU the physical address of the
** base of the page directory. Thus, calling this function switches the
** current page directory.
*/
Uint32 _mman_set_cr3(Pagedir_entry *pgdir);

/*
** (Assembly routine)
**
** Get the contents of cr0.
*/
Uint32 _mman_get_cr0(void);

/*
** (Assembly routine)
**
** Get the contents of cr2.
**
** During a page fault, cr2 contains the faulting virtual address.
*/
Uint32 _mman_get_cr2(void);

/*
** (Assembly routine)
**
** Get the contents of cr3. See _mman_set_cr3 for details.
*/
Uint32 _mman_get_cr3(void);

/*
** (Assembly routine)
**
** Set the paging and supervisor write fault bits in cr0.
*/
Uint32 _mman_enable_paging(void);

/*
** (Assembly routine)
**
** Invalidate a TLB entry for a given virtual address in the current
** virtual address space. Call to prevent a cached translation from
** being used when kernel virtual pages change their corresponding
** physical pages (present to present remapping).
*/
Uint32 _invlpg(void *ptr);

/*
** Pagefault handler ISR.
**
** vec should always be INT_VEC_PAGE_FAULT
**
** Code is a combination of PF_USER, PF_WRITE, and PF_PRESENT indicating
** the fault. The faulting address is in cr2.
*/
void _mman_pagefault_isr(int vec, int code);

/*
** Allocate a new page directory or page table
*/
Status _mman_pgdir_alloc(Pagedir_entry **pgdir);

/*
** Allocate a new address space bitmap
*/
Status _mman_map_alloc(Memmap_ptr *map);

/*
** Release an unused page directory or page table
*/
Status _mman_pgdir_free(Pagedir_entry *pgdir);

/*
** Release an unused address space bitmap
*/
Status _mman_map_free(Memmap_ptr map);

/*
** Semi-internal functions that are used elsewhere
*/

/*
** Translate a virtual page to a physical page.
**
** Note: these are page numbers, not addresses.
** To translate an address, call _mman_translate_addr.
** Also, this translation is relative to the passed-in pgdir.
**
** "write" indicates whether the page should be writable (PT_WRITE).
**
** If pgdir != _kpgdir, the PT_USER flag is also checked.
*/
Status _mman_translate_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 *phys, Uint32 write);

/*
** Translate a virtual address to a physical address for a given process's
** address space (pgdir).
*/
Status _mman_translate_addr(Pagedir_entry *pgdir, void *virt, void **phys, Uint32 write);

/*
** Map a virtual page to a physical page. Does not addref the pages.
*/
Status _mman_map_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 phys, Uint32 flags);

/*
** Unmap a virtual page from an address space. Does not release the pages.
*/
Status _mman_unmap_page(Pagedir_entry *pgdir, Uint32 virt);

/*
** Allocate a region in virtual address space, and optionally call
** _mman_alloc_at to finish the job. Does not addref the pages.
*/
Status _mman_alloc(struct pcb *pcb, void **ptr, Uint32 size, Uint32 flags);

/*
** Allocate physical memory to back a virtual memory region. If MAP_ZERO,
** use zero-mapped pages. Addrefs the physical page and marks the virtual
** pages as in-use.
*/
Status _mman_alloc_at(struct pcb *pcb, void *ptr, Uint32 size, Uint32 flags);

/*
** Release a region of virtual memory. Releases the physical pages and marks
** them as free if their refcounts reach zero. Marks the virtual pages as
** free.
*/
Status _mman_free(struct pcb *pcb, void *ptr, Uint32 size);

/*
** Copy a process's page directory into a new process's page directory for
** fork(). All writable pages are marked copy on write in both the source
** and destination page directories, and pages are addref'd appropriately.
** Shared writable pages are not COW'd.
*/
Status _mman_pgdir_copy(Pagedir_entry *dst, Memmap_ptr dstmap, Pagedir_entry *src, Memmap_ptr srcmap);

/*
** Set up the framebuffer
*/
Status _mman_alloc_framebuffer(struct pcb *pcb, void *videoBuf, Uint size);

/*
** Sets up a new process's page directory. Used to create init, also
** called during exec().
*/
Status _mman_proc_init(struct pcb *pcb);

/*
** Copy a process's memory mangement state for fork().
** Calls _mman_pgdir_copy to do most of the work, but extra logic is
** included here for setting up the new stack.
*/
Status _mman_proc_copy(struct pcb *new, struct pcb *old);

/*
** Clean up an exiting process's memory mangement information.
*/
Status _mman_proc_exit(struct pcb *pcb);

/*
** Print out some information about mman state.
*/
void _mman_info(void);


/*
** Functions intended to be called from outside of the module
*/

/*
** Module init, sets up kernel page directory and turns on paging.
*/
void _mman_init(void *videoBuf, Uint size);

/*
** Copy a user buffer into kernel virtual address space.
*/
Status _mman_get_user_data(struct pcb *pcb, void *buf, void *virt, Uint32 size);

/*
** Copy a kernel buffer into a user process's address space.
*/
Status _mman_set_user_data(struct pcb *pcb, void *virt, void *buf, Uint32 size);

/*
** Switch to the current user process's (as defined by _current)
** page directory.
*/
void _mman_restore_pgdir(void);

/*
** Switch to the kernel page directory
*/
void _mman_kernel_mode(void);


#endif

#endif
