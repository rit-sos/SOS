
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

#include "pci.h"
#include "ata.h"
#include <startup.h>
#include "fd.h"
#include "c_io.h"


/*
 * Private definitions
 */


#define ATA_MAX_FILES 16

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
	Uint64 read_index;
	Uint64 write_index;
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
	Drive	d;
	Uint64	LBA;
	Uint16	sectors;
}Iorequest;

/*
** PRIVATE GLOBAL VARIABLES
*/

Uint16 block[255];
Ata_fd_data fd_dat_pool[ATA_MAX_FILES];
static Queue *free_dat_queue;

/*
** PUBLIC GLOBAL VARIABLES
*/


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
	__outb(d->base+DRIVE_HEAD_PORT, mode | ~(d->master)<< 4);	
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
	__outb(control, SRST | NIEN );
	__outb(control, 0x00 | NIEN);
	delay(control);
	//disable interrupts
	__outb(control,NIEN);
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



/*
 ** PUBLIC FUNCTIONS
 */

Fd* _ata_fopen(Drive *d, Uint64 sector, Uint16 len, Rwflags flags){
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
	dev_data->read_index=sector;
	dev_data->write_index=sector;
	fd->device_data=dev_data;
	
	if (flags & FD_R){
		fd->startRead=&_ata_read_blocking;
	}else{
		fd->startRead=NULL;
	}
	if(flags & FD_W){
		fd->startWrite=&_ata_write_blocking;
	}else{
		fd->startWrite=NULL;
	}

	
	fd->rxwatermark = 1;
	fd->txwatermark = 512;
	
	return fd;
}

void _ata_read_blocking(Fd* fd){
	Ata_fd_data *dev_data;
	int blocks_read,i;
	char *data;
	dev_data=(Ata_fd_data *)fd->device_data;
	
	if(dev_data->read_index > dev_data->sector_end){
		return;
	}
	
	blocks_read = read_raw_blocking(	dev_data->d,
					dev_data->read_index,
					1,
					block);
	data=(char *)block;
	
	if (blocks_read < 0) blocks_read=0;
	dev_data->read_index+=blocks_read;
	for(i =0;i<blocks_read*512;i++){
		_fd_readBack(fd,data[i]);
	}
	_fd_readDone(fd);
}

void _ata_write_blocking(Fd* fd){
	
	Ata_fd_data *dev_data;
	int blocks_wrote,i;
	char *data;
	dev_data=(Ata_fd_data *)fd->device_data;
	
	if(dev_data->read_index > dev_data->sector_end){
		return;
	}
	
	data=(char *)block;
	
	for(i =0;i<512;i++){
		data[i]= _fd_getTx(fd);
	}
	
	blocks_wrote = write_raw_blocking(dev_data->d,
					dev_data->write_index,
					1,
					block);
	
	if (blocks_wrote < 0) blocks_wrote=0;
	dev_data->write_index+=blocks_wrote;

	_fd_readDone(fd);

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
				c_printf(".");
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
	__outb(d->control,NIEN);
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
				c_printf(".");
			}
			c_printf("done!\n");
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
	__outb(d->control,NIEN);
	return i;
}


int _ata_identify(int master, Uint32 base, Uint32 control, Drive* d ){

	delay(control);

	d->type=INVALID_DRIVE;
	d->base=base;
	d->control=control;
	d->master=master;

	selectDrive(d,CHS); //chs mode for identification
	__outb(control,NIEN); //disable interrupts
	//detect a floating drive
	if( __inb( base+REGULAR_STATUS ) != 0xFF){ //High if there is no drive
		int i;


		volatile unsigned char status = 0;
		status =__inb(base+REGULAR_STATUS);

		//load 0 into the follow addresses. If these change, we messed up.
		__outb(base+SECTOR_COUNT,0);	
		__outb(base+LBA_LOW,0);	
		__outb(base+LBA_MID,0);	
		__outb(base+LBA_HIGH,0);	

		//send the identify command	
		__outb(base+COMMAND,IDENTIFY);
		delay(control);
		status =__inb(base+REGULAR_STATUS);
		if (status == 0){
			return -1;
		}

		if( status & ERR || status & DF){
			c_printf("identify error.. SATA drive?\n");
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
			if (cl == 0x14 && ch == 0xEB)
				c_printf("ATAPI drive detected but not supported\n");
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
	}
	__outb(control,NIEN); //disable interrupts
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
				BAR[3]=0x170;
				BAR[4]=0x374;

			}
			/*
			for(i =0;i<5;i++){
				c_printf("BAR%d %x ", i, BAR[i]);
			}
			c_printf("irq: 0x%x\n",_busses[currentBus].interrupt_line);*/

			_busses[currentBus].primary_base=BAR[0];
			_busses[currentBus].primary_control=BAR[1]+2;
			_busses[currentBus].secondary_base=BAR[2];
			_busses[currentBus].secondary_control=BAR[3]+2;

			//TODO: figure out why regular PATA drives aren't working
			if (1 || BAR[0] != 0x1f0){
				_ata_identify(0, BAR[0],BAR[1]+2,&_busses[currentBus].drives[0]);
				_ata_identify(1, BAR[0],BAR[1]+2,&_busses[currentBus].drives[1]);
				_ata_identify(0, BAR[2],BAR[3]+2,&_busses[currentBus].drives[2]);
				_ata_identify(1, BAR[2],BAR[3]+2,&_busses[currentBus].drives[3]);
				disableIRQ(&_busses[currentBus]);

			}
			currentBus++;
		}
	}

	c_puts( " ATA" );
}
