
/*
 **
 ** File:	ioreq.c
 **
 ** Author:	Andrew LeCain
 **
 ** Contributor:
 **
 ** Description:	hard disk IO queue
 */

#define	__KERNEL__20113__

#include "fd.h"
#include "ata.h"
#include "ata_pio.h"
#include "ioreq.h"
#include "queues.h"

Iorequest iorequest_pool[ATA_MAX_REQUESTS];
Queue	*idle_q;
Queue	*data_q;
Queue	*free_req_q;

void _ioreq_handle_idle_request(Drive *d){
	Status status;
	Key key;
	Iorequest *req;

	key.v = (void *) d;
	status = _q_remove_by_key( idle_q, (void **) &req, key );
	if(status == NOT_FOUND || status == EMPTY_QUEUE){
		d->state=IDLE;
		req = NULL; //wasn't found
		return;
	} else if (status != SUCCESS){
		_kpanic("handle_idle_request", "remove status", status);
	}

	if (! (_ata_pio_read_status(d) & (BSY | DRQ)) ){
		if (req->read){
			//send read request and add the data queue
			d->state=READING;
			req->st = WAIT_ON_DATA;
			_q_insert(data_q, (void *) req, key);
			_ata_pio_send_read(req->fd->device_data);
		} else {
			//transfer data and send write reqest
			d->state=WRITING;
			_ata_pio_send_write(req->fd->device_data);
			_ata_pio_write_fd_block(req->fd);
			_fd_writeDone(req->fd);
			_ioreq_dealloc(req);
		}
	}else {
		//no idle request on disk, despite removing from idle queue.. this shouldn't happen
		_q_insert(idle_q, (void *) req, key);
	}
}

void _ioreq_handle_data_request(Drive *d){
	Status status;
	Key key;
	Iorequest *req;

	key.v = d;
	status = _q_remove_by_key( data_q, (void **) &req, key );
	if(status == NOT_FOUND || status == EMPTY_QUEUE){
		req = NULL; //wasn't found
		//no data request on disk. Unsolicited data
		int i,j;
		for (i =0; i< 4;i++){
			for (j=0;j<4;j++){
				if (&_busses[i].drives[j]==d){
					c_printf("bus[%d].drive[%d] %s",i,j,d->model);
				}
			}
		}
		_kpanic("handle_data_request", "Got unsolicited data.",FAILURE);
		return;
	}

	if (req->st == WAIT_ON_DATA){
		if (req->read){
			//send read request and add the data queue
			req->d->state=IDLE;
			_ata_read_finish(req->fd);
			_ioreq_dealloc(req);
		} else {
			//Unsolicited data...
			_kpanic("handle_data_request", "Got unsolicited, but waiting to write only.",FAILURE);
		}
	}else {
		//no data request on disk. Unsolicited data
		_kpanic("handle_data_request", "Got unsolicited data.",FAILURE);
	}
}



void _ioreq_dealloc(Iorequest *req){
	Key key;
	Status status;
	key.v = req->d;

	status = _q_insert(free_req_q, (void *) req,key);
	if( status != SUCCESS ) {
		_kpanic( "_ioreq_init", "IO free queue insertion status %s", status );
	}
}


int _ioreq_enqueue(Drive *d, Fd *fd, Uint8 rd, Iostate state){
	Status status;
	Key key;
	Iorequest *req;

	if( (status = _q_remove( free_req_q, (void **) &req ))!= SUCCESS ) {
		_kpanic( "_ioreq_enqueue", "No free requests. Status: %s", status );
	}

	key.v =(void *) d;

	req->d=d;
	req->fd=fd;
	req->read=rd;
	req->st=state;

	if (state == WAIT_ON_IDLE){
		status = _q_insert( idle_q,(void *) req, key);
	}else if (state == WAIT_ON_DATA){
		status = _q_insert( data_q,(void *) req, key);
	}else{
		//drop invalid request
		status=FAILURE;
	}

	if( status != SUCCESS ) {
		_kpanic( "_ioreq_enqueue", "IO queue insertion status %s", status );
	}

	return 1;
}


void _ioreq_init(void){
	int i;
	Status status;

	status = _q_alloc( &data_q, &_comp_ascend_uint );
	if( status != SUCCESS ) {
		_kpanic( "_ioreq_init", "IO read queue alloc status %s", status );
	}
	status = _q_alloc( &idle_q, &_comp_ascend_uint );
	if( status != SUCCESS ) {
		_kpanic( "_ioreq_init", "IO write queue alloc status %s", status );
	}
	status = _q_alloc( &free_req_q, NULL);
	if( status != SUCCESS ) {
		_kpanic( "_ioreq_init", "IO free queue alloc status %s", status );
	}

	for (i =0 ; i < ATA_MAX_REQUESTS; i++){
		_ioreq_dealloc(&iorequest_pool[i]);
	}

}
