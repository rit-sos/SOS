
/*
**
** File:	ioreq.h
**
** Author:	Andrew LeCain
**
** Contributor:
**
** Description: IO request queue for ata module
*/

#ifndef _IOREQ_H
#define _IOREQ_H

#include "fd.h"
#include "ata.h"


typedef enum Iostate{
	WAIT_ON_IDLE,
	WAIT_ON_DATA,
	DONE
}Iostate;

typedef struct Iorequest{
	Drive	*d;
	Fd		*fd;
	Iostate	st;
	Uint8	read;
}Iorequest;


/*
 * _ioreq_dealloc -> return an io request to the pool of free requests
 *
 *  usage: after a request has been completely handled, notify caller
 *           then call _ioreq_dealloc(req)
 */
void _ioreq_dealloc(Iorequest *req);
/*
 * _ioreq_handle_idle_request -> pop the next idle request for specified
 *               drive off the queue and handle it. To be called when the
 *               drive is free to serve idle requests
 *
 *  usage: _ioreq_handle_idle_request() (probably in an ISR)
 */
void _ioreq_handle_idle_request(Drive *d);
/*
 * _ioreq_handle_data_request -> pop the next data request for the specified
 *               drive off the queue and handle it. TO be called when the
 *               drive is ready to supply data for a data request
 *
 *  usage: _ioreq_handle_data_request() (probably in an ISR)
 */
void _ioreq_handle_data_request(Drive *d);
/*
 * _ioreq_enqueue -> add a request to the queue for a specified drive
 *
 *  usage: _ioreq_enqueue(drive, fd, 1, WAIT_ON_IDLE); //for read
 *         _ioreq_enqueue(drive, fd, 0, WAIT_ON_IDLE); //for write
 */
int _ioreq_enqueue(Drive *d, Fd *fd, Uint8 rd,Iostate st);
/*
 * _ioreq_init -> initialize the queues for the iorequest system
 *
 *  usage: _ioreq_init();
 */
void _ioreq_init(void);

#endif
