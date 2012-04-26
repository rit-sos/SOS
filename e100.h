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

#define PCI_VENDOR_ID_INTEL           0x8086

/*
struct csr {
    struct {
        u8 status;
        u8 stat_ack;
        u8 cmd_lo;
        u8 cmd_hi;
        u32 gen_ptr;
    } scb;
    u32 port;
    u16 flash_ctrl;
    u8 eeprom_ctrl_lo;
    u8 eeprom_ctrl_hi;
    u32 mdi_ctrl;
    u32 rx_dma_count;
};
*/

int     _e100_init_module       (void);

void    _e100_cleanup_module    (void);


