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

#define X_RES	1280
#define Y_RES	1024

__vbe_init:
	pushw	%bx
	pushw	%di
	pushw	%si
	pushw	%es
	pushw	%gs

	movw	$vbe_start_str,	%si
	call	dispMsg

	/* preset signature to "VBE2" */
	movw	$_vbe_info, 	%bx
	movb	$'V',	%al
	movb	%al,	(%bx)
	movb	$'B',	%al
	movb	%al,	1(%bx)
	movb	$'E',	%al
	movb	%al,	2(%bx)
	movb	$'2',	%al
	movb	%al,	3(%bx)

	/* get controller info */
	/* es:di points to _vbe_info */
	/* ax = 0x4F00 */
	movw	$_vbe_info,	%di
	movw	$BOOT_SEGMENT,	%ax
	movw	%ax,		%es
	movw	$0x4F00,	%ax
	/* call BIOS */
	int		$0x10
	cmpw	$0x004F,	%ax
	jne		_error_info

	movw	$vbe_version_str,	%si
	call	dispMsg

	movw	%es:VBE_INFO_VERSION(%di),	%cx
	call	printNum


	/* set to the first mode that is 1280x1024x32 */
	movw	%es:VBE_INFO_VIDEO_MODE_PTR(%di),	%si
	movw	%es:VBE_INFO_VIDEO_MODE_PTR+2(%di),	%gs

_mode_loop:
	movw	%si,	%bx

	/* read next mode number */
	movw	%gs:(%bx), %cx

	/* display the number */
	pushw	%si
	movw	$vbe_mode_str,	%si
	call	dispMsg
	call	printNum
	popw	%si

	/* check that we aren't at the end of the list */
	cmpw	$0xFFFF, %cx
	je		_not_found

	/* get the mode info for this mode */
	movw	$_vbe_mode_info,	%di
	movw	$BOOT_SEGMENT,	%ax
	movw	%ax,		%es
	movw	$0x4F01,	%ax
	/* call BIOS */
	int		$0x10
	cmpw	$0x004F,	%ax
	jne		_call_error_mode_get
	jmp		_check_res

_call_error_mode_get:
	call	_error_mode_get
	jmp		_end_loop

_check_res:

	pushw	%cx

	/* check screen width */
	movw	%es:VBE_MODE_INFO_X_RES(%di), %cx
	cmpw	$X_RES,	%cx
	jne		_end_loop

	/* check screen height */
	movw	%es:VBE_MODE_INFO_Y_RES(%di), %cx
	cmpw	$Y_RES,	%cx
	jne		_end_loop

	/* check bpp */
	xorw	%cx,	%cx
	movb	%es:VBE_MODE_INFO_BPP(%di), %cl
	cmpw	$32,	%cx
	jne		_end_loop

	/* check mode attributes */
	/* should be supported, graphics, color, and linear */
	movw	%es:VBE_MODE_INFO_MODE_ATTRIB(%di), %cx
	andw	$0x009B,	%cx
	cmpw	$0x009B,	%cx
	jnz		_end_loop

	/* found a mode thats 1024x1280x32 */
	/* set it, using linear buffer and clearing memory */
	popw	%cx
	orw		$0xC000,	%cx
	movw	$0x4f02,	%ax
	/* call BIOS */
	int		$0x10

	cmpw	$0x004F,	%ax
	jne		_error_mode_set

	/* clear screen */

	jmp		_success

_end_loop:
	addw	$2, %si
	jmp		_mode_loop

_not_found:
	movw	$vbe_none_str,	%si
	call	dispMsg
	jmp		_done

_error_info:
	
	movw	%ax,	%cx
	movw	$vbe_err_info_str,	%si
	call	dispMsg
	call	printNum
	jmp		_done

_error_mode_set:
	
	pushw	%cx
	movw	$vbe_err_str,	%si
	call	dispMsg
	movw	%ax,	%cx
	call	printNum

	popw	%cx
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
	popw	%gs
	popw	%es
	popw	%si
	popw	%di
	popw	%bx
	ret

_error_mode_get:
	
	pushw	%si
	pushw	%ax
	pushw 	%cx

	movw	$vbe_err_get_mode_str,	%si
	call	dispMsg

	call	printNum

	movw	$vbe_eol_str,	%si
	call	dispMsg

	popw	%cx
	popw	%ax
	popw	%si
	ret

/* print the value of %cx */
printNum:
	pushw	%cx
	pushw	%si
	pushw	%di

	movw	%cx,	%si
	movw	$'x',	%di
	call	pnum

	movw	$vbe_eol_str,	%si
	call	dispMsg

	popw	%di
	popw	%si
	popw	%cx

	ret

/*
** Debugging routine.  This lives in the 1st block of the bootstrap
** so it can be called from there as well as from the 2nd block.
**
**	movw	$'x',%di	// a single character to print
**	movw	value,%si	// a 16-bit value to print in hex
**	call	pnum
*/
pnum:
	pushw	%ax
	pushw	%bx
	movw	%di,%ax
	movb	$0x0e,%ah
	movw	$0x0007,%bx
	int		$0x10

	call	pdigit
	call	pdigit
	call	pdigit
	call	pdigit

	popw	%bx
	popw	%ax
	ret

pdigit:	movw	%si,%ax
	shl	$4,%si
	shr	$12,%ax
	cmpw	$9,%ax
	jle	pdd
	addw	$'A'-10,%ax
	jmp	prt
pdd:	addw	$'0',%ax
prt:	movb	$0xe,%ah
	movw	$7,%bx
	int	$0x10
	ret

vbe_start_str:
	.asciz	"\n\rInit VBE\n\r"
vbe_err_str:
	.asciz	"Error "
vbe_err_info_str:
	.asciz	"Error info\n\r"
vbe_err_get_mode_str:
	.asciz	"Error getting mode "
vbe_err_set_mode_str:
	.asciz	"Error setting mode "
vbe_eol_str:
	.asciz	"\n\r"
vbe_version_str:
	.asciz	"VBE Version: "
vbe_mode_str:
	.asciz	"mode "
vbe_none_str:
	.asciz	"mode not found\n\r"
