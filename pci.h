/*
 * =====================================================================================
 *
 *       Filename:  pci.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/14/12 23:04:20
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Eamon Doran (ed), emd3753@rit.edu
 *        Company:  None
 *
 * =====================================================================================
 */

#include "headers.h"




#define PCI_MAX_BUSSES	3
#define PCI_MAX_SLOTS	32	
#define PCI_MAX_FUNCS	8



#define PCI_CONF_ADDR_IOPORT        0x0cf8
#define PCI_CONF_DATA_IOPORT        0x0cfc

#define PCI_VENDOR_ID               0x00
#define PCI_DEVICE_ID               0x02
#define PCI_COMMAND_REGISTER        0x04
#define PCI_STATUS_REGISTER         0x06
#define PCI_CLASS_REVISION          0x08
#define PCI_SUBCLASS		    0x0A
#define PCI_CLASS_CODE		    0x0B
#define PCI_HEADER_TYPE             0x0E


#define PCI_BAR0			0x10
#define PCI_BAR1			0x14
#define PCI_BAR2			0x18
#define PCI_BAR3			0x1C
#define PCI_BAR4			0x20
#define PCI_BAR5			0x24

#define PCI_INTERRUPT_LINE		0x3C
#define PCI_INTERRUPT_PIN		0x3D


#define PCI_COMMAND_STATUS_REG          0x04

#define PCI_COMMAND_SHIFT               0
#define PCI_COMMAND_MASK                0xffff
#define PCI_STATUS_SHIFT                16
#define PCI_STATUS_MASK                 0xffff

#define PCI_COMMAND_IO_ENABLE           0x00000003
#define PCI_COMMAND_MEM_ENABLE          0x00000002
#define PCI_COMMAND_MASTER_ENABLE       0x00000004
#define PCI_COMMAND_SPECIAL_ENABLE      0x00000008
#define PCI_COMMAND_INVALIDATE_ENABLE   0x00000010
#define PCI_COMMAND_PALETTE_ENABLE      0x00000020
#define PCI_COMMAND_PARITY_ENABLE       0x00000040
#define PCI_COMMAND_STEPPING_ENABLE     0x00000080
#define PCI_COMMAND_SERR_ENABLE         0x00000100
#define PCI_COMMAND_BACKTOBACK_ENABLE   0x00000200

#define PCI_STATUS_CAPLIST_SUPPORT      0x00100000
#define PCI_STATUS_66MHZ_SUPPORT        0x00200000
#define PCI_STATUS_UDF_SUPPORT          0x00400000
#define PCI_STATUS_BACKTOBACK_SUPPORT   0x00800000
#define PCI_STATUS_PARITY_ERROR         0x01000000
#define PCI_STATUS_DEVSEL_FAST          0x00000000
#define PCI_STATUS_DEVSEL_MEDIUM        0x02000000
#define PCI_STATUS_DEVSEL_SLOW          0x04000000
#define PCI_STATUS_DEVSEL_MASK          0x06000000
#define PCI_STATUS_TARGET_TARGET_ABORT  0x08000000
#define PCI_STATUS_MASTER_TARGET_ABORT  0x10000000
#define PCI_STATUS_MASTER_ABORT         0x20000000
#define PCI_STATUS_SPECIAL_ERROR        0x40000000
#define PCI_STATUS_PARITY_DETECT        0x80000000

struct pci_bus;

struct pci_func
    {
    Uint8   bus;
    Uint8   slot;
    Uint8   func;

    Uint16  vendor_id;
    Uint16  device_id;
    Uint8  dev_class;
    Uint8  subclass;
    Uint16  revision;

    Uint32  reg_base[6];
    Uint32  reg_size[6];
    Uint8   irq_line;
    };

void set_pci_conf_addr ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset);


Uint32 read_pci_conf_long ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset );
Uint16  read_pci_conf_word ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset ); 
Uint8   read_pci_conf_byte ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset );

void dump_pci_device( Uint8 bus, Uint8 slot, Uint8 func );
void dump_pci_devices ( void );

void _pci_scan_for( struct pci_func *f, Uint8 bus_start, Uint8 slot_start, Uint8 func_start );

void play (void);




