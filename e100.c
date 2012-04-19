/*
 * =====================================================================================
 *
 *       Filename:  e100.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/16/12 22:41:00
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Eamon Doran (ed), emd3753@rit.edu
 *        Company:  None
 *
 * =====================================================================================
 */

#include "pci.h"

#define PCI_VENDOR_ID_INTEL     0x8086
#define PCI_DEVICE_ID_E100_QEMU 0x1209
#define PCI_DEVICE_ID_E100_REAL 0x1229

struct pci_func f;

    int
_e100_init_module (void)
    {
    f.bus = -1;
    f.vendor_id = PCI_VENDOR_ID_INTEL;
    f.device_id = PCI_DEVICE_ID_E100_QEMU;
        
    scan_pci_for(&f);

    if (f.bus == -1)
        {
        f.device_id = PCI_DEVICE_ID_E100_REAL;
        scan_pci_for(&f);
        }

    if (f.bus == -1)
        {
        return -1;
        }

    dump_pci_device(f.bus,f.slot,f.func);

    write_pci_conf_word (f.bus, f.slot, f.func, PCI_COMMAND_REGISTER, 
      PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE);



    while(1);
    return 0;
    }

void    _e100_cleanup_module    (void);




