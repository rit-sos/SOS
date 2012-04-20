/*
** File:	bios_probe.h
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	Probe the bios to find PMIB for VBE
*/
#ifndef BIOS_PROBE_H
#define BIOS_PROBE_H

#include "headers.h"

#define BIOS_START	0xC0000
#define BIOS_END	0xC8000
#define BIOS_SIZE	(BIOS_END-BIOS_START)

/* protected mode bios */
extern char *PMBios;

/* 
 * return the location of the VBE protected mode
 * interface in the bios if found, or NULL
 */
void* bios_probe(void);

#endif
