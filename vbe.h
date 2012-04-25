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
 * Execute a far call to the copied bios for init at the specified location
 */
void _vbe_call_init( Uint offset );

/*
 * Excecute a far call into the copied bios for the specified function and args
 */
int _vbe_call_func( Uint offset, Uint16 ax, Uint16 bx, Uint16 cx, void* data, Uint data_size );
int __vbe_call_func_( Uint offset, Uint16 ax, Uint16 bx, Uint16 cx, Uint32 es );


/* gdt data types */
struct gdt_pointer
{
	Uint16	limit;
	Uint32	base;
} __attribute__((packed));

typedef struct gdt_pointer GDT_Pointer;

struct gdt_entry
{
	Uint16	limit_low;
	Uint16	base_low;
	Uint8	base_mid;
	Uint8	access;
	Uint8	granularity;
	Uint8	base_high;
} __attribute__((packed));
typedef struct gdt_entry GDT_Entry;

#define BASE_LOW(x)		((((int)(x)) >> 0 ) & 0xFFFF)
#define BASE_MID(x)		(((int)((x)) >> 16) & 0xFF)
#define BASE_HIGH(x)	(((int)((x)) >> 24) & 0xFF)

#define BASE(gdt_e, x)	(gdt_e)->base_low = BASE_LOW((x)); \
						(gdt_e)->base_mid = BASE_MID((x)); \
						(gdt_e)->base_high = BASE_HIGH((x));

#define LIMIT_LOW(x)		(BASE_LOW((x)))
#define LIMIT_HIGH(gran, x)	(((gran) & 0xF0) | ((((int)x) >> 16) & 0x0F))

#define LIMIT(gdt_e, x)	(gdt_e)->limit_low = LIMIT_LOW((x));\
						(gdt_e)->granularity = LIMIT_HIGH((gdt_e)->granularity, (x));

/*
 * Get a pointer to the GDT
 */
void _vbe_getGDT( GDT_Pointer *gdt );
#endif


#endif
