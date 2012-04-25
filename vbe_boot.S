/*
** File:	vbe_boot.S
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE bootstrap code
*/
#define __ASM__20113__
#define __KERNEL__20113__

#include "vbe_boot.h"
#include "bootstrap.h"

/* THIS CODE IS #included INTO BOOTSTRAP */

__vbe_init:
	pushw	%bx
	pushw	%di
	pushw	%si
	pushw	%es
	pushw	%gs

	movw	$vbe_start_str,	%si
	call	dispMsg

	/* preset signature to VBE2 */
	movw	$_vbe_info, 	%bx
	movb	$0x56,	%al
	movb	%al,	0(%bx)
	movb	$0x42,	%al
	movb	%al,	1(%bx)
	movb	$0x45,	%al
	movb	%al,	2(%bx)
	movb	$0x32,	%al
	movb	%al,	3(%bx)

	/* get controller info */
	/* es:di points to _vbe_info */
	/* ax = 0x4F00 */
	movw	$_vbe_info,	%di
	movw	$0x0000,	%ax
	movw	%ax,		%es
	movw	$0x4F00,	%ax
	/* call BIOS */
	int	$0x10
	cmpw	$0x004F,	%ax
	jne		_error_info

	movw	$vbe_info_str,	%si
	call	dispMsg

	/* set to the first mode that is 1280x1024x32 */
	movw	VBE_INFO_VIDEO_MODE_PTR(%bx),	%si
	movw	VBE_INFO_VIDEO_MODE_PTR+2(%bx),	%gs
_mode_loop:
	pushw	%si
	movw	$vbe_mode_str,	%si
	call	dispMsg
	popw	%si

	movw	%si,	%bx
	/* read next mode number */
	movw	%gs:(%bx), %cx

	/* check that we aren't at the end of the list */
	cmpw	$0xFFFF, %cx
	je		_not_found

	/* get the mode info for this mode */
	movw	_vbe_mode_info,	%di
	movw	$0x0000,	%ax
	movw	%ax,		%es
	movw	$0x4F01,	%ax
	/* call BIOS */
	int	$0x10
	cmpw	$0x004F,	%ax
	jne		_call_error_mode_get
	jmp		_check_res

_call_error_mode_get:
	call	_error_mode_get
	jmp		_end_loop

_check_res:

	movw	$_vbe_mode_info,	%bx
	/* check screen width */
	movw	VBE_MODE_INFO_X_RES(%bx), %dx
	cmpw	$1024,	%dx
	jne		_end_loop

	/* check screen height */
	movw	VBE_MODE_INFO_Y_RES(%bx), %dx
	cmpw	$1280,	%dx
	jne		_end_loop

	/* check bpp */
	movw	VBE_MODE_INFO_BPP(%bx), %dx
	cmpw	$32,	%dx
	jne		_end_loop

	/* check mode attributes */
	/* should be supported, graphics, color, and linear */
	movw	VBE_MODE_INFO_MODE_ATTRIB(%bx), %dx
	andw	$0x009B,	%dx
	cmpw	$0x009B,	%dx
	jne		_end_loop

	/* found a mode thats 1024x1280x32 */
	/* set it, using linear buffer and clearing memory */
	orw		$0xC000,	%cx
	movw	$0x4f02,	%ax
	/* call BIOS */
	int	$0x10
	cmpw	$0x004F,	%ax
	jne		_error_mode_set

	jmp		_success

_end_loop:
	addw	$2, %si
	jmp		_mode_loop

_not_found:
	movw	$vbe_none_str,	%si
	call	dispMsg
	jmp		_done

_error_info:
	
	movw	$vbe_err_str,	%si
	call	dispMsg
	movw	$vbe_info_str,	%si
	call	dispMsg
	jmp		_done

_error_mode_set:
	
	movw	$vbe_err_set_mode_str,	%si
	call	dispMsg

	call	printNum

	movw	$vbe_eol_str,	%si
	call	dispMsg
	jmp		_done

_success:
	movw	$msg_go,	%si
	call	dispMsg
	
_done:
	jmp		.
	popw	%gs
	popw	%es
	popw	%si
	popw	%di
	popw	%bx
	ret

.globl _error_mode_get
_error_mode_get:
	
	pushw	%ax
	pushw 	%cx

	movw	$vbe_err_get_mode_str,	%si
	call	dispMsg

	call	printNum

	movw	$vbe_eol_str,	%si
	call	dispMsg

	/* delay for a while */
	
	movw	$0x8000,	%ax
	movw	$0xFFFF,	%cx
_error_delay_loop1:
	sub		$1,		%ax

_error_delay_loop2:
	sub		$1,		%cx
	jnz		_error_delay_loop2

	cmpw	$0,	%ax
	jne		_error_delay_loop1

	popw	%cx
	popw	%ax
	ret

/* print the value of %cx */
printNum:
	pushw	%di
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx

	/* shift amount */
	movw	%cx,	%di
	movw	$12,	%cx	
_print_loop:
	cmpw	$0,	%cx
	jl		_print_done

	/* print next nibble */
	movw	%di,	%ax
	shlw	%cl,	%ax
	andw	$0xF,	%ax

	cmpw	$10,	%ax
	jge		_print_letter

	/* disp '0' + ax */
	addw	$48,	%ax

	jmp	_print_end_loop

_print_letter:
	/* disp 'A' + (ax-10) */
	addw	$55,	%ax
	
_print_end_loop:
	/* print %ax */
	movb	$0x0E,	%ah
	movw	$0x07,	%bx

	int	$0x10

	subw	$4, 	%cx
	jmp	_print_loop

_print_done:

	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	popw	%di

	ret


vbe_start_str:
	.asciz	"Init VBE\n\r"
vbe_err_str:
	.asciz	"Error "
vbe_err_get_mode_str:
	.asciz	"Error getting mode "
vbe_err_set_mode_str:
	.asciz	"Error getting mode "
vbe_eol_str:
	.asciz	"\n\r"
vbe_info_str:
	.asciz	"info\n\r"
vbe_mode_str:
	.asciz	"mode\n\r"
vbe_none_str:
	.asciz	"mode not found\n\r"
