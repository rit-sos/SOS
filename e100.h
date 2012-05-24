/*
 * =====================================================================================
 *
 *       Filename:  e100.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/16/12 22:30:29
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Eamon Doran (ed), emd3753@rit.edu
 *        Company:  None
 *
 * =====================================================================================
 */

#include "headers.h"
#include "fd.h"

#define PCI_VENDOR_ID_INTEL     0x8086
#define PCI_E100_BAR_MM_MSK     0xffffffff0
#define PCI_E100_BAR_MM(v)      ((v) & PCI_E100_BAR_MM_MSK)
#define PCI_E100_BAR_MM_REG     0x10




#define PAGE                    4096
#define E100_CU_RING_SIZE       32
#define E100_RU_RING_SIZE       32
#define E100_CB_SIZE            2048
#define E100_RFD_SIZE           2048


/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct csr {
    struct {
        volatile Uint8 status;
        volatile Uint8 stat_ack;
        Uint8 cmd_lo;
        Uint8 cmd_hi;
        Uint32 gen_ptr;
    } scb;
    Uint32 port;
    Uint16 flash_ctrl;
    Uint8 eeprom_ctrl_lo;
    Uint8 eeprom_ctrl_hi;
    Uint32 mdi_ctrl;
    Uint32 rx_dma_count;
} csr;

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
struct scb_status {
    Uint8 zero : 2;
    Uint8 rus  : 4;
    Uint8 cus  : 2;
};

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct scb_stat_ack {
    Uint8 fcp  : 1;
    Uint8 rsv  : 1;
    Uint8 swi  : 1;
    Uint8 mdi  : 1;
    Uint8 rnr  : 1;
    Uint8 cna  : 1;
    Uint8 fr   : 1;
    Uint8 cx   : 1;
} scb_stat_ack;

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
enum scb_cmd_lo {
    cuc_nop        = 0x00,
    ruc_start      = 0x01,
    ruc_load_base  = 0x06,
    cuc_start      = 0x10,
    cuc_resume     = 0x20,
    cuc_dump_addr  = 0x40, 
    cuc_dump_stats = 0x50,
    cuc_load_base  = 0x60, 
    cuc_dump_reset = 0x70,
};

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
enum scb_cmd_hi {
    irq_mask_none = 0x00,
    irq_mask_all  = 0x01,
    irq_sw_gen    = 0x02,
};

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
enum port {
    software_reset  = 0x0000,
    selftest        = 0x0001,
    selective_reset = 0x0002,
};

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct cmd_blk {
    volatile Uint16 status;
    Uint16 command;
    struct cmd_blk *link;
} cmd_blk;


/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------*/
enum cb_status {
	cb_complete = 0x8000,
	cb_ok       = 0x2000,
};

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
struct rf_status {
    Uint8 tco :      1;   
    Uint8 ia :       1;   
    Uint8 nomatch :  1;  
    Uint8 res :      1;   
    Uint8 rcv_err :  1;
    Uint8 type :     1;
    Uint8 res2 :     1;
    Uint8 tooshort : 1;
    Uint8 dma_err :  1;
    Uint8 no_buf :   1;
    Uint8 align_err :1;
    Uint8 crc_err :  1;
    Uint8 res3 :     1;
} rf_status;

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
enum cb_command {
    cb_nop    = 0x0000,
    cb_iaaddr = 0x0001,
    cb_config = 0x0002,
    cb_multi  = 0x0003,
    cb_tx     = 0x0006,
    cb_ucode  = 0x0005,
    cb_dump   = 0x0006,
    cb_tx_sf  = 0x0008,
    cb_cid    = 0x1f00,
    cb_i      = 0x2000,
    cb_s      = 0x4000,
    cb_el     = 0x8000,
};      

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct cfg_cmd {
    cmd_blk head;
    struct {
        /*0*/   Uint8 byte_count:6, pad0:2;
        /*1*/   Uint8 rx_fifo_limit:4, tx_fifo_limit:3, pad1:1;
        /*2*/   Uint8 adaptive_ifs;
        /*3*/   Uint8 mwi_enable:1, type_enable:1, read_align_enable:1, 
            term_write_cache_line:1, pad3:4;
        /*4*/   Uint8 rx_dma_max_count:7, pad4:1;
        /*5*/   Uint8 tx_dma_max_count:7, dma_max_count_enable:1;
        /*6*/   Uint8 late_scb_update:1, direct_rx_dma:1,tno_intr:1, cna_intr:1, 
            standard_tcb:1, standard_stat_counter:1,rx_discard_overruns:1, 
            rx_save_bad_frames:1;
        /*7*/   Uint8 rx_discard_short_frames:1, tx_underrun_retry:2,pad7:2, 
            rx_extended_rfd:1, tx_two_frames_in_fifo:1,tx_dynamic_tbd:1;
        /*8*/   Uint8 mii_mode:1, pad8:6, csma_disabled:1;
        /*9*/   Uint8 rx_tcpudp_checksum:1, pad9:3, vlan_arp_tco:1,link_status_wake:1, 
            arp_wake:1, mcmatch_wake:1;
        /*10*/  Uint8 pad10:3, no_source_addr_insertion:1, preamble_length:2,loopback:2;
        /*11*/  Uint8 linear_priority:3, pad11:5;
        /*12*/  Uint8 linear_priority_mode:1, pad12:3, ifs:4;
        /*13*/  Uint8 ip_addr_lo;
        /*14*/  Uint8 ip_addr_hi;
        /*15*/  Uint8 promiscuous_mode:1, broadcast_disabled:1,wait_after_win:1, 
            pad15_1:1, ignore_ul_bit:1, crc_16_bit:1,pad15_2:1, crs_or_cdt:1;
        /*16*/  Uint8 fc_delay_lo;
        /*17*/  Uint8 fc_delay_hi;
        /*18*/  Uint8 rx_stripping:1, tx_padding:1, rx_crc_transfer:1,rx_long_ok:1, 
            fc_priority_threshold:3, pad18:1;
        /*19*/  Uint8 addr_wake:1, magic_packet_disable:1,fc_disable:1, fc_restop:1, 
            fc_restart:1, fc_reject:1,full_duplex_force:1, full_duplex_pin:1;
        /*20*/  Uint8 pad20_1:5, fc_priority_location:1, multi_ia:1, pad20_2:1;
        /*21*/  Uint8 pad21_1:3, multicast_all:1, pad21_2:4;
        /*22*/  Uint8 rx_d102_mode:1, rx_vlan_drop:1, pad22:6;
        Uint8 pad_d102[9];
    } config;
} cfg_cmd;

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct tx_cmd {
    cmd_blk head;
    Uint32 tbd_addr;
    struct {
        Uint32 byte_count : 14;
        Uint8 res :          1;
        Uint8 eof :          1;
        Uint8 trans_thres;
        Uint8 tbd_num;
    };
} tx_cmd;

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct rfd {
    cmd_blk head;
    Uint32 pad;
    struct {
        Uint16 count :  14;
        Uint8 f :        1;
        Uint8 eof :      1;
        Uint16 size :   14;
        Uint8 res2 :     2;
    };  
} rfd;

enum eeprom_ctrl_lo {
    eesk = 0x01,
    eecs = 0x02,
    eedi = 0x04,
    eedo = 0x08,
};

enum eeprom_op {
    op_write = 0x05,
    op_read  = 0x06,
    op_ewds  = 0x10,
    op_ewen  = 0x13,
};

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
typedef struct e100 {
    Uint8 macaddr[6];
    struct pci_func *fun;
    csr *csr;
    struct {
        Uint32 first;
        Uint32 idle;
        Uint32 size;
        Uint32 base;
        Uint32 count;
        cmd_blk *next;
        cmd_blk *last;
    } cu;
    struct {
        Uint32 full;
        Uint32 size;
        Uint32 base;
        Uint32 count;
        rfd *first;
        rfd *last;
        rfd *prev;
    } ru;
} e100;

/*-----------------------------------------------------------------------------
 *  
 *-----------------------------------------------------------------------------*/
void    _e100_enable_irq (csr *csr);
void    _e100_disable_irq ( csr *csr);
void    _e100_ring_init (e100 *dev);
void*   _e100_ring_alloc (e100 *dev, Uint32 len);
void    _e100_scb_command (csr *csr, Uint32 cmd, void *cmd_addr);

int     _e100_init_module (void);
void    _e100_cleanup_module (void);

Status _e100_startWrite(Fd *fd);
