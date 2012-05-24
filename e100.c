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

#define __KERNEL__20113__

#include "pci.h"
#include "e100.h"
#include <x86arch.h>
#include "heaps.h"
#include "c_io.h"

const Uint16 device_id[] = { 0x1029, 0x1030, 0x1031, 0x1032, 0x1033, 0x1034, 0x1038, 0x1039, 0x103A, 0x103B, 0x103C, 0x103D, 0x103E, 0x1050, 0x1051, 0x1052, 0x1053, 0x1054, 0x1055, 0x1056, 0x1057, 0x1059, 0x1064, 0x1065, 0x1066, 0x1067, 0x1068, 0x1069, 0x106A, 0x106B, 0x1091, 0x1092, 0x1093, 0x1094, 0x1095, 0x10fe, 0x1209, 0x1229, 0x2449, 0x2459, 0x245D, 0x27DC };


/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
e100 *dev;

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_enable_irq
 *  Description:  
 * =====================================================================================
 */
    void
_e100_enable_irq (csr *csr)
    {
    csr->scb.cmd_hi = irq_mask_none;
    return;
    }   /* -----  end of function _e100_enable_irq  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_disable_irq
 *  Description:  
 * =====================================================================================
 */
    void
_e100_disable_irq (csr *csr)
    {
    csr->scb.cmd_hi = irq_mask_all;
    return;
    }   /* -----  end of function _e100_disable_irq  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_reset
 *  Description:  
 * =====================================================================================
 */
    void
_e100_reset (csr *csr)
    { c_printf("ENTER _e100_reset\n");
    csr->port = selective_reset;
    __delay (1);
    csr->port = software_reset;
    __delay (1);
    _e100_disable_irq (csr);
    c_printf("LEAVE _e100_reset\n");  
    return;
    }   /* -----  end of function _e100_reset  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_isr
 *  Description:  
 * =====================================================================================
 */
    void
_e100_isr ( int vector, int code )
    {
    c_printf("ENTER:_e100_isr\n");
    
    //Uint8 b = dev->csr->scb.stat_ack;
    //scb_stat_ack *stat_ack = (void*)&b;
    scb_stat_ack *stat_ack = dev->csr->scb.stat_ack;

    c_printf("write to stat_ack");
    dev->csr->scb.stat_ack = 0xff;   
    c_printf("wrote to stat_ack");

    if (stat_ack->cx)
        c_printf("\nCU EXECUTED\n");	
    if (stat_ack->fr)
        {
        c_printf("\nFRAME RECEIVED\n");

        if (!dev->ru.first)
            {
            dev->ru.first = dev->ru.last;
            }
        }
    if (stat_ack->cna)
        {
        c_printf("\nCNA\n");
        if (dev->cu.last)
            {
            if (dev->cu.last->status & cb_complete)
                {
                dev->cu.idle = 1;
                dev->cu.next = 0;
                }
            else
                {
                dev->cu.idle = 0;
                while (dev->cu.next->status & cb_complete)
                    {
                    if (!(dev->cu.next->status & cb_ok))
                        {

                        }
                    dev->cu.next = (void*)dev->cu.next->link;
                    }
                _e100_scb_command (dev->csr, cuc_resume, 0);
                }
            } 
        }
    if (stat_ack->rnr)
        {
        c_printf("e100ISR: RU overrun!\n");
        dev->ru.full = 1;
        }
    if (stat_ack->mdi)
        c_printf("MDI operation completed\n");
    if (stat_ack->swi)
        c_printf("Software Interrupt\n");
    if (stat_ack->fcp)
        c_printf("Flow Control Pause\n");

    if( vector >= 0x20 && vector < 0x30 )
        {
        __outb( PIC_MASTER_CMD_PORT, PIC_EOI );
        if( vector > 0x28 )
            {
            __outb( PIC_SLAVE_CMD_PORT, PIC_EOI );
            }
        }

    c_printf("\nLEAVE:_e100_isr\n");
    return;
    }		/* -----  end of function _e100_isr  ----- */


/*          
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_ring_init
 *  Description:  
 * =====================================================================================
 */
    void
_e100_ring_init (e100 *dev)
    {
    int i;

    /*-----------------------------------------------------------------------------
     *  
     *-----------------------------------------------------------------------------*/
    dev->cu.count = dev->cu.size / E100_CB_SIZE;
    dev->cu.next = 0;

    for (i=0; i < dev->cu.count ; i++)
        {
        cmd_blk *cb = (void*)(dev->cu.base + i * E100_CB_SIZE);

        cb->command = cb_nop | cb_el;
        cb->status = 0;


        if (i == dev->cu.count - 1)
            {
            cb->link = (void*)dev->cu.base;
            dev->cu.last = cb;
            }
        else
            {
            cb->link = (void*)((Uint32)cb + E100_CB_SIZE);
            }
        }

    /*-----------------------------------------------------------------------------
     *  
     *-----------------------------------------------------------------------------*/
    dev->ru.count = dev->ru.size / E100_RFD_SIZE;
    for (i=0; i < dev->ru.count; i++)
        {
        rfd *rfd  = (void*)(dev->ru.base + i * E100_RFD_SIZE);

        rfd->head.command = cb_nop;
        rfd->head.status = 0;
        rfd->size = E100_RFD_SIZE - sizeof(rfd);

        if (rfd->size & 1) rfd->size--;

        rfd->eof = 0;
        rfd->f = 0;

        if (i == dev->ru.count - 1)
            {
            rfd->head.link = (void*)dev->ru.base;
            rfd->head.command |= cb_el;
            dev->ru.prev = rfd;
            }
        else
            {
            rfd->head.link = (void*)((Uint32)rfd + E100_RFD_SIZE);
            }
        }
    dev->ru.first = 0;
    dev->ru.last = (void*)dev->ru.base;
    return;
    }   /* -----  end of function _e100_ring_init  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_ring_alloc
 *  Description:  
 * =====================================================================================
 */
    void*
_e100_ring_alloc (e100 *dev, Uint32 len)
    {
    if (len > E100_CB_SIZE) 
        return (void*) 0;

    if (dev->cu.next)
        {
        if ((void*)dev->cu.last->link == dev->cu.next) 
            return (void*) 0;

        return (void*)dev->cu.last->link;
        } 
    else 
        {
        return (void*)dev->cu.last->link;
        }
    }   /* -----  end of function _e100_ring_alloc  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_config
 *  Description:  
 * =====================================================================================
 */
    void*
_e100_config (e100 *dev)
    {
    cfg_cmd *cmd = _e100_ring_alloc(dev, sizeof(cfg_cmd));

    cmd->head.command = cb_config | cb_el | cb_i | cb_s;
    cmd->head.status = 0xE;

    //  Byte 0    
    cmd->config.byte_count = 0x16;
    cmd->config.pad0 = 0x0;
    //  Byte 1
    cmd->config.rx_fifo_limit = 0x0;
    cmd->config.rx_fifo_limit = 0x8;
    cmd->config.pad1 = 0x0;
    //  Byte 2
    cmd->config.adaptive_ifs = 0x0;
    //  Byte 3    
    cmd->config.mwi_enable = 0x0;
    cmd->config.type_enable = 0x0;//maybe
    cmd->config.read_align_enable = 0x0;
    cmd->config.term_write_cache_line = 0x0;
    cmd->config.pad3 = 0x0;
    //  Byte 4    
    cmd->config.rx_dma_max_count = 0x0;
    cmd->config.pad4 = 0x0;
    //  Byte 5
    cmd->config.tx_dma_max_count = 0x0;  
    cmd->config.dma_max_count_enable = 0x0;
    //  Byte 6
    cmd->config.late_scb_update = 0x0;
    cmd->config.direct_rx_dma = 0x1;
    cmd->config.tno_intr = 0x0;
    cmd->config.cna_intr = 0x0;
    cmd->config.standard_tcb = 0x1;
    cmd->config.standard_stat_counter = 0x1;
    cmd->config.rx_discard_overruns = 0x1;
    cmd->config.rx_save_bad_frames = 0x0;
    //  Byte 7
    cmd->config.rx_discard_short_frames  = 0x1; 
    cmd->config.tx_underrun_retry = 0x1;
    cmd->config.pad7 = 0x0; 
    cmd->config.rx_extended_rfd = 0x0; 
    cmd->config.tx_two_frames_in_fifo = 0x0;
    cmd->config.tx_dynamic_tbd = 0x0;
    //  Byte 8
    cmd->config.mii_mode = 0x1;
    cmd->config.pad8 = 0x0;
    cmd->config.csma_disabled = 0x0;
    //  Byte 9
    cmd->config.rx_tcpudp_checksum = 0x0;
    cmd->config.pad9 = 0x0;
    cmd->config.vlan_arp_tco = 0x0;
    cmd->config.link_status_wake = 0x0;
    cmd->config.arp_wake = 0x0;
    cmd->config.mcmatch_wake = 0x0;
    //  Byte 10    
    cmd->config.pad10 = 0x6;
    cmd->config.no_source_addr_insertion = 0x0;//0x1;
    cmd->config.preamble_length = 0x2;
    cmd->config.loopback = 0x0;
    //  Byte 11
    cmd->config.linear_priority = 0x0;
    cmd->config.pad11 = 0x0;
    //  Byte 12
    cmd->config.linear_priority_mode = 0x1;
    cmd->config.pad12 = 0x0;
    cmd->config.ifs = 0x6;
    //  Byte 13
    cmd->config.ip_addr_lo = 0x00;
    //  Byte 14
    cmd->config.ip_addr_hi = 0xf2;
    //  Byte 15
    cmd->config.promiscuous_mode = 0x0;
    cmd->config.broadcast_disabled = 0x1;
    cmd->config.wait_after_win = 0x0;
    cmd->config.pad15_1 = 0x1;
    cmd->config.ignore_ul_bit = 0x0;
    cmd->config.crc_16_bit = 0x0;
    cmd->config.pad15_2 = 0x1;
    cmd->config.crs_or_cdt = 0x0;

    return cmd;
    }   /* -----  end of function _e100_config  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_scb_commmand
 *  Description:  
 * =====================================================================================
 */
    void
_e100_scb_command (csr *csr, Uint32 cmd, void *cmd_addr)
    {
    c_printf("\nENTER:_e100_scb_command\n");

    csr->scb.gen_ptr = (Uint32)cmd_addr;
    _e100_enable_irq (csr);
    csr->scb.cmd_lo = cmd;
    // Wait for command to be accepted
    while (csr->scb.cmd_lo);
    
    c_printf("\nLEAVE:_e100_scb_command\n"); 
    return;
    }		/* -----  end of function _e100_scb_command  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_ru_start
 *  Description:  
 * =====================================================================================
 */
    void
_e100_ru_start (e100 *dev)
    {
    c_printf("\nENTER:_e100_ru_start\n");
    
    dev->ru.full = 0;
    _e100_scb_command (dev->csr, ruc_start,  (void*)dev->ru.base);
    
    c_printf("\nENTER:_e100_ru_start\n");
    return ;
    }		/* -----  end of function _e100_ru_start  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_send_dev
 *  Description:  
 * =====================================================================================
 */
    Int32
_e100_send_dev (e100 *dev, void* buffer, Uint32 len)
    {
    tx_cmd *start = _e100_ring_alloc(dev, sizeof(tx_cmd) + len);

    if (!start)
        {
        }

    if (!dev->cu.idle)
        dev->cu.last->command &= ~cb_s;

    start->head.command = cb_tx | cb_cid | cb_i | cb_s;
    //start->head.status = 
    start->tbd_addr = 0xffffffff;
    start->byte_count = len;
    start->eof = 1;
    start->trans_thres = 0xE0;
    start->tbd_num = 0;
    _kmemcpy ((void*)(start+1), buffer, len);

    return len;
    }		/* -----  end of function _e100_send_dev  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_send
 *  Description:  
 * =====================================================================================
 */
    Int32
_e100_send (void *buffer, Uint32 len)
    {
    if (dev == NULL) return -2;
    return _e100_send_dev (dev, buffer, len);
    }		/* -----  end of function _e100_send  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_receive_dev
 *  Description:  
 * =====================================================================================
 */
    Int32
_e100_receive_dev (e100 *dev, void* buffer, Uint32 len)
    {
    Int32 count;

    //while(dev.ru.first = 0)
    if (len < dev->ru.first->count)
        return -1;
    _kmemcpy (buffer, dev->ru.first + 1, dev->ru.first->count);

    count = dev->ru.first->count;

    dev->ru.first->eof = 0;
    dev->ru.first->f = 0;
    dev->ru.prev->head.command &= ~cb_el;
    dev->ru.first->head.command |= cb_el;
    dev->ru.prev = dev->ru.first;
    dev->ru.first = (void*)dev->ru.first->head.link;

    if (dev->ru.full)
        {
        dev->ru.full = 0;
        _e100_scb_command(dev->csr, ruc_start, dev->ru.prev);
        }
    if (dev->ru.first == dev->ru.last)
        dev->ru.first = 0;
    return count;
    }		/* -----  end of function _e100_receive_dev  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_receive
 *  Description:  
 * =====================================================================================
 */
    Int32
_e100_receive (void *buffer, Uint32 len)
    {
    if (dev == NULL) return -2; 
    return _e100_receive_dev (dev, buffer, len);
    }		/* -----  end of function _e100_receive  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_find_pci
 *  Description:  
 * =====================================================================================
 */
    void*
_e100_find_pci (void)
    {
    struct pci_func *f = _kmalloc(sizeof(struct pci_func));
    int num_devices =  sizeof (device_id) / sizeof (Uint16);
    int i = 0;

    f->bus = 0xff;
    f->slot = 0xff;
    f->func = 0xff;
    f->vendor_id = 0x8086;
    f->device_id = 0xffff;

    for (i = 0; i < num_devices; i++)
        {
        f->device_id = device_id[i];
        _scan_pci_for (f);

        if (f->bus != 0xff)
            return f;
        }

    return NULL;
    }		/* -----  end of function _e100_find_pci  ----- */

struct pci_func *f;


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_csr_size
 *  Description:  
 * =====================================================================================
 */
    Uint
_e100_csr_size (void)
    {
    return sizeof(struct csr) ;
    }		/* -----  end of function _e100_csr_size  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_csr_addr
 *  Description:  
 * =====================================================================================
 */
    void*
_e100_csr_addr ( )
    {
    struct pci_func f;
    int num_devices =  sizeof (device_id) / sizeof (Uint16);
    int i = 0;
    Uint32 addr;

    f.bus = 0xff;
    f.slot = 0xff;
    f.func = 0xff;
    f.vendor_id = 0x8086;
    f.device_id = 0xffff;

    for (i = 0; i < num_devices; i++)
        {
        f.device_id = device_id[i];
        _scan_pci_for (&f);

        if (f.bus != 0xff)
            {
            addr = PCI_E100_BAR_MM(read_pci_conf_long (f.bus, f.slot, f.func, PCI_E100_BAR_MM_REG));
            c_printf("\nfound e100 csr @ %08x\n",addr);
            return (void*)addr;
            }
        }

    c_printf("This is not going to be good"); 
    return NULL;
    }		/* -----  end of function _e100_csr_addr  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _e100_init_module
 *  Description:  
 * =====================================================================================
 */
    int
_e100_init_module (void)
    {   c_printf("_e100_init_module\n");
    struct pci_func *f = _e100_find_pci();
    c_printf("_e100_init_module: found pci device @ %02x,%02x,%d irq=%d\n", f->bus,f->slot,f->func,f->irq_line);
    void *csr_addr;

    dev = _kmalloc(sizeof(e100)); c_printf("_e100_init_module: After kmalloc\n");
    dev->fun = f;
    
    c_printf("_e100_init_module: found pci device @ %02x,%02x,%d irq=%d\n", dev->fun->bus,dev->fun->slot,dev->fun->func,dev->fun->irq_line); 
    
    write_pci_conf_word (f->bus, f->slot, f->func, PCI_COMMAND_REGISTER, 
        PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE);


    csr_addr = (void*)(Uint32)PCI_E100_BAR_MM(read_pci_conf_long (f->bus, f->slot, f->func, PCI_E100_BAR_MM_REG));

     c_printf("_e100_init_module: CSR  %x  with size %d\n",csr_addr,sizeof(csr));

     c_printf("_e100_init_module: CSR @ %x\n",csr_addr );
     dev->csr = csr_addr; 

    _e100_reset(dev->csr);

    dev->cu.size = E100_CU_RING_SIZE * PAGE;
    dev->ru.size = E100_RU_RING_SIZE * PAGE;
    dev->cu.base = (Uint32)_kmalloc (dev->cu.size);
    dev->ru.base = (Uint32)_kmalloc (dev->ru.size); 

    _e100_ring_init (dev);
    dev->cu.first = 1;
    dev->cu.idle = 1;    

    cfg_cmd *cmd = _e100_config(dev);

    __install_isr (f->irq_line + IRQ_OFFSET, _e100_isr);

    _e100_scb_command (dev->csr,cuc_start,cmd);

    _e100_ru_start(dev);
    return 0;
    }		/* -----  end of function _e100_init_module  ----- */


