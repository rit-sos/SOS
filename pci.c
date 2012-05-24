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
 *         Author:  Eamon Doran (ed), emd3753@rit.edu
 *        Company:  None
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
	__outl (PCI_CONF_ADDR_IOPORT,0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | (offset & 0xfc));
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
read_pci_conf_word (Uint8 bus, Uint8 slot, Uint8 func, Uint8 offset)
{
	Uint16 v;
	set_pci_conf_addr (bus, slot, func, offset);
	v = (__inl (PCI_CONF_DATA_IOPORT)>> ((offset & 2)*8)) & 0xffff;
	return v;
}  /* -----  end of function read_pci_conf_word  ----- */
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
}		/* -----  end of function read_pci_conf_byte  ----- */
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
}  /* -----  end of function write_pci_conf_word  ----- */
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
}		/* -----  end of function write_pci_conf_byte  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  dump_pci_device
 *  Description:  
 * =====================================================================================
 */
	void 
dump_pci_device(Uint8 bus, Uint8 slot, Uint8 func)
{
	int i;
	int j;
	Uint32 val;

	c_printf ("pci 0000:%02x:%02x.%d config space:", bus, slot, func);

	for (i = 0; i < 256; i += 4) 
	{
		if (!(i & 0x0f))
			c_printf ("\n  %02x:",i);

		val = read_pci_conf_long(bus, slot, func, i);

		for (j = 0; j < 4; j++) 
		{
			c_printf (" %02x", val & 0xff);
			val >>= 8;
		}
	}
	c_puts ("\n");
}       /* -----  end of function dump_pci_device  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  dump_pci_devices
 *  Description:  
 * =====================================================================================
 */
	void 
dump_pci_devices (void)
{
	Uint16 bus; 
	Uint8 slot;
	Uint8 func;

	for (bus = 0; bus < 256; bus++)
	{
		for (slot = 0; slot < 32; slot++) 
		{
			for (func = 0; func < 8; func++)
			{
				Uint32 class;
				Uint8 type;

				class = read_pci_conf_long(bus, slot, func, PCI_CLASS_REVISION);

				if (class == 0xffffffff)
					continue; 

				dump_pci_device(bus, slot, func);

				// BUSY WAIT
				int i = 0; 
				for (i=0; i < 4294967295; i++);                

				if (func == 0)
				{
					type = read_pci_conf_byte(bus, slot,func, PCI_HEADER_TYPE);
					if (!(type & 0x80))
						break;
				}
			}
		}
	}
	return;
}       /* -----  end of function dump_pci_devices  ----- */

void _pci_scan_for ( struct pci_func *f, Uint8 bus_start, Uint8 slot_start, Uint8 func_start )
{

	Uint8   bus;
	Uint8   slot;
	Uint8   func;

	Uint16  vendor_id;
	Uint16  device_id;
	Uint8  dev_class;
	Uint8  subclass;
	Uint16  revision;

	for (bus = bus_start; bus < PCI_MAX_BUSSES; bus++)
	{
		for (slot = slot_start; slot < PCI_MAX_SLOTS; slot++) 
		{
			for (func = func_start; func < PCI_MAX_FUNCS; func++)
			{
				
				dev_class = read_pci_conf_byte (bus, slot, func, PCI_CLASS_CODE);
				if (dev_class == 0xff)
					continue;
				subclass = read_pci_conf_byte (bus, slot, func, PCI_SUBCLASS);
				vendor_id = read_pci_conf_word (bus, slot, func, PCI_VENDOR_ID);
				device_id = read_pci_conf_word (bus, slot, func, PCI_DEVICE_ID);



				if(f->vendor_id != 0xFFFF){
					if (f->vendor_id != vendor_id){
						continue;
					}
				}

				if(f->device_id != 0xFFFF){
					if (f->device_id != device_id){
						continue;
					}
				}

				if(f->dev_class != 0xFF){
					if (f->dev_class != dev_class){
						continue;
					}
				}


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
	f->bus = bus;
	f->slot = slot;
	f->func = func;

	f->vendor_id = vendor_id;
	f->device_id = device_id;
	f->dev_class = dev_class;
	f->subclass = subclass;
	return;
}

    void
_scan_pci_for ( struct pci_func *f )
    {
    Uint16 bus;
    Uint8 slot;
    Uint8 func;

    for (bus = 0; bus < 256; bus++)
        {
        for (slot = 0; slot < 32; slot++)
            {
            for (func = 0; func < 8; func++)
                {
                Uint32 class;

                class = read_pci_conf_long(bus, slot, func, PCI_CLASS_REVISION);

                if (class == 0xffffffff)
                    continue;

                if (f->vendor_id == read_pci_conf_word (bus, slot, func, PCI_VENDOR_ID) &&
                  f->device_id == read_pci_conf_word (bus, slot, func, PCI_DEVICE_ID))
                    {
                    f->bus = bus;
                    f->slot = slot;
                    f->func = func;
                    f->irq_line = read_pci_conf_byte (bus, slot, func, PCI_INTERRUPT_LINE);
                    }
                }
            }
        }
    return;
    }

void play (void)
{
	Uint16 bus = 0;
	Uint8 slot = 3;
	Uint8 func = 0;

	for (bus = 0; bus < 256; bus++)
	{
		for (slot = 0; slot < 32; slot++)
		{
			for (func = 0; func < 8; func++)
			{
				Uint32 class = read_pci_conf_long(bus, slot, func, PCI_CLASS_REVISION);
				Uint16 vendor;
				Uint16 device;

				if (class == 0xffffffff)
					continue;

				vendor = read_pci_conf_word (bus, slot, func, PCI_VENDOR_ID);
				device = read_pci_conf_word (bus, slot, func, PCI_DEVICE_ID);

				if (1)
				{
					Uint32 mmio;
					//struct csr my_csr;
					/*
					   c_printf ("pci 0000:%02x:%02x.%d VENDOR_ID=%02x DEVICE_ID=%02x\n", bus, slot, func,vendor,device);
					   mmio = read_pci_conf_long (bus, slot, func, PCI_E100_MMIO_CSR_ADDR); 
					   c_printf ("CSR Memory Mapped Base Address=%02x\n",mmio);
					   write_pci_conf_long(bus, slot, func, PCI_COMMAND_STATUS_REG, 
					   PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE);
					   c_printf ("pci 0000:%02x:%02x.%d VENDOR_ID=%02x DEVICE_ID=%02x\n", bus, slot, func,vendor,device);
					   mmio = read_pci_conf_long (bus, slot, func, PCI_E100_MMIO_CSR_ADDR);
					   c_printf ("CSR Memory Mapped Base Address=%02x\n",mmio);
					   */
				}
			}
		}
	}

	//dump_pci_device(bus, slot, func);

	//v1 = read_pci_config_long (bus, slot, func, PCI_E100_CONFIG_CSR_MMIO);
	//c_printf ("CSR MMIO: %x\n", v1);
	//v2 = read_pci_config_long (bus, slot, func, PCI_E100_CONFIG_CSR_IO);
	//c_printf ("CSR IO: %x\n", v2);



	while(1);

	return;
}
