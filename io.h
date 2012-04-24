
/*
**
** File:	io.h
**
** Author:	Andrew LeCain	
**
** Contributor:
**
** Description:	info about the file descriptors, etc
*/

#ifndef _IO_H
#define _IO_H

#ifndef __ASM__20113__

// only pull these in if we're not in assembly language

#define SIO_FD 		1	//define default file descriptors for serial I/O 
#define CIO_FD	 	0	//define default file descriptors for console I/O

#define	FD_BUF_SIZE	512

#ifdef __KERNEL__20113__


#else

// User mode needs only the user library headers
#endif

#endif

#endif
