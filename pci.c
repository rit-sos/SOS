/*
 * =====================================================================================
 *
 *       Filename:  pci.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/14/12 23:07:45
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Eamon Doran,    emd3753@rit.edu
 *                  Andrew LeCain,
 *        Company:  RIT
 *
 * =====================================================================================
 */
#include    "pci.h"
#include    "startup.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  set_pci_conf_addr
 *  Description:  
 * =====================================================================================
 */
    void 
set_pci_conf_addr ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset )
    {
    __outl (PCI_CONF_ADDR_IOPORT,0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | 
        (offset & 0xfc));
    return;
    }   /* -----  end of function set_pci_conf_addr  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  read_pci_conf_long
 *  Description:  
 * =====================================================================================
 */
    Uint32
read_pci_conf_long ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset ) 
    {
    Uint32 v;
    set_pci_conf_addr (bus, slot, func, offset);
    v = __inl (PCI_CONF_DATA_IOPORT);
    return v;
    }   /* -----  end of function read_pci_conf_long  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  read_pci_conf_word
 *  Description:  
 * =====================================================================================
 */
    Uint16 
read_pci_conf_word ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset )
    {
    Uint16 v;
    set_pci_conf_addr (bus, slot, func, offset);
    v = (__inl (PCI_CONF_DATA_IOPORT)>> ((offset & 2)*8)) & 0xffff;
    return v;
    }   /* -----  end of function read_pci_conf_word  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  read_pci_conf_byte
 *  Description:  
 * =====================================================================================
 */
    Uint8
read_pci_conf_byte ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset )
    {
    Uint8 v;
    set_pci_conf_addr (bus, slot, func, offset);
    v = (__inl (PCI_CONF_DATA_IOPORT) >> ((offset & 3)*8)) & 0xff;
    return v;
    }   /* -----  end of function read_pci_conf_byte  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  write_pci_conf_long
 *  Description:  
 * =====================================================================================
 */
    void
write_pci_conf_long ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset, Uint32 v ) 
    {
    set_pci_conf_addr (bus, slot, func, offset);
    __outl (PCI_CONF_DATA_IOPORT,v);
    return;
    }   /* -----  end of function write_pci_conf_long  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  write_pci_conf_word
 *  Description:  
 * =====================================================================================
 */
    void 
write_pci_conf_word (Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset, Uint16 v )
    {
    set_pci_conf_addr (bus, slot, func, offset);
    __outw (PCI_CONF_DATA_IOPORT + (offset & 2),v);
    return;
    }   /* -----  end of function write_pci_conf_word  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  write_pci_conf_byte
 *  Description:  
 * =====================================================================================
 */
    void
write_pci_conf_byte ( Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset, Uint8 v )
    {
    set_pci_conf_addr (bus, slot, func, offset);
    __outb ( PCI_CONF_DATA_IOPORT + (offset & 3),v);
    return;
    }   /* -----  end of function write_pci_conf_byte  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _pci_scan_for
 *  Description:  
 * =====================================================================================
 */
    void
_pci_scan_for ( struct pci_func *f, Uint8 bus_start, Uint8 slot_start, Uint8 func_start )
    {
    Uint8   bus;
    Uint8   slot;
    Uint8   func;
    Uint16  vendor_id;
    Uint16  device_id;
    Uint8   dev_class;
    Uint8   subclass;

    for (bus = bus_start; bus < PCI_MAX_BUSSES; bus++)
        {
        for (slot = slot_start; slot < PCI_MAX_SLOTS; slot++) 
            {
            for (func = func_start; func < PCI_MAX_FUNCS; func++)
                {
                dev_class = read_pci_conf_byte (bus, slot, func, PCI_CLASS_CODE);
                if (dev_class == 0xff || (f->dev_class != 0xFF && 
                        f->dev_class != dev_class))
                    continue;

                vendor_id = read_pci_conf_word (bus, slot, func, PCI_VENDOR_ID);
                if(f->vendor_id != 0xFFFF && f->vendor_id != vendor_id)
                    continue;

                device_id = read_pci_conf_word (bus, slot, func, PCI_DEVICE_ID);
                if(f->device_id != 0xFFFF && f->device_id != device_id)
                    continue;

                subclass = read_pci_conf_byte (bus, slot, func, PCI_SUBCLASS);

                f->bus = bus;
                f->slot = slot;
                f->func = func;

                f->vendor_id = vendor_id;
                f->device_id = device_id;
                f->dev_class = dev_class;
                f->subclass = subclass;
                return;
                }
            }
        }
    /*    
    f->bus = bus;
    f->slot = slot;
    f->func = func;

    f->vendor_id = vendor_id;
    f->device_id = device_id;
    f->dev_class = dev_class;
    f->subclass = subclass;
    */
    return;
    }   /* -----  end of function _pci_scan_for  ----- */
