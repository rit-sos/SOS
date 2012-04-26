
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

#define MASTER		0xA0
#define SLAVE		0xB0

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
**Commands
*/

#define IDENTIFY	0xEC


Uint16 block[255];
Drive a,b,c,d;

void delay(Uint32 control){
	Uint8 i;

	for (i = 0 ; i < 4; i++){
		//read the control register 5 times to give it time to settle
		__inb(control);

	}
}

void _ata_reset(Uint32 control){
	__outb(control, 0x02);
	__outb(control, 0x00);
	delay(control);
}


int _ata_identify(int slave, Uint32 base, Uint32 control, Drive* d ){

	delay(control);
	
	//detect a floating drive
	if( __inb( base+REGULAR_STATUS ) != 0xFF){ //High if there is no drive
		int i;

		//select the master/slave drive as appropriate
		__outb(base+DRIVE_HEAD_PORT, MASTER | slave<< 4);	

		delay(control);

		Uint8 cl = __inb(base+CYLINDER_LOW);
		Uint8 ch = __inb(base+CYLINDER_HIGH);

		volatile unsigned char status = 0;
		status =__inb(base+REGULAR_STATUS);

		__outb(base+SECTOR_COUNT,0);	
		__outb(base+LBA_LOW,0);	
		__outb(base+LBA_MID,0);	
		__outb(base+LBA_HIGH,0);	

		//send the identify command	
		__outb(base+COMMAND,IDENTIFY);


		delay(control);

		status =__inb(base+REGULAR_STATUS);

		if (status == 0){
			c_puts("drive doesn't exist\n");
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
			c_puts("Non ata!");
			Uint8 cl = __inb(base+CYLINDER_LOW);
			Uint8 ch = __inb(base+CYLINDER_HIGH);
			c_printf(" status %d cl: %d ch:%d\n",status, cl, ch);
		}
		while( (status = __inb(base+REGULAR_STATUS))){
			//read status until error or DRQ
			if (status & DRQ) break;
			if (status & ERR) break;
		}
		if(status & ERR){
			//there was an error requesting
			c_puts("read error");
			return -1;
		}
		c_puts("IDENTIFY\n");

		for(i =0; i < 256; i++){
			block[i] = __inw(base+DATA);
		}

		if (block[83] & (1<<10) ){
			c_puts("48 bit mode supported\n");
			c_printf("sectors: %d ",*(Uint64* ) &block[100] );
			//c_printf("sectors: %d %d %d %d\n",block[103], block[102],block[101],block[100] );
		}else{
			c_printf("sectors: %d %d\n", block[61],block[60] );
		}

	}else{
		c_puts("Floating bus!\n");
	}


	return 0;

}


void _ata_init(void){

	struct pci_func f;


	f.bus = 0;
	f.slot = 0;
	f.func = 0;


	while(f.bus != 3){
		//reset the scan

		f.vendor_id = 0xFFFF;
		f.device_id = 0xFFFF;
		f.dev_class = PCI_DEVICE_CLASS_MASS_STORAGE;
		scan_pci_for(&f,f.bus,f.slot,f.func+1);

		if(f.bus!=3){

			c_printf("\ncontroller found on bus:%d slot:%d function:%d\n", f.bus,f.slot,f.func);

			Uint32 BAR[6];

			BAR[0] = read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR0) &0xFFFE;
			if (BAR[0] == 0x01 || BAR[0]==0x00){
				BAR[0]=0x1F0;
			}
			BAR[1] = read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR1)&0xFFFE;
			if (BAR[1] == 0x01 || BAR[1]==0x00){
				BAR[1]=0x3F4;
			}
			BAR[2]= read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR2)&0xFFFE;
			if (BAR[2] == 0x01 || BAR[2]==0x00){
				BAR[2]=0x170;
			}
			BAR[3]= read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR3)&0xFFFE;
			if (BAR[3] == 0x01 || BAR[3]==0x00){
				BAR[3]=0x374;

			}
			BAR[4]= read_pci_conf_long (f.bus, f.slot, f.func, PCI_BAR4)&0xFFFE;
			if (BAR[4] == 0x01 || BAR[4]==0x00){
				//no busmaster on this device
			}

			Uint8 i;
			for(i =0;i<5;i++){
				c_printf("BAR%d %x ", i, BAR[i]);
			}

			_ata_identify(0, BAR[0],BAR[1],&a); //hda
			_ata_identify(1, BAR[0],BAR[1],&b); //hdb
			_ata_identify(0, BAR[2],BAR[3],&c); //hdc
			_ata_identify(1, BAR[2],BAR[3],&d); //hdd
		}


	}
	c_puts( " ATA" );
	while(1){};
}
