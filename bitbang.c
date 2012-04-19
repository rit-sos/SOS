/*
 * =====================================================================================
 *
 *       Filename:  bitbang.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/18/12 13:15:16
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Eamon Doran (ed), emd3753@rit.edu
 *        Company:  None
 *
 * =====================================================================================
 */

#include "bitbang.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _outl
 *  Description:  
 * =====================================================================================
 */
    void
_outl ( Uint32 value, int port )
    {
    asm volatile ("outl %0, %w1"
                    : /* no output registers */ 
                    : "a"(value), "Nd"(port));
    return;
    }     /* -----  end of function _outl  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _outw
 *  Description:  
 * =====================================================================================
 */
    void
_outw ( Uint16 value, int port )
    {
    asm volatile ("outw %0, %w1"
                    : /* no output registers */ 
                    : "a"(value), "Nd"(port));
    return;
    }     /* -----  end of function _outw  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _outb
 *  Description:  
 * =====================================================================================
 */
    void
_outb ( Uint8 value, int port )
    {
    asm volatile ("outb %0, %w1"
                    : /* no output registers */ 
                    : "a"(value), "Nd"(port));
    return;
    }     /* -----  end of function _outb  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _inl
 *  Description:  
 * =====================================================================================
 */
    Uint32
_inl ( int port )
    {
    Uint32 value;
    asm volatile ("inl %w1, %0" 
                    : "=a"(value) 
                    : "Nd"(port));
    return value;
    }     /* -----  end of function _inl  ----- */
/*  
 * ===  FUNCTION  ======================================================================
 *         Name:  _inw
 *  Description:  
 * =====================================================================================
 */
    Uint16
_inw ( int port )
    {
    Uint16 value;
    asm volatile ("inw %w1, %0"
                    : "=a"(value)
                    : "Nd"(port));
    return value;
    }     /* -----  end of function _inw  ----- */
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  _inb
 *  Description:  
 * =====================================================================================
 */
    Uint8
_inb ( int port )
    {
    Uint8 value;
    asm volatile ("inb %w1, %0" 
                    : "=a"(value) 
                    : "Nd"(port));
    return value;
    }     /* -----  end of function _inb  ----- */
