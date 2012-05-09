
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
#include "sio.h"


/*
 * Parametric definitions
 */



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

Uint16 block[255];


typedef struct Iorequest{
	Drive	d;
	Uint64	LBA;
	Uint16	sectors;
}Iorequest;


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
		selectDrive(d,LBA_MODE);
		__outb(b->primary_control,0);
		__outb(b->secondary_control,0);
		__outb(b->primary_control,NIEN);
		__outb(b->secondary_control,NIEN);
	}	

}
void _ata_reset(Uint32 control){
	__outb(control, SRST | NIEN );
	__outb(control, 0x00 | NIEN);
	delay(control);
	//disable interrupts
	__outb(control,NIEN);
}

//read sector into buffer and return number of sectors read 
int _ata_read(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf ){
	volatile unsigned char status = 0;
	Uint16 i,j;
	Uint8 *sector_b= (Uint8 *)&sector;
	Uint8 *sectorcount_b = (Uint8 *)&sectorcount;

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
			selectDrive(d,LBA_MODE);

			__outb (d->base+SECTOR_COUNT, sectorcount_b[1]);//sectorcount high byte
			__outb (d->base+LBA_LOW, sector_b[3]);
			__outb (d->base+LBA_MID, sector_b[4]);
			__outb (d->base+LBA_HIGH,sector_b[5]);
			__outb (d->base+SECTOR_COUNT, sectorcount_b[0]);//sectorcount low byte
			__outb (d->base+LBA_LOW, sector_b[0]);
			__outb (d->base+LBA_MID, sector_b[1]);
			__outb (d->base+LBA_HIGH, sector_b[2]);

			//Send the "READ SECTORS EXT" command (0x24) to port base+COMMAND:
			__outb (d->base+COMMAND, READ_SECTORS_EXT);

			c_printf("Reading from %x sector %d count %d",d->base+COMMAND,sector_b[0],sectorcount_b[0]);

			for(i=0;i<sectorcount;i++){
				status = __inb(d->base+REGULAR_STATUS);

				//read status until error or DRQ
				while(status & BSY){
					status = __inb(d->base+REGULAR_STATUS);
				}

				if(status & ERR || status & DF){
					//there was an error requesting
					c_printf("read error. status: %x\n",status);
					return -1;
				}

				if(!(status & DRQ)){
					c_printf("read failed!\n");
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
			selectDrive(d,LBA_MODE);

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
			c_printf("Non ata!");
			Uint8 cl = __inb(base+CYLINDER_LOW);
			Uint8 ch = __inb(base+CYLINDER_HIGH);
			c_printf(" status %x cl: %x ch:%x\n",status, cl, ch);
			return -1;
		}
		while(1){
			status = __inb(base+REGULAR_STATUS); //read status until error or DRQ
			if (status & DRQ) break;
			if (status & ERR) break;
			if (status & DF) break;
		}
		if(status & ERR || status &DF){
			//there was an error requesting
			c_printf("error reading the identify block");
			return -1;
		}
		if(!(status & DRQ)){
			return -1;
		}

		for(i =0; i < 256; i++){
			block[i] = __inw(base+DATA);
		}
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

	struct pci_func f;
	f.bus = 0;
	f.slot = 0;
	f.func = 0;

	int currentBus=0;
	int i,j;
	for(i =0; i< ATA_MAX_BUSSES;i++){
		for (j=0;j<ATA_DRIVES_PER_BUS;j++){
			busses[i].drives[j].type=INVALID_DRIVE;
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

			busses[currentBus].interrupt_line = read_pci_conf_byte(f.bus,f.slot,f.func,PCI_INTERRUPT_LINE);


			Uint8 i;
			for(i =0;i<5;i++){
				c_printf("BAR%d %x ", i, BAR[i]);
			}
			c_printf("irq: 0x%x\n",busses[currentBus].interrupt_line);			

			//check if this is an ATA drive
			if (BAR[0] == 0x01 || BAR[0]==0x00){
				BAR[0]=0x1F0;
				//BAR[1]=0x3F4;
				//BAR[2]=0x170;
				//BAR[3]=0x374;
			}
			if (BAR[1] == 0x01 || BAR[1]==0x00){
				BAR[1]=0x3F4;
			}
			if (BAR[2] == 0x01 || BAR[2]==0x00){
				BAR[2]=0x170;
			}
			if (BAR[3] == 0x01 || BAR[3]==0x00){
				BAR[3]=0x374;
			}
			if (BAR[4] == 0x01 || BAR[4]==0x00){
				//no busmaster on this device
			}

			for(i =0;i<5;i++){
				c_printf("BAR%d %x ", i, BAR[i]);
			}

			c_printf("irq: 0x%x\n",busses[currentBus].interrupt_line);			

			busses[currentBus].primary_base=BAR[0];
			busses[currentBus].primary_control=BAR[1]+2;
			busses[currentBus].secondary_base=BAR[2];
			busses[currentBus].secondary_control=BAR[3]+2;

			if (BAR[0] != 0x1f0){
				_ata_identify(0, BAR[0],BAR[1]+2,&busses[currentBus].drives[0]);
				_ata_identify(1, BAR[0],BAR[1]+2,&busses[currentBus].drives[1]);
				_ata_identify(0, BAR[2],BAR[3]+2,&busses[currentBus].drives[2]);
				_ata_identify(1, BAR[2],BAR[3]+2,&busses[currentBus].drives[3]);
				disableIRQ(&busses[currentBus]);



				for(i=0;i<4;i++){
					if (busses[currentBus].drives[i].type!=INVALID_DRIVE){
						_ata_read(&busses[currentBus].drives[i],0,1,block);
						for (j=200;j<256;j++){
							c_printf("%x ",block[j]);
						}
						_ata_read(&busses[currentBus].drives[i],0,1,block);
						for (j=200;j<256;j++){
							c_printf("%x ",block[j]);
						}

					}	
				}
			}	

			currentBus++;
		}


	}
	c_puts( " ATA" );
}

