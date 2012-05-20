
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

#include "support.h"
#include "pci.h"
#include "ata.h"
#include <startup.h>
#include <x86arch.h>
#include "fd.h"
#include "c_io.h"


/*
 * Private definitions
 */


#define ATA_MAX_FILES 	16
#define ATA_MAX_REQUESTS 16
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

#define PRIMARY 	0xF0E0
#define SECONDARY  	0xF0C0

#define CHS_MODE	0xA0
#define LBA_MODE	0x40

/*
 **Port offsets
 */

#define DATA		0x0
#define FEATURES	0x1
#define SECTOR_COUNT	0x2
#define SECTOR_NUMBER	0x3
#define LBA_LOW		0x3
#define CYLINDER_LOW	0x4
#define LBA_MID		0x4
#define CYLINDER_HIGH	0x5
#define LBA_HIGH	0x5
#define DRIVE_HEAD_PORT	0x6
#define REGULAR_STATUS 	0x7
#define COMMAND		0x7

/*
 **Status bits
 */
#define ERR	(1<<0)
#define DRQ	(1<<3)
#define SRV	(1<<4)
#define DF	(1<<5)
#define RDY	(1<<6)
#define BSY	(1<<7)

/*
 **control register bit descriptions
 */
#define NIEN	0x02
#define SRST 	0x04
#define HOB  	0x08

/*
 **Commands
 */

#define IDENTIFY		0xEC
#define READ_SECTORS_EXT 	0x24
#define WRITE_SECTORS_EXT 	0x34
#define FLUSH_CACHE		0xE7

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
typedef struct ata_fd_data{
	Drive *d;
	Uint64 sector_start;
	Uint64 sector_end;
	Uint64 read_sector;
	Uint64 write_sector;
}Ata_fd_data;

typedef struct Identify_packet{
	char devtype[2];
	char cylinders[(6-2)];
	char heads[(12-6)];
	char sectors[(20-12)];
	char serial[(54-20)];
	char model[(98-54)];
	char capabilities[(106-98)];
	char fieldvalid[(120-106)];
	char max_lba[(164-120)];
	char commandsets[(200-164)];
	char max_lba_ext[(256-200)];
}Identify_packet;

typedef struct Iorequest{
	Drive	*d;
	Fd		*fd;
	Uint8	read;
}Iorequest;

/*
 ** PRIVATE GLOBAL VARIABLES
 */

Uint16 block[255];
Ata_fd_data fd_dat_pool[ATA_MAX_FILES];
static Queue *free_dat_queue;
Iorequest iorequest_pool[ATA_MAX_REQUESTS];
static Queue *io_queue;

/*
 ** PUBLIC GLOBAL VARIABLES
 */


/*
 ** PRIVATE FUNCTION DECLARATIONS
 */


void flush_cache(Drive *d);
Status iorequest_dealloc(Iorequest *toFree);
Iorequest *iorequest_alloc(void);
Status fd_dat_dealloc(Ata_fd_data *toFree);
Ata_fd_data *fd_dat_alloc(void);
void delay(Uint32 control);
void selectDrive(Drive *d, Uint8 mode);
void disableIRQ(Bus *b);
void _ata_reset(Uint32 control);
void lba_set_sectors(Drive *d, Uint64 sector, Uint16 sectorcount);
char read_status(Drive *d);
Status _ata_read_finish(Fd* fd);

/*
 ** PRIVATE FUNCTIONS
 */

void flush_cache(Drive *d){
	__outb (d->base+COMMAND, FLUSH_CACHE);
}
Status iorequest_dealloc(Iorequest *toFree){
	Key key;
	if(toFree == NULL){
		return( BAD_PARAM ); 
	}

	key.i = 0;
	return( _q_insert( io_queue, (void *) toFree, key) );
}
Iorequest *iorequest_alloc(void){
	Iorequest *req;
	if( _q_remove( io_queue, (void **) &req ) != SUCCESS ) {
		req = NULL;
	}
	return req;
}

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

void delay(Uint32 control){
	Uint8 i;
	for (i = 0 ; i < 5; i++){
		//read the control register 5 times to give it time to settle
		__inb(control);
	}
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
void selectDrive(Drive *d, Uint8 mode){
	//select the master/slave drive as appropriate
	__outb(d->base+DRIVE_HEAD_PORT, mode | (~(d->master))<< 4);	
	delay(d->control);
}

void disableIRQ(Bus *b){
	//disable interrupts
	Uint8 i;
	for(i=0;i<4;i++){
		Drive *d = & b->drives[i];
		if(d->type != INVALID_DRIVE){
			selectDrive(d,LBA_MODE);
			__outb(d->control,0);
			__outb(d->control,NIEN);
		}
	}
}

void _ata_reset(Uint32 control){
	__outb(control, SRST );
	__outb(control, 0x00 );
	delay(control);
	//disable interrupts
}


void lba_set_sectors(Drive *d, Uint64 sector, Uint16 sectorcount){

	Uint8 *sector_b= (Uint8 *)&sector;
	Uint8 *sectorcount_b = (Uint8 *)&sectorcount;

	selectDrive(d,LBA_MODE);
	__outb (d->base+SECTOR_COUNT, sectorcount_b[1]);//sectorcount high byte
	__outb (d->base+LBA_LOW, sector_b[3]);
	__outb (d->base+LBA_MID, sector_b[4]);
	__outb (d->base+LBA_HIGH,sector_b[5]);
	__outb (d->base+SECTOR_COUNT, sectorcount_b[0]);//sectorcount low byte
	__outb (d->base+LBA_LOW, sector_b[0]);
	__outb (d->base+LBA_MID, sector_b[1]);
	__outb (d->base+LBA_HIGH, sector_b[2]);

}


char read_status(Drive *d){
	selectDrive(d,LBA_MODE);
	return __inb(d->base+REGULAR_STATUS);
}

Status _ata_read_finish(Fd* fd){
	char *data;
	int i;
	Ata_fd_data *dev_data;
	dev_data=(Ata_fd_data *)fd->device_data;
	data=(char *)block;

	for(i =0; i < 256; i++){
		block[i] = __inw(dev_data->d->base+DATA);
	}

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
	status = __inb(_busses[i].busmaster_base + 2);
	__outb(_busses[i].busmaster_base + 2,0x04);
	c_printf("b[%d].primary busmaster Status %x\n",i,status);
	status = __inb(_busses[i].busmaster_base + 8 + 2);
	__outb(_busses[i].busmaster_base + 8 + 2,0x04);
	c_printf("b[%d].secondary busmaster Status %x\n",i,status);

	for(j=0;j<ATA_DRIVES_PER_BUS;j++){
		d= &_busses[i].drives[j];
		selectDrive(d, LBA48);
		status = __inb(d->base+ REGULAR_STATUS);
		c_printf("b[%d].d[%d] Status %x\n",i,j,status);

		if(status & ERR){
			//crap.. 
			c_printf("isr: vector %x code %x\n", vector, code);
			c_printf("b[%d].d[%d] Status %x\n",i,j,status);

			__outb(d->control, 0);
			__outb(d->control, NIEN);

			//_kpanic("_ata_isr", "ATA drive in error state.",FAILURE);
		}

		if(status & DRQ){
			if (d->lastRequest != NULL){
				c_puts("got data\n");
				_ata_read_finish(d->lastRequest);
				d->lastRequest=NULL;
			} else{
				_kpanic("_ata_isr", "Got unsolicited data.",FAILURE);
			}
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
		fd->startWrite=&_ata_write_blocking;
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

	if(fd->write_index % SECTOR_SIZE){
		blocks_rw = read_raw_blocking(dev_data->d,
				dev_data->write_sector,
				1, block);
		if(blocks_rw != 1){
			c_puts("error reading back block on writeback");
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
			return FAILURE;
		}
	}
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
	dev_data->d->lastRequest=fd;

	//block until the drive is ready.. TODO: block on drive
	while (read_status(dev_data->d) & (BSY | DRQ)); //spin until not busy

	switch(dev_data->d->type){
		case LBA48:
			lba_set_sectors(dev_data->d, dev_data->read_sector, 1); //request only one sector
			__outb (dev_data->d->base+COMMAND, READ_SECTORS_EXT);
			break;
		case LBA28:
			c_puts("mode not supported yet!");
			break;
		case INVALID_DRIVE:
			c_puts("invalid!");
			break;
		default:
			c_puts("what?");
			break;
	}

	__outb(dev_data->d->control,0);// enable interrupts on this device
	return SUCCESS;
}

Status _ata_write_start(Fd* fd){
	Ata_fd_data *dev_data;
	int i;
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
	//block until the drive is ready.. TODO: block on drive
	while (read_status(dev_data->d) & (BSY | DRQ)); //spin until not busy

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


int write_raw_blocking(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf ){
	volatile unsigned char status = 0;
	Uint16 i,j;

	if(d->type== INVALID_DRIVE){
		return -1;
	}

	if (sector > d->sectors){
		//tried to read off the end of disk. 
		return -1;	
	}

	while (__inb(d->base+REGULAR_STATUS) & (BSY | DRQ)); //spin until not busy. TODO:add to blocked queue

	switch(d->type){
		case LBA48:
			lba_set_sectors(d, sector, sectorcount);
			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			__outb (d->base+COMMAND, WRITE_SECTORS_EXT);

			for(i=0;i<sectorcount;i++){
				//read status until error or DRQ
				do {
					status = __inb(d->base+REGULAR_STATUS);
				}while(status & BSY);

				if(status & (ERR | DF) || !(status & DRQ) ){
					//there was an error requesting
					c_printf("write error. status: %x\n",status);
					return -1;
				}

				for(j =0; j < 256; j++){
					__outw(d->base+DATA, buf[i*256+j]);
				}
			}
			break;

		case LBA28:
			//selectDrive(d,LBA_MODE);

			break;

		case CHS:
			break;

		default:
			//invalid drive
			return -1;

	}

	//disable interrupts
	return i;	

}
//read sector into buffer and return number of sectors read 
int read_raw_blocking(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf ){
	volatile unsigned char status = 0;
	Uint16 i,j;

	if(d->type== INVALID_DRIVE){
		c_printf("Invalid drive\n");
		return -1;
	}

	if (sector > d->sectors){
		c_printf("Invalid sector\n");
		//tried to read off the end of disk. 
		return -1;	
	}

	while (__inb(d->base+REGULAR_STATUS) & (BSY | DRQ)); //spin until not busy. TODO:add to blocked queue

	switch(d->type){
		case LBA48:
			lba_set_sectors(d, sector, sectorcount);
			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			__outb (d->base+COMMAND, READ_SECTORS_EXT);
			//for each sector
			for(i=0;i<sectorcount;i++){
				//read status until error or DRQ
				do {
					status = __inb(d->base+REGULAR_STATUS);
				}while(status & BSY);

				if(status & (ERR | DF) || !(status & DRQ) ){
					//there was an error requesting
					c_printf("read error. status: %x\n",status);
					return -1;
				}

				for(j =0; j < 256; j++){
					buf[i*256+j] = __inw(d->base+DATA);
				}
			}
			break;

		case LBA28:
			//selectDrive(d,LBA_MODE);

			break;

		case CHS:
			break;

		default:
			//invalid drive
			return -1;
	}
	//disable interrupts
	return i;
}


int _ata_identify(int master, Uint32 base, Uint32 control, Drive* d ){

	d->type=INVALID_DRIVE;
	d->base=base;
	d->control=control;
	d->master=master;
	d->lastRequest=NULL;

	if(base == 0x00 || control == 0x00){
		c_puts("invalid bus. Skipping");
		return -1;
	}

	delay(control);

	selectDrive(d,CHS); //chs mode for identification
	//detect a floating drive
	if( __inb( base+REGULAR_STATUS ) != 0xFF){ //High if there is no drive
		int i;

		volatile unsigned char status = 0;
		do{
			status =__inb(base+REGULAR_STATUS);
		}while (status & BSY);
		//load 0 into the follow addresses. If these change, we messed up.
		__outb(base+SECTOR_COUNT,0);
		__outb(base+LBA_LOW,0);
		__outb(base+LBA_MID,0);
		__outb(base+LBA_HIGH,0);

		//send the identify command
		__outb(base+COMMAND,IDENTIFY);
		delay(control);
		status =__inb(base+REGULAR_STATUS);
		if (status == 0){ //no device
			return -1;
		}

		if( status & ERR || status & DF){
			c_printf("identify error.. SATA drive? Status:%x\n", status);
			_ata_reset(control);
			return -1;
		}

		while( (status = __inb(base+REGULAR_STATUS)) & BSY){
			//read status until busy bit clears
		}

		if (__inb(base+LBA_MID) || __inb(base+LBA_HIGH)){
			//check if the drive is non ATA. If so, end
			Uint8 cl = __inb(base+CYLINDER_LOW);
			Uint8 ch = __inb(base+CYLINDER_HIGH);
			if (cl == 0x14 && ch == 0xEB){
				c_printf("ATAPI drive detected but not supported\n");
				__outb(control, 0);
				__outb(control, NIEN);
				flush_cache(d);
			}
			return -1;
		}
		do{
			status = __inb(base+REGULAR_STATUS); //read status until error or DRQ
		}while(!(status & (DRQ | ERR | DF)));

		if(status & ERR || status &DF){
			//there was an error requesting
			c_printf("error reading the identify block");
			return -1;
		}
		if(!(status & DRQ)){
			return -1;
		}
		//read in the identify block
		for(i =0; i < 256; i++){
			block[i] = __inw(base+DATA);
		}

		Identify_packet *ip=(Identify_packet *) block;

		for (i=0;i<40;i+=2){
			d->model[i]=ip->model[i+1];
			d->model[i+1]=ip->model[i];
			if(d->model[i] < ' ' || d->model[i] > '~')
				break;
		}
		c_printf("%s",d->model);

		//check sector count
		if (block[83] & (1<<10) ){
			d->type=LBA48;
			d->sectors=*(Uint64* ) &block[100];
			c_printf("48 bit mode sectors: %d \n",*(Uint64* ) &block[100] );
			//c_printf("sectors: %d %d %d %d\n",block[103], block[102],block[101],block[100] );
		}else{
			d->type=LBA28;
			d->sectors=*(Uint32* ) &block[60];
			c_printf("sectors: %d %d\n", block[61],block[60] );
		}

		//Ask the user if we should use this drive.
		c_puts("Should we use this drive?");
		int c='j';
		while((c = c_getchar()) != 'y' && c !='n');
		if (c=='n'){
			c_puts(" Okay. Marking as invalid.");
			d->type=INVALID_DRIVE;
		}else{
			_ata_primary=d;
		}
		c_puts("\n");
	}
	return 0;
}


void _ata_init(void){
	Status status;
	struct pci_func f;
	f.bus = 0;
	f.slot = 0;
	f.func = 0;

	int currentBus=0;
	int i,j;

	_ata_primary = &_busses[0].drives[0]; //set primary to something by default

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

	status = _q_alloc( &io_queue, NULL );
	if( status != SUCCESS ) {
		_kpanic( "_ata_init", "IO queue alloc status %s", status );
	}
	for(i=0;i<ATA_MAX_REQUESTS;i++){
		status = fd_dat_dealloc(&fd_dat_pool[i]);
		if( status != SUCCESS ) {
			_kpanic( "_ata_init", "IO queue insert status %s", status );
		}
	}

	for(i =0; i< ATA_MAX_BUSSES;i++){
		for (j=0;j<ATA_DRIVES_PER_BUS;j++){
			_busses[i].drives[j].type=INVALID_DRIVE;
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
				_ata_identify(0, BAR[0],BAR[1]+2,&_busses[currentBus].drives[0]);
				c_printf("Bus[%d].drive[%d]:",currentBus, 1);
				_ata_identify(1, BAR[0],BAR[1]+2,&_busses[currentBus].drives[1]);
				if(BAR[2] != 0x00){
					c_printf("Bus[%d].drive[%d]:",currentBus, 2);
					_ata_identify(0, BAR[2],BAR[3]+2,&_busses[currentBus].drives[2]);
					c_printf("Bus[%d].drive[%d]:",currentBus, 3);
					_ata_identify(1, BAR[2],BAR[3]+2,&_busses[currentBus].drives[3]);
					//disableIRQ(&_busses[currentBus]);
				}
			}
			currentBus++;
		}
	}

	_fd_flush_rx(&_fds[CIO_FD]);
	_fd_flush_tx(&_fds[CIO_FD]);

	c_puts( " ATA" );
}
