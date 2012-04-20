/*
** File:	vbe.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE protected mode interface 
*/
#ifndef VBE_H
#define VBE_H

#include "headers.h"

#ifndef __ASM__20113__
/* 
 * _vbe_init()
 * 
 * Try to find the protected mode interface for vbe
 * if found initialize it and set up a video mode
 */
void _vbe_init(void);

/*
 * Set the vbe mode to closest mode to the one specified
 */
void _vbe_modeset(Uint w, Uint h);

/*
 * Execute a far call to the copied bios at the specified offset
 */
void _vbe_call( Uint offset );

#endif


#endif
