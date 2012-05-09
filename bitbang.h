/*
 * =====================================================================================
 *
 *       Filename:  bitbang.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/18/12 13:24:10
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Eamon Doran (ed), emd3753@rit.edu
 *        Company:  None
 *
 * =====================================================================================
 */

#include "headers.h"

void _outl ( Uint32 value, int port );
void _outw ( Uint16 value, int port );
void _outb ( Uint8 value, int port );

Uint32 _inl ( int port );
Uint16 _inw ( int port );
Uint8 _inb ( int port );
