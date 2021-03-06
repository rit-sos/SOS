/*
** umem.c
**
** User-space memory library functions.
**
** Corey Bloodstein (cmb4247)
*/

#include "headers.h"
#include "heaps.h"

/*
** The heap manager forms a doubly linked list of allocation nodes (Heapbufs).
** Each Heapbuf has a pointer to the next and previous allocation in the list.
** When doing a new allocation, the heap manager searches from the beginning
** of the list, looking at the difference between the end of each heap buffer
** and the beginning of the next heap tag to see if space exists to insert.
** If no such space exists, it will append the allocation to the end of the list,
** expanding the heap with grow_heap() if necessary.
*/

/*
** Private heap management globals
*/

static void *heap_start_ptr;
static void *heap_end_ptr;
static Heapbuf *heap_first;
static unsigned int heap_extent;


/*
** Public buffer manipulation functions
*/

/*
** Set each byte of a buffer to a given byte value.
*/
void *memset(void *ptr, int byte, unsigned int size) {
	int i = size;
	unsigned char b = byte & 0xff;
	void *out = ptr;

	while (i) {
		*(unsigned char*)(ptr++) = b;
		--i;
	}

	return out;
}

/*
** Copy a buffer to another buffer.
*/
void *memcpy(void *dst, void *src, unsigned int size) {
	int i = size;
	void *out = dst;

	while (i) {
		*(unsigned char*)(dst++) = *(unsigned char*)(src++);
		--i;
	}

	return out;
}


/*
** Public heap management functions
*/

void heap_init(void) {
	unsigned int size;
	unsigned int status;

	heap_first = NULL;

	if ((status = get_heap_base(&heap_start_ptr)) != SUCCESS) {
		puts("heap_init: get_heap_base: ");
		putx(status);
		puts("\n");
		exit();
	}

	if ((status = get_heap_size(&size)) != SUCCESS) {
		puts("heap_init: get_heap_size: ");
		putx(status);
		puts("\n");
		exit();
	}

	heap_end_ptr = heap_start_ptr + size;

	heap_extent = 0;
}

/*
** malloc - allocate memory on the heap
**
** usage: ptr = malloc(size);
**
** Simple, slow first-fit allocator.
*/
void *malloc(unsigned int size) {
	void *ptr;
	Heapbuf *tag, *prev, *curr;
	unsigned int real_size;
	unsigned int heap_grow_size;

	if (size == 0) {
		return NULL;
	}

	/* add the size of the heap tag to the allocation size */
	real_size = size + HEAP_TAG_SIZE;

	/* align the tag */
	if (real_size & 0x03) {
		real_size += 4 - (real_size & 0x03);
	}

	putx(real_size);
	puts("\n");

	/* sanity check: is there any chance we can fit this allocation? */
	if (real_size > HEAP_CHUNK_SIZE * USER_MAX_CHUNKS) {
		return NULL;
	}

	if (!heap_first) {
		/* case #1: there are no other heap allocations */
		ptr = heap_start_ptr;
		prev = NULL;
		curr = NULL;
	} else if ((void*)heap_first - heap_start_ptr >= real_size) {
		/* case #2: insert before the head */
		ptr = heap_start_ptr;
		prev = NULL;
		curr = heap_first;
	} else {
		prev = heap_first;
		curr = heap_first->next;

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
		if (ptr + real_size > heap_end_ptr) {
			/* is there any hope of fitting it? */
			if (ptr + real_size <= (void*)(USER_HEAP_BASE+HEAP_CHUNK_SIZE*USER_MAX_CHUNKS)-heap_extent) {
				/* try to grow the heap */
				do {
					if (grow_heap(&heap_grow_size) != SUCCESS) {
						return NULL;
					} else {
						heap_end_ptr = heap_start_ptr + heap_grow_size;
					}
				} while(ptr + real_size > heap_end_ptr);
			} else {
				/* give up before we ask for tons of memory we don't need */
				return NULL;
			}
		}

		/* update the extent */
		heap_extent += real_size;
	}

	/* set up the new heap tag and return a pointer to the new buffer */
	tag = (Heapbuf*)ptr;

	tag->prev = prev;
	tag->next = curr;

	if (prev) {
		prev->next = tag;
	} else {
		heap_first = tag;
	}

	if (curr) {
		curr->prev = tag;
	}

	tag->size = size;

	return tag->buf;
}

void *realloc(void *ptr, unsigned int size) {
	Heapbuf *tag;
	void *new;

	/* don't do anything if the input doesn't make sense */
	if (!size || size > HEAP_CHUNK_SIZE*USER_MAX_CHUNKS) {
		return ptr;
	}

	/* if we got NULL, just do malloc */
	if (!ptr) {
		return malloc(size);
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
		new = malloc(size);

		if (!new) {
			free(ptr);
			return NULL;
		}

		memcpy(new, ptr, tag->size);

		free(ptr);
		return new;
	}
}

void free(void *ptr) {
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
				heap_first = tag->next;
			} else {
				heap_first = NULL;
			}
	
			/* last tag? */
			if (tag->next) {
				tag->next->prev = tag->prev;
			} else {
				heap_extent -= tag->size + HEAP_TAG_SIZE;
			}

			/* mark the tag as invalid */
			tag->next = (void*)-1;
			tag->prev = (void*)-1;
			tag->size = -1;
		} else {
			puts("free: double free or heap corruption (ptr=");
			putx((unsigned int)ptr);
			puts("), abort\n");
			exit();
		}
	}
}

Status shm_create(const char *name, unsigned int min_size, unsigned int flags, void **ptr) {
	return real_shm_create(name, strlen(name), min_size, flags, ptr);
}

Status shm_open(const char *name, void **ptr) {
	return real_shm_open(name, strlen(name), ptr);
}

Status shm_close(const char *name) {
	return real_shm_close(name, strlen(name));
}


