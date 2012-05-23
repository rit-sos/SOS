#define __KERNEL__20113__

#include "headers.h"
#include "c_io.h"
#include "mman.h"
#include "heaps.h"

/*
** Private heap management globals
*/

static Heapinfo _kheapinfo;
static void *_heap_start_ptr;
static void *_heap_end_ptr;
static Heapbuf *_heap_first;
static Uint32 _heap_extent;

/*
** Heap initialization
*/
void _heap_init(void) {
	Status status;

	/* set up the kernel heap */
	status = _mman_alloc_at(NULL, (void*)KERNEL_HEAP_BASE, HEAP_CHUNK_SIZE, MAP_WRITE);
	if (status != SUCCESS) {
		_kpanic("heap", "_mman_alloc_at: %s", status);
	}

	_kmemclr(&_kheapinfo, sizeof(Heapinfo));

	_heap_start_ptr = (void*)KERNEL_HEAP_BASE;
	_heap_end_ptr = (void*)(KERNEL_HEAP_BASE + HEAP_CHUNK_SIZE);
	_heap_first = NULL;
	_heap_extent = 0;
	_kheapinfo.count = 1;

	c_printf("clear ok\n");

	c_puts(" heap");
}

/*
** Public heap management functions, same as umem.c user allocator
*/

/*
** Create a new heap allocation on the kernel heap. Like malloc().
*/
void *_kmalloc(Uint32 size) {
	void *ptr;
	Heapbuf *tag, *prev, *curr;
	Uint32 real_size;

	if (size == 0) {
		return NULL;
	}

	/* add the size of the heap tag to the allocation size */
	real_size = size + HEAP_TAG_SIZE;

	/* align the tag */
	if (real_size & 0x03) {
		real_size += 4 - (real_size & 0x03);
	}

	/* sanity check: is there any chance we can fit this allocation? */
	if (real_size > HEAP_CHUNK_SIZE * KERNEL_MAX_CHUNKS) {
		return NULL;
	}

	if (!_heap_first) {
		/* case #1: there are no other heap allocations */
		ptr = _heap_start_ptr;
		prev = NULL;
		curr = NULL;
	} else if ((void*)_heap_first - _heap_start_ptr >= real_size) {
		/* case #2: insert before the head */
		ptr = _heap_start_ptr;
		prev = NULL;
		curr = _heap_first;
	} else {
		prev = _heap_first;
		curr = _heap_first->next;

		while (curr) {
			ptr = (void*)prev->buf + prev->size;

			/* is this a big enough space? */
			if (ptr - (void*)curr >= real_size) {
				/* case #3: insert into the list */
				break;
			}

			/* otherwise advance */
			prev = curr;
			curr = curr->next;
		}
	
		/* case #4: reached the end of the list, so append */
		if (!curr) {
			ptr = (void*)prev->buf + prev->size;
		}
	}


	if (!curr) {
		/* is the new pointer past the end of the heap? */
		if (ptr + real_size > _heap_end_ptr) {
			/* is there any hope of fitting it? */
			if (ptr + real_size <= (void*)(KERNEL_HEAP_BASE+HEAP_CHUNK_SIZE*KERNEL_MAX_CHUNKS)-_heap_extent) {
				/* try to grow the heap */
				do {
					if (_heap_grow(NULL) != SUCCESS) {
						return NULL;
					} else {
						_heap_end_ptr += HEAP_CHUNK_SIZE;
					}
				} while(ptr + real_size > _heap_end_ptr);
			} else {
				/* give up before we ask for tons of memory we don't need */
				return NULL;
			}
		}

		/* update the extent */
		_heap_extent += real_size;
	}

	/* set up the new heap tag and return a pointer to the new buffer */
	tag = (Heapbuf*)ptr;

	tag->prev = prev;
	tag->next = curr;

	if (prev) {
		prev->next = tag;
	} else {
		_heap_first = tag;
	}

	if (curr) {
		curr->prev = tag;
	}

	tag->size = size;

	return tag->buf;
}

/*
** Release a heap allocation on the kernel heap. Like free().
*/
void _kfree(void *ptr) {
	Heapbuf *tag;

	if (ptr) {
		/* get the associated tag */
		tag = (ptr - HEAP_TAG_SIZE);

		if (tag->size > 0) {
			/* remove the tag from the chain */

			/* first tag? */
			if (tag->prev) {
				tag->prev->next = tag->next;
			} else if (tag->next) {
				_heap_first = tag->next;
			} else {
				_heap_first = NULL;
			}
	
			/* last tag? */
			if (tag->next) {
				tag->next->prev = tag->prev;
			} else {
				_heap_extent -= tag->size + HEAP_TAG_SIZE;
			}

			/* mark the tag as invalid */
			tag->next = (void*)-1;
			tag->prev = (void*)-1;
			tag->size = -1;
		} else {
			c_printf("_kfree: double free or heap corruption (ptr=%08x)\n", ptr);
			_kpanic("heap", "", FAILURE);
		}
	}
}

/*
** Resize a heap allocation on the kernel heap. Like realloc().
*/
void *_krealloc(void *ptr, Uint32 size) {
	Heapbuf *tag;
	void *new;

	/* don't do anything if the input doesn't make sense */
	if (!size || size > HEAP_CHUNK_SIZE*KERNEL_MAX_CHUNKS) {
		return ptr;
	}

	/* if we got NULL, then just do malloc */
	if (!ptr) {
		return _kmalloc(size);
	}

	tag = (ptr - HEAP_TAG_SIZE);

	/* are we increasing the size or decreasing? */
	if (size < tag->size) {
		/* decreasing is easy, just set the size in the tag and return */
		tag->size = size;
		return ptr;
	} else if (tag->next == NULL || (void*)(tag->next) - (void*)(tag->buf) >= size) {
		/* in this case, there's room to grow the current buffer */
		tag->size = size;
		return ptr;
	} else {
		/* unfortunately we need to copy in this case */
		new = _kmalloc(size);

		if (!new) {
			_kfree(ptr);
			return NULL;
		}

		_kmemcpy(new, ptr, tag->size);

		_kfree(ptr);
		return new;
	}
}


/*
** User heap syscall helpers
*/

/*
** Create a new heap for a process
*/
Status _heap_create(struct pcb *pcb) {
	Status status;

	if (!pcb) {
		return BAD_PARAM;
	}

	status = _mman_alloc_at(pcb, (void*)USER_HEAP_BASE, HEAP_CHUNK_SIZE, MAP_USER | MAP_WRITE | MAP_ZERO);
	if (status == SUCCESS) {
		pcb->heapinfo.count = 1;
	}

	return status;
}

/*
** Grow a process's heap
*/
Status _heap_grow(struct pcb *pcb) {
	Heapinfo *info;
	void *ptr;
	Uint32 flags;
	Status status;

	if (pcb) {
		info = &pcb->heapinfo;
		ptr = (void*)(USER_HEAP_BASE + HEAP_CHUNK_SIZE * pcb->heapinfo.count);
		flags = MAP_WRITE | MAP_USER | MAP_ZERO;
	} else {
		info = &_kheapinfo;
		ptr = (void*)(KERNEL_HEAP_BASE + HEAP_CHUNK_SIZE * _kheapinfo.count);
		flags = MAP_WRITE;
	}

	status = _mman_alloc_at(pcb, ptr, HEAP_CHUNK_SIZE, flags);
	if (status == SUCCESS) {
		info->count++;
	}

	return status;
}

/*
** Destroy a process's heap
*/
Status _heap_destroy(struct pcb *pcb) {
	Status status;

	if (!pcb) {
		return BAD_PARAM;
	}

	status = _mman_free(pcb, (void*)USER_HEAP_BASE, HEAP_CHUNK_SIZE * pcb->heapinfo.count);
	if (status == SUCCESS) {
		pcb->heapinfo.count = 0;
	}

	return status;
}
