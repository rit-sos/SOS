

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


void _ioreq_dealloc(Iorequest *req);
void handle_idle_request(Drive *d);
void handle_data_request(Drive *d);
int _ioreq_enqueue(Drive *d, Fd *fd, Uint8 rd,Iostate st);

void _ioreq_init(void);

#endif
