
/*
 **
 ** File:	ata.c
 **
 ** Author:	Andrew LeCain
 **
 ** Contributor:
 **
 ** Description:	ata module
 */

#define	__KERNEL__20113__

#include "headers.h"

#include <x86arch.h>
#include <startup.h>
#include "pci.h"
#include "ata.h"
#include "fd.h"
#include "c_io.h"
#include "ioreq.h"
#include "ata_pio.h"


/*
 * Private definitions
 */


#define ATA_MAX_FILES 	16
#define SECTOR_SIZE	512

/*
 *PCI device codes
 */

#define PCI_DEVICE_CLASS_MASS_STORAGE 	0x01
/*
 **IO ports
 */

//#define DEVICE_CONTROL	0x3F6
#define DEVICE_CONTROL	0x374


/*
 **Identify packet fields
 **
 */

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

/*
 ** PRIVATE DATA TYPES
 */


/*
 ** PRIVATE GLOBAL VARIABLES
 */

Ata_fd_data fd_dat_pool[ATA_MAX_FILES];
static Queue *free_dat_queue;

/*
 ** PUBLIC GLOBAL VARIABLES
 */

/*
 ** PRIVATE FUNCTION DECLARATIONS
 */


Status fd_dat_dealloc(Ata_fd_data *toFree);
Ata_fd_data *fd_dat_alloc(void);

/*
 ** PRIVATE FUNCTIONS
 */

Status fd_dat_dealloc(Ata_fd_data *toFree){
	Key key;
	if(toFree == NULL){
		return( BAD_PARAM );
	}
	key.i = 0;
	return( _q_insert( free_dat_queue, (void *) toFree, key) );
}

Ata_fd_data *fd_dat_alloc(void){
	Ata_fd_data *fd_dat;
	if( _q_remove( free_dat_queue, (void **) &fd_dat ) != SUCCESS ) {
		fd_dat = NULL;
	}
	return fd_dat;
}


void _ata_debug(char *fmt, ...){
#ifdef DEBUG_ATA
	c_printf(fmt);
#endif
}

/*
 * Helper function to select the drive and delay
 *
 */

Status _ata_read_finish(Fd* fd){
	char *data;
	int i;
	Ata_fd_data *dev_data;
	dev_data=(Ata_fd_data *)fd->device_data;
	data=(char *)block;

	_ata_pio_read_block(block, dev_data->d);

	for(i =0;i<SECTOR_SIZE;i++){
		_fd_readBack(fd,data[i]);
	}

	dev_data->read_sector++;
	_fd_readDone(fd);
	return SUCCESS;
}

/*
 ** PUBLIC FUNCTIONS
 */

void _isr_ata( int vector, int code ) {
	volatile unsigned char status = 0;
	int i,j;
	Drive *d;

	//if this is the secondary bus interrupt, pretend it's the first.
	if (vector == 0x2F) vector=0x2E ;

	for(i=0;i<ATA_MAX_BUSSES;i++){
		if (_busses[i].interrupt_line == vector)
			break;
	}

	//read the busmaster status
	_ata_pio_read_busmaster(&_busses[i]);

	for(j=0;j<ATA_DRIVES_PER_BUS;j++){
		d= &_busses[i].drives[j];
		status = _ata_pio_read_status(d);

		if(d->type != INVALID_DRIVE && (status & ERR || status & DF) ){
			//crap.. 
			c_printf("isr: vector %x code %x\n", vector, code);
			c_printf("b[%d].d[%d] Status %x\n",i,j,status);

			_ata_pio_disableIRQ(d);
			d->type = INVALID_DRIVE;
		}

		if(status & DRQ && !(status & BSY) ){ //drive has provided data
			d->state=READING;
			handle_data_request(d);
		}
		if (status == 0x50 && !(status & BSY)){ // drive is idle
			d->state=IDLE;
			handle_idle_request(d);
		}
	}
	__outb( PIC_MASTER_CMD_PORT, PIC_EOI );
	__outb( PIC_SLAVE_CMD_PORT, PIC_EOI );
}


Fd* _ata_fopen(Drive *d, Uint64 sector, Uint16 len, Flags flags){
	Fd* fd;
	Ata_fd_data *dev_data;
	if(d->type == INVALID_DRIVE){
		c_printf("Invalid drive!");
		return NULL;
	}

	fd=_fd_alloc(flags);
	if(fd==NULL){
		return NULL;
	}

	//initialize the device data structure so we know where in the file
	//to read from
	dev_data=fd_dat_alloc();
	dev_data->d=d;
	dev_data->sector_start=sector;
	dev_data->sector_end=sector+len;
	dev_data->read_sector=sector;
	dev_data->write_sector=sector;
	fd->device_data=dev_data;

	if (flags & FD_R){
		fd->startRead=&_ata_read_start;
	}else{
		fd->startRead=NULL;
	}
	if(flags & FD_W){
		fd->startWrite=&_ata_write_start;
	}else{
		fd->startWrite=NULL;
	}

	fd->rxwatermark = 1;
	fd->txwatermark = SECTOR_SIZE;

	return fd;
}

Status _ata_fclose(Fd *fd){
	if(fd->flags == FD_UNUSED){
		return FAILURE;
	}

	_ata_flush(fd);
	fd_dat_dealloc(fd->device_data);
	_fd_dealloc (fd);
	return SUCCESS;

}

Status _ata_flush(Fd *fd){
	int i;
	char *data;
	int blocks_rw;
	Ata_fd_data *dev_data;

	dev_data=(Ata_fd_data *)fd->device_data;
	data=(char *)block;

	_ata_pio_disableIRQ(dev_data->d);

	if(fd->write_index % SECTOR_SIZE){
		blocks_rw = read_raw_blocking(dev_data->d,
				dev_data->write_sector,
				1, block);
		if(blocks_rw != 1){
			c_puts("error reading back block on writeback");
			_ata_pio_enableIRQ(dev_data->d);
			return FAILURE;
		}
		for(i = 0 ; i < fd->write_index % SECTOR_SIZE;i++){
			data[i]= _fd_getTx(fd);
		}
		blocks_rw = write_raw_blocking(dev_data->d,
				dev_data->write_sector,
				1, block);
		if(blocks_rw != 1){
			c_puts("error writing block on writeback");
			_ata_pio_enableIRQ(dev_data->d);
			return FAILURE;
		}
	}
	_ata_pio_enableIRQ(dev_data->d);
	return SUCCESS;
}

Status _ata_read_start(Fd* fd){
	Ata_fd_data *dev_data;
	dev_data=(Ata_fd_data *)fd->device_data;

	if(dev_data->d->type == INVALID_DRIVE){
		fd->flags |= FD_EOF;
		return EOF;
	}

	if(dev_data->read_sector >= dev_data->sector_end){
		fd->flags |= FD_EOF;
		return EOF;
	}


	_ioreq_enqueue(dev_data->d, fd , 1, WAIT_ON_IDLE);
	_ata_pio_enableIRQ(dev_data->d);

	if (! (_ata_pio_read_status(dev_data->d) & BSY)){
		handle_idle_request(dev_data->d);
	}

	//block until the drive is ready.. TODO: block on drive
	//while (read_status(dev_data->d) & (BSY | DRQ)); //spin until not busy

	return SUCCESS;
}

Status _ata_write_start(Fd* fd){
	Ata_fd_data *dev_data;
	dev_data=(Ata_fd_data *)fd->device_data;

	if(dev_data->write_sector >= dev_data->sector_end){
		fd->flags |= FD_EOF;
		return EOF;
	}
	_ioreq_enqueue(dev_data->d, fd, 0, WAIT_ON_IDLE);
	_ata_pio_enableIRQ(dev_data->d);
	
	if (! (_ata_pio_read_status(dev_data->d) & BSY)){
		handle_idle_request(dev_data->d);
	}

	return SUCCESS;
}


Status _ata_read_blocking(Fd* fd){
	Ata_fd_data *dev_data;
	int blocks_read,i;
	char *data;
	dev_data=(Ata_fd_data *)fd->device_data;

	if(dev_data->d->type == INVALID_DRIVE){
		fd->flags |= FD_EOF;
		return EOF;
	}

	if(dev_data->read_sector >= dev_data->sector_end){
		fd->flags |= FD_EOF;
		return EOF;
	}

	blocks_read = read_raw_blocking(dev_data->d,
			dev_data->read_sector,
			1,
			block);
	data=(char *)block;

	if (blocks_read < 0) blocks_read=0;
	dev_data->read_sector+=blocks_read;
	for(i =0;i<blocks_read*SECTOR_SIZE;i++){
		_fd_readBack(fd,data[i]);
	}
	_fd_readDone(fd);
	return SUCCESS;
}

Status _ata_write_blocking(Fd* fd){

	Ata_fd_data *dev_data;
	int blocks_wrote,i;
	char *data;
	dev_data=(Ata_fd_data *)fd->device_data;

	if(dev_data->write_sector >= dev_data->sector_end){
		fd->flags |= FD_EOF;
		return EOF;
	}

	data=(char *)block;

	for(i =0;i<SECTOR_SIZE;i++){
		data[i]= _fd_getTx(fd);
	}

	blocks_wrote = write_raw_blocking(dev_data->d,
			dev_data->write_sector,
			1,
			block);

	if (blocks_wrote < 0) blocks_wrote=0;

	//if we just wrote to the sector we last read from, we made our FD dirty
	if(dev_data->write_sector == dev_data->read_sector-1 && fd->write_index > fd->read_index){
		//we just overwrote the sector we are reading from. Update said data
		_fd_flush_rx(fd);
		for (i=fd->read_index % SECTOR_SIZE; i < SECTOR_SIZE; i++){
			_fd_readBack(fd,data[i]);
		}
	}

	dev_data->write_sector+=blocks_wrote;
	_fd_writeDone(fd);
	return SUCCESS;
}

//read sector into buffer and return number of sectors read 
int read_raw_blocking(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf ){
	volatile unsigned char status = 0;
	Uint16 i;

	if(d->type== INVALID_DRIVE){
		c_printf("Invalid drive\n");
		return -1;
	}

	if (sector > d->sectors){
		c_printf("Invalid sector\n");
		//tried to read off the end of disk. 
		return -1;	
	}

	while (_ata_pio_read_status(d) & (BSY | DRQ)); //spin until not busy. TODO:add to blocked queue

	switch(d->type){
		case LBA48:
			_ata_pio_lba48_set_sectors(d, sector, sectorcount);
			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			_ata_pio_send_command(d,READ_SECTORS_EXT);
			//for each sector
			for(i=0;i<sectorcount;i++){
				//read status until error or DRQ
				do {
					status = _ata_pio_read_status(d);
				}while(status & BSY);

				if(status & (ERR | DF) || !(status & DRQ) ){
					//there was an error requesting
					c_printf("read error. status: %x\n",status);
					return -1;
				}
				_ata_pio_read_block(&buf[i*256],d);
			}
			break;

		case LBA28:
			_ata_pio_lba28_set_sectors(d, sector, sectorcount);
			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			_ata_pio_send_command(d,READ_SECTORS);
			//for each sector
			for(i=0;i<sectorcount;i++){
				//read status until error or DRQ
				do {
					status = _ata_pio_read_status(d);
				}while(status & BSY);

				if(status & (ERR | DF) || !(status & DRQ) ){
					//there was an error requesting
					c_printf("read error. status: %x\n",status);
					return -1;
				}
				_ata_pio_read_block(&buf[i*256],d);
			}

			break;

		case CHS:
			break;

		default:
			//invalid drive
			return -1;
	}
	return i;
}

int write_raw_blocking(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf ){
	volatile unsigned char status = 0;
	Uint16 i;

	if(d->type== INVALID_DRIVE){
		return -1;
	}

	if (sector > d->sectors){
		//tried to read off the end of disk.
		return -1;
	}

	while (_ata_pio_read_status(d) & (BSY | DRQ)); //spin until not busy. TODO:add to blocked queue

	switch(d->type){
		case LBA48:
			_ata_pio_lba48_set_sectors(d, sector, sectorcount);
			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			_ata_pio_send_command(d,WRITE_SECTORS_EXT);

			for(i=0;i<sectorcount;i++){
				//read status until error or DRQ
				do {
					status = _ata_pio_read_status(d);
				}while(status & BSY);

				if(status & (ERR | DF) || !(status & DRQ) ){
					//there was an error requesting
					c_printf("write error. status: %x\n",status);
					return -1;
				}
				_ata_pio_write_block(&buf[i*256],d);
			}
			break;

		case LBA28:
			_ata_pio_lba28_set_sectors(d, sector, sectorcount);
			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			_ata_pio_send_command(d,WRITE_SECTORS);

			for(i=0;i<sectorcount;i++){
				//read status until error or DRQ
				do {
					status = _ata_pio_read_status(d);
				}while(status & BSY);

				if(status & (ERR | DF) || !(status & DRQ) ){
					//there was an error requesting
					c_printf("write error. status: %x\n",status);
					return -1;
				}
				_ata_pio_write_block(&buf[i*256],d);
			}

			break;

		case CHS:
			break;

		default:
			//invalid drive
			return -1;

	}

	return i;	

}




void _ata_init(void){
	Status status;
	struct pci_func f;
	f.bus = 0;
	f.slot = 0;
	f.func = 0;

	_ioreq_init();

	int currentBus=0;
	int i,j;

	_ata_primary = &_busses[0].drives[0]; //set primary to something by default

	block = (Uint16 *) _kmalloc(sizeof (Uint16)*256);

	status = _q_alloc( &free_dat_queue, NULL );
	if( status != SUCCESS ) {
		_kpanic( "_ata_init", "File data queue alloc status %s", status );
	}
	for(i=0;i<ATA_MAX_FILES;i++){
		status = fd_dat_dealloc(&fd_dat_pool[i]);
		if( status != SUCCESS ) {
			_kpanic( "_ata_init", "file data queue insert status %s", status );
		}
	}

	for(i =0; i< ATA_MAX_BUSSES;i++){
		for (j=0;j<ATA_DRIVES_PER_BUS;j++){
			_busses[i].drives[j].type=INVALID_DRIVE;
			_busses[i].drives[j].state=IDLE;
		}
	}

	while(f.bus != ATA_MAX_BUSSES){
		//reset the scan

		f.vendor_id = 0xFFFF;
		f.device_id = 0xFFFF;
		f.dev_class = PCI_DEVICE_CLASS_MASS_STORAGE;
		_pci_scan_for(&f,f.bus,f.slot,f.func+1);

		if(f.bus!=PCI_MAX_BUSSES){

			c_printf("\ncontroller found on bus:%d slot:%d function:%d\n", f.bus,f.slot,f.func);
			Uint32 BAR[6];
			BAR[0] = read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR0)&0xFFFE;
			BAR[1] = read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR1)&0xFFFE;
			BAR[2]= read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR2)&0xFFFE;
			BAR[3]= read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR3)&0xFFFE;
			BAR[4]= read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR4)&0xFFFE;

			_busses[currentBus].interrupt_line = read_pci_conf_byte(f.bus,f.slot,f.func,PCI_INTERRUPT_LINE);


			//check if this is an ATA drive (not supported for now)
			if (BAR[0] == 0x01 || BAR[0]==0x00){
				BAR[0]=0x1F0;
				BAR[1]=0x3F4;
			}

			if (BAR[2] == 0x01 || BAR[2]==0x00){
				BAR[2]=0x170;
				BAR[3]=0x374;
			}

			if (BAR[2] == 0x8F0 || BAR[3] ==  0x8F8){
				BAR[2]=0x00;
				BAR[3]=0x00;
			}
			_busses[currentBus].primary_base=BAR[0];
			_busses[currentBus].primary_control=BAR[1]+2;
			_busses[currentBus].secondary_base=BAR[2];
			_busses[currentBus].secondary_control=BAR[3]+2;
			_busses[currentBus].busmaster_base=BAR[4];

			if( BAR[0] == 0x1F0){ //Real ATA controller. Install the correct interrupts
				_busses[currentBus].interrupt_line = 0x2E;
				__install_isr(0x2E,_isr_ata);
				__install_isr(0x2F,_isr_ata);
			}else{
				_busses[currentBus].interrupt_line += 0x20;
				__install_isr(_busses[currentBus].interrupt_line,_isr_ata);
			}
			int i;
			for(i=0;i<5;i++){
				c_printf("BAR[%d] = %x ", i, BAR[i]);
			}
			c_printf("IRQ= %x\n", _busses[currentBus].interrupt_line);

			//TODO: figure out why regular PATA drives aren't working
			if (1 || BAR[0] != 0x1F0){
				c_printf("Bus[%d].drive[%d]:",currentBus, 0);
				_ata_pio_identify(0, BAR[0],BAR[1]+2,&_busses[currentBus].drives[0]);
				c_printf("Bus[%d].drive[%d]:",currentBus, 1);
				_ata_pio_identify(1, BAR[0],BAR[1]+2,&_busses[currentBus].drives[1]);
				if(BAR[2] != 0x00){
					c_printf("Bus[%d].drive[%d]:",currentBus, 2);
					_ata_pio_identify(0, BAR[2],BAR[3]+2,&_busses[currentBus].drives[2]);
					c_printf("Bus[%d].drive[%d]:",currentBus, 3);
					_ata_pio_identify(1, BAR[2],BAR[3]+2,&_busses[currentBus].drives[3]);
				}
			}
			currentBus++;
		}
	}

	_fd_flush_rx(&_fds[CIO_FD]);
	_fd_flush_tx(&_fds[CIO_FD]);

	c_puts( " ATA" );
}
