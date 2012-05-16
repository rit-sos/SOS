#ifdef __ASM__20113__

/* PCB offsets */
#define PCB_CONTEXT		(0)
#define PCB_HEAPINFO	(60)
#define PCB_STACK		(64)
#define PCB_VIRT_MAP	(68)
#define PCB_PGDIR		(72)
#define PCB_WAKEUP		(76)
#define PCB_PID			(80)
#define PCB_PPID		(82)
#define PCB_STATE		(84)
#define PCB_PRIORITY	(85)
#define PCB_QUANTUM		(86)
#define PCB_PROGRAM		(87)

/* context offsets */
#define CTX_EDI			(0)
#define CTX_ESI			(4)
#define CTX_EBP			(8)
#define CTX_DUMMY_ESP	(12)
#define CTX_EBX			(16)
#define CTX_EDX			(20)
#define CTX_ECX			(24)
#define CTX_EAX			(28)
#define CTX_VECTOR		(32)
#define CTX_CODE		(36)
#define CTX_EIP			(40)
#define CTX_CS			(44)
#define CTX_EFLAGS		(48)
#define CTX_ESP			(52)
#define CTX_SS			(56)

#endif
