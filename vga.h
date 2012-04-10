/*
** File:	vga.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	Implementation of VGA driver
*/

#ifndef _VGA_H
#define _VGA_H

#include "headers.h"

#define VGA_WIDTH	320
#define VGA_HEIGHT	240

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _vga_mode_switch
 *  Description: Initiate a mode switch for VGA
 * =====================================================================================
 */
Status _vga_mode_switch(int width, int height, int chain4);

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _video_init
 *  Description: Initialize the VGA chip, put in graphics mode 
 * =====================================================================================
 */
void _vga_init ( void );

#endif
