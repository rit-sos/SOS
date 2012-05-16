#ifndef _HEAPS_H
#define _HEAPS_H

/*
** User heap management
**
** A newborn process only has a huge address space, but almost none of it
** is mapped to physical memory. The purpose of heap management is to give
** the user program room outside of its tiny stack and program data section
** to store data.
**
** A user heap starts out as a 4 MB chunk at USER_HEAP_BASE and grows as needed
** in 4 MB increments. The userspace heap memory allocator is responsible
** for calling _sys_grow_heap as needed. Heap memory is reclaimed on process
** exit.
** 
*/

/* heap is mapped into user memory at HEAP_BASE and up */
#define USER_HEAP_BASE		(0x20000000)
#define KERNEL_HEAP_BASE	(USER_HEAP_BASE)

/* max heap size is HEAP_CHUNK_SIZE * MAX_CHUNKS */
#define HEAP_CHUNK_SIZE		(0x00100000)
#define USER_MAX_CHUNKS		(1024)
#define KERNEL_MAX_CHUNKS	(1024)

#define USER_HEAP_MAX		(USER_HEAP_BASE + HEAP_CHUNK_SIZE * USER_MAX_CHUNKS)

#if defined(__KERNEL__20113__) && !defined(__ASM__20113__)

/*
** Heap allocation information struct
*/
typedef struct {
	/* number of chunks that make up the heap */
	Uint32 count;
} Heapinfo;

struct pcb;

/*
** Heap manipulation functions
*/
void _heap_init(void);
Status _heap_create(struct pcb *pcb);
Status _heap_grow(struct pcb *pcb);
Status _heap_destroy(struct pcb *pcb);

typedef struct tagHeapbuf{
	/* size of the heap array */
	int size;
	struct tagHeapbuf *prev;
	struct tagHeapbuf *next;
	/* the end of the heap array is heapbuf.buf + heapbuf.size */
	unsigned char buf[1];
} __attribute__((aligned(4))) Heapbuf;

#define HEAP_TAG_SIZE	(offsetof(struct tagHeapbuf, buf))

void *_kmalloc(Uint32 size);
void _kfree(void *ptr);

#endif

#endif
