/*
** SCCS ID:	@(#)ulib.h	1.1	4/5/12
**
** File:	ulib.h
**
** Author:	4003-506 class of 20113
**
** Contributor:
**
** Description:	User-level library definitions
*/

#ifndef _ULIB_H
#define _ULIB_H

#include "headers.h"
#include "io.h"

/*
** General (C and/or assembly) definitions
*/

#define	PRIO_HIGH	0
#define	PRIO_STD	1
#define	PRIO_LOW	2
#define	PRIO_IDLE	3

#define	N_PRIOS		4

#ifndef __ASM__20113__

/*
** Start of C-only definitions
*/

#include "types.h"
#include "clock.h"
#include "pcbs.h"
#include "u_windowing.h"

// pseudo-function:  sleep for a specified number of seconds

#define	sleep(n)	msleep(SECONDS_TO_TICKS(n))

// pseudo-function:  return a string describing a Status value

#define ustatus(n) \
	((n) >= STATUS_SENTINEL ? "bad status value" : ustatus_strings[(n)])

/*
** Types
*/

/*
** Node in a list of heap allocations, describing a single allocation.
*/
typedef struct tagHeapbuf{
	/* size of the heap array */
	int size;
	struct tagHeapbuf *prev;
	struct tagHeapbuf *next;
	/* the end of the heap array is heapbuf.buf + heapbuf.size */
	unsigned char buf[1];
} __attribute__((aligned(4))) Heapbuf;

#define HEAP_TAG_SIZE	(offsetof(struct tagHeapbuf, buf))

/*
** Globals
*/

// Status value strings

extern const char *ustatus_strings[];

/*
** Prototypes
*/

/*
** fork - create a new process
**
** usage:  status = fork(&pid);
**
** returns:
**      PID of new process via the info pointer (in parent)
**      0 via the info pointer (in child)
**      status of the creation attempt
*/

Status fork(unsigned int *pid);

/*
** exit - terminate the calling process
**
** usage:	exit();
**
** does not return
*/

Status exit(void);
/*
** fopen- open a file 
**
** usage:	status = fopen(sectorstart, length, int *fd);
**
**
** returns:
**      file descriptor refering to file
**      status of the operation
*/
Status fopen(Uint64 sectorstart, Uint16 length, int *fd);

/*
** fclose- close an open file 
**
** usage:	status = fopen(int *fd);
**
**
** returns:
**      status of the operation
*/
Status fclose(int fd);
/*
** read - read a single character from the SIO
**
** usage:	status = read(&buf);
**
** blocks the calling routine if there is no character to return
**
** returns:
**      the character via the info pointer
**      status of the operation
*/

Status read( int fd, int *buf);

/*
** write - write a single character to the SIO
**
** usage:	status = write(ch);
**
** returns:
**      status of the operation
*/

Status write(int fd, char buf);


/*
** msleep - put the current process to sleep for some length of time
**
** usage:	status = msleep(ms);
**
** if the sleep time (in milliseconds) is 0, just preempts the process;
** otherwise, puts it onto the sleep queue for the specified length of
** time
**
** returns:
**      status of the sleep attempt
*/

Status msleep(unsigned int ms);

/*
** kill - terminate a process with extreme prejudice
**
** usage:	status kill(pid);
**
** returns:
**      status of the termination attempt
*/

Status kill(Pid pid);

/*
** get_priority - retrieve the priority of the current process
**
** usage:	status = get_priority(&prio);
**
** returns:
**      the process' priority via the info pointer
**      SUCCESS
*/

Status get_priority(unsigned int *prio);

/*
** get_pid - retrieve the PID of the current process
**
** usage:	status = get_pid(&pid);
**
** returns:
**      the process' pid via the info pointer
**      SUCCESS
*/

Status get_pid(unsigned int *pid);

/*
** get_ppid - retrieve the parent PID of the current process
**
** usage:	status = get_ppid(&pid);
**
** returns:
**      the process' parent's pid via the info pointer
**      SUCCESS
*/

Status get_ppid(unsigned int *pid);

/*
** get_time - retrieve the current system time
**
** usage:	status = get_time(&time);
**
** returns:
**      the process' pid via the info pointer
**      SUCCESS
*/

Status get_time(unsigned int *time);

/*
** get_state - retrieve the state of the current process
**
** usage:	status = get_state(&state);
**
** returns:
**      the process' state via the info pointer
**      SUCCESS
*/

Status get_state(unsigned int *state);

/*
** set_priority - change the priority of the current process
**
** usage:	status = set_priority(prio);
**
** returns:
**      success of the change attempt
*/

Status set_priority(unsigned int prio);

/*
** set_time - change the current system time
**
** usage:	status = set_time(time);
**
** returns:
**      SUCCESS
*/

Status set_time(unsigned int time);

/*
** exec - replace a process with a different program
**
** usage:	status = exec(entry);
**
** returns:
**	does not return (if the attempt succeeds)
**      failure status of the replacement attempt (if the attempt fails)
*/

//Status exec(void (*entry)(void));
Status exec(unsigned int entry_id);

/*
** s_windowing_get_window	-	Request a window for this process
**
** returns:	
**		SUCCESS	if window was reservered
**		FAILURE	otherwise
*/
Status s_windowing_get_window(Window *win);

/*
** s_windowing_free_window	-	Frees the specified window
**
** returns:	SUCCESS
*/
Status s_windowing_free_window(Window win);

/*
** s_windowing_print_str	- display a string on the monitor
**
** returns:	SUCCESS
*/
Status s_windowing_print_str(Window win, int x, int y, const char *str);

/*
** s_windowing_print_char	- display a character on the monitor
**
** returns:	SUCCESS
*/
Status s_windowing_print_char(Window win, int x, int y, const char c);

/*
** s_windowing_clearscreen	- clear the display
**
** returns:	SUCCESS
*/
Status s_windowing_clearscreen(Window win, char r, char g, char b);

/*
** s_windowing_draw_line - draws a line
**
** returns:	SUCCESS
*/
Status s_windowing_draw_line(Window win, Uint x0, Uint y0, Uint x1, Uint y1, char r, char g, char b);

/*
** s_windowing_copy_rect - copys part of user frame buffer into video memory
**
** returns:	SUCCESS
*/
Status s_windowing_copy_rect(Window win, Uint x0, Uint y0, Uint w, Uint h, Uint8 *buf);

/*
** s_map_framebuffer - map the framebuffer into memory
**
** returns:	SUCCESS
*/
Status s_map_framebuffer( Uint32 **framebuffer );

/*
** bogus - a bogus system call, for testing our syscall ISR
**
** usage:	bogus();
*/

void bogus( void );

/*
** prt_status - print a status value to the console
**
** usage:  prt_status(msg,code);
**
** the 'msg' argument should contain a %s where
** the desired status value should be printed
*/

//void prt_status( char *msg, Status stat );

/*
** spawn - create a new process running a different program
**		at standard priority
**
** usage:  status = spawn( &pid, entry );
*/

//Status spawn( unsigned int *pid, void (*entry)(void) );
Status spawn(unsigned int *pid, unsigned int entry_id);

/*
** spawnp - create a new process running a different program at
**		a specific process priority
**
** usage:  status = spawnp( &pid, prio, entry );
*/

//Status spawnp( unsigned int *pid, unsigned int prio, void (*entry)(void) );
Status spawnp(unsigned int *pid, unsigned int prio, unsigned int entry_id);

/*
** grow_heap - expand the heap region
**
** usage: status = grow_heap();
*/
Status grow_heap(unsigned int *size);

/*
** get_heap_size - expand the heap region
**
** usage: status = get_heap_size();
*/
Status get_heap_size(unsigned int *size);

/*
** get_heap_base - get a pointer to the beginning of the user heap region
**
** usage: status = get_heap_base();
*/
Status get_heap_base(void *base);

/*
** write_buf - write a string from a user buffer
**
** usage: status = write_buf(str);
*/
Status write_buf(const char *str, unsigned int size);

/*
** sys_sum - print the sum of an array of integers
**
** usage: status = sys_sum(buf, count);
*/
Status sys_sum(int *numbers, int count);

/*
** set_test - get a large array of integers back from the kernel
**
** usage: status = set_test(buf, count);
*/
Status set_test(int *buf, int count);

/*
** Create and open a new shared memory region
*/
Status shm_create(const char *name, unsigned int min_size, unsigned int flags, void **ptr);
Status real_shm_create(const char *name, unsigned int name_len, unsigned int min_size, unsigned int flags, void **ptr);

/*
** Open (map and addref) an existing shared memory region
*/
Status shm_open(const char *name, void **ptr);
Status real_shm_open(const char *name, unsigned int name_len, void **ptr);

/*
** Release a shared memory region
*/
Status shm_close(const char *name);
Status real_shm_close(const char *name, unsigned int name_len);

/*
** Set each byte of a buffer to a given byte value.
*/
void *memset(void *ptr, int byte, unsigned int size);

/*
** Copy a buffer to another buffer.
*/
void *memcpy(void *dst, void *src, unsigned int size);

/*
** heap_init - set up the user-space heap allocator
**
** usage: Called by ustrap, other code shouldn't call.
** Further calls will cause the heap manager to forget all heap allocations,
** making use of previously allocated heap buffers unsafe.
*/
void heap_init(void);

/*
** malloc - allocate memory on the heap
**
** usage: ptr = malloc(size);
*/
void *malloc(unsigned int size);

/*
** free - release memory on the heap
**
** usage: free(ptr);
*/
void free(void *ptr);

/*
** realloc - allocate a larger or smaller buffer for the same data
**
** usage: ptr = realloc(ptr, new_size);
*/
void *realloc(void *ptr, unsigned int size);

/*
** strlen - get the offset of the first '\0' in a buffer
**
** usage: length = strlen(str);
*/
int strlen(const char *str);

Status puts(const char *str);
Status fputs(int fd, const char *str);
void putx(unsigned int x);

/*
** put_char_or_code - write a single printable character or its code to the file descriptor
**
** usage:	put_char_or_code(fd, ch);
**
*/
void put_char_or_code(int fd, int ch );


/*
** readInt- reads from the file descriptor until a number followed by \n is read.
**
** usage:	in = readInt(fd);
**
*/
int readInt(int fd);

#endif

#endif
