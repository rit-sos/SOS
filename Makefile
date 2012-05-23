###############################################################################
# Break-a-build ver. -1.0
#
# SOS build system
#
# Corey Bloodstein (cmb4247)
###############################################################################
#
# Hey, it's probably a good idea to read this before you do anything!
#
# SOS requires a boot image to start up. This boot image is composed of
# three parts:
#   1) The bootloader
#   2) The kernel
#   3) User programs
#
# The process for building each of these is slightly different, but the
# general idea is the same. All of the objects that the component depends
# on must first be built (USER_BITS, BOOT_BITS, KERNEL_BITS). Next, the
# programs themselves can be linked (kernel, boot, and any number of user
# programs). Finally, the independent programs can be assembled into the
# boot image.
#
# The boot image layout is described by variables in this makefile, as well
# as the map file. The relevant variables are USER_BASE, KERNEL_BASE, and
# BOOT_BASE. These variables tell the linker what the base address of the
# resulting executable is.
#
# In the case of user programs, the base variable is the address of the
# program -in its own virtual address space-. As such, it is not useful
# in creating the boot image. To compensate, the map file is used to
# describe the actual layout of the boot image. Each line of the map file
# consists of the name of a user program (such as init) followed by its
# desired address in the boot image. The address should be on a page
# boundary.
#
# The map file is used twice during the build process. It is used near the
# beginning of the build to generate umap.h, kmap.h, and kmap.c. These files
# are used by user-space programs and the kernel respectively to identify
# known user programs. The user-space version is a list of program image IDs,
# used in exec so that the kernel can find the program. The kernel-space
# version contains the IDs and mapping addresses, as well as an array to map
# program IDs to their mapping addresses.
#
# Another note on user programs: all user programs are linked with ustrap.S
# for two reasons. The first is to ensure that the entry point, _start, is
# always at the beginning of the user program when it's mapped in. _start
# in turn calls main, so as long as your user program has a function called
# main you should be good to go. For reference, the prototype is a rather
# non-standard "void main(void);".
#
# The other reason that user programs are linked with ustrap.S is to set up
# the "exit handler", in other words, returning to exit(). The asm pseudo--
# function "_start" sets the return address to the address of exit() in the
# user program's own local copy of ulib, then jumps to main.
# 
# Right now, user programs can only use one source file (<program-name>.c)
# and the user libraries. A future update to this makefile will add
# support for more involved user programs.
#
# tl;dr version:
#
# If you want to add a kernel module:
#   1. Add <module-name>.o to KERNEL_BITS
#   2. make depends (maybe)
#   3. make
#
# If you want to add a user program:
#   1. Make sure your program has a main() function
#   2. Add <program-name> to USERS
#   3. Add <program-name> to map
#   4. make depends (maybe)
#   5. make
#
# If you want to change the bootloader:
#   1. Don't.
#   2. Please don't.
#   (The Makefile won't really stop you from doing this...)
#   
############################################################################END

# Add your user programs here, and add a line to the map too.
# Don't remove init from this list, bad things will happen.
USERS = init user_a mman_test heap_test user_disk bad_user shm_test \
		window_test user_shell

MAPS = umap.h kmap.h kmap.c 

USER_BITS = ulibc.o ulibs.o u_windowing.o graphics_font.o umem.o
USER_SRC = $(patsubst %,%.c,$(USERS))
USER_BASE = 0x10000000

KERNEL_BITS = startup.o system.o klibc.o klibs.o pcbs.o queues.o scheduler.o \
	clock.o sio.o stacks.o syscalls.o kmap.o isr_stubs.o support.o c_io.o \
	mmanc.o mmans.o fd.o vbe.o graphics_font.o windowing.o ata.o pci.o heaps.o \
	shm.o ata_pio.o ioreq.o

KERNEL_BASE = 0x10000

BOOT_BITS = bootstrap.o 
BOOT_BASE = 0x0

INCLUDES = -I. -Iinclude
SOURCES = $(wildcard *.c) $(wildcard *.S)

USER_OPTIONS = -DCLEAR_BSS_SEGMENT -DSP2_CONFIG -DISR_DEBUGGING_CODE -DUSER_ENTRY="$(USER_BASE)" -ggdb

INCLUDES = -I. -I./include
CPP = cpp
CPPFLAGS = $(USER_OPTIONS) -nostdinc $(INCLUDES)
CC = gcc
CFLAGS = -m32 -fno-stack-protector -fno-builtin -Wall -Wstrict-prototypes -Wno-main $(CPPFLAGS)
AS = as --32
ASFLAGS = 
LD = ld -m elf_i386
LDFLAGS = --oformat binary -s

.DEFAULT_GOAL = all

.PHONY: build list all clean realclean depend help
$(USER_BITS): $(MAPS)

# top level target
all: build list

# Generic build rules for {.c,.s,.S} => .o
.SUFFIXES:  .S

.c.s:
	$(CC) $(CFLAGS) -S $*.c

.S.s:
	$(CPP) $(CPPFLAGS) -o $*.s $*.S

.S.o:
	$(CPP) $(CPPFLAGS) -o $*.s $*.S
	$(AS) -o $*.o $*.s -a=$*.lst; $(RM) -f $*.s

.c.o:
	$(CC) $(CFLAGS) -c $*.c

#
# make the usb.image and floppy.image from the map
#
build: boot kernel $(USERS) BuildImage
	./BuildImage -d usb -o usb.image -b boot kernel $(KERNEL_BASE) `cat map`
	./BuildImage -d floppy -o floppy.image -b boot kernel $(KERNEL_BASE) `cat map`

BuildImage: BuildImage.c
	$(CC) -o BuildImage BuildImage.c

#
# make the bootloader
#
boot: $(BOOT_BITS)
	$(LD) $(LDFLAGS) -Ttext $(BOOT_BASE) -o boot $(BOOT_BITS) -e begtext
bootstrap.o: bootstrap.h vbe_boot.S
#
# make the kernel
#
kernel: $(MAPS) $(KERNEL_BITS)
	$(LD) -Ttext $(KERNEL_BASE) -o kernel.o $(KERNEL_BITS)
	$(LD) $(LDFLAGS) -Ttext $(KERNEL_BASE) -o kernel -e _start kernel.o

# Make the user programs. More complicated than it sounds.
# To make the build a little less fragile, ustrap.S defines the real
# entry point of the program and is located at the beginning of the program
# text. Without this, main would have to be the first function linked into
# the user program.
$(USERS): ustrap.o $(USER_BITS) $(USER_SRC)
	$(CC) $(CFLAGS) -c -o $@.tmp.o $@.c
	$(LD) -Ttext $(USER_BASE) -o $@.o ustrap.o $@.tmp.o $(USER_BITS)
	-rm -f $@.tmp.o
	$(LD) $(LDFLAGS) -Ttext $(USER_BASE) -o $@ -e _start $@.o

# When adding or removing user programs, update the map file (called 'map').
# This file contains a list of program names and their offsets in the
# image file. mkmap.sh will generate umap.h, kmap.h, and kmap.c based on
# this list.
$(MAPS): map
	@echo Reading map...
	@bash mkmap.sh < map

#
# make something bootable
#
usb: build
	dd if=usb.image of=/local/devices/disk && sync

floppy: build
	dd if=floppy.image of=/dev/fd0 && sync

#
# namelists
#
list: boot kernel $(USERS)
	@for i in $(USERS) kernel bootstrap; do echo "$$i: generating namelist"; nm -Bn $$i.o | pr -w80 -3 > $$i.nl; done
#	@bash -c "if [[ \"0x`nm -Bn kernel.o | tail -n 1 | cut -f 1 -d ' '`\" -ge \"$(PGDIR_BASE)\" ]]; then echo 'KERNEL SIZE CHANGED, ADJUST PGTBL START'; false; else true; fi"

#
# etcetera
#
clean:
	-rm -f *.o $(MAPS) *.nl *.lst *.s

realclean: clean
	-rm -f boot kernel $(USERS) usb.image floppy.image 

help:
	@head -n `grep -m 1 -n "##END" Makefile | cut -f 1 -d ':'` Makefile \
		| sed -e 's/^#[ ]\?//' -e 's/^##.*$$//'

depend: realclean
	makedepend $(INCLUDES) $(SOURCES)

# DO NOT DELETE

0.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
0.o: clock.h stacks.h shm.h klib.h
ata.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
ata.o: clock.h stacks.h shm.h klib.h ./include/x86arch.h ./startup.h pci.h
ata.o: ata.h fd.h io.h queues.h c_io.h windowing.h graphics_font.h vbe.h
ata.o: ioreq.h ata_pio.h
ata_pio.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
ata_pio.o: clock.h stacks.h shm.h klib.h ata.h fd.h io.h queues.h ata_pio.h
ata_pio.o: c_io.h windowing.h graphics_font.h vbe.h ./startup.h
bad_user.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
bad_user.o: clock.h stacks.h shm.h klib.h
bitbang.o: bitbang.h headers.h defs.h types.h support.h system.h mman.h
bitbang.o: heaps.h pcbs.h clock.h stacks.h shm.h klib.h
BuildImage.o: /usr/include/stdio.h /usr/include/features.h
BuildImage.o: /usr/include/libio.h /usr/include/_G_config.h
BuildImage.o: /usr/include/wchar.h /usr/include/stdlib.h
BuildImage.o: /usr/include/alloca.h /usr/include/unistd.h
BuildImage.o: /usr/include/getopt.h /usr/include/string.h
BuildImage.o: /usr/include/xlocale.h
c_io.o: io.h fd.h headers.h defs.h types.h support.h system.h mman.h heaps.h
c_io.o: pcbs.h clock.h stacks.h shm.h klib.h queues.h c_io.h windowing.h
c_io.o: graphics_font.h vbe.h ./startup.h ./include/x86arch.h
clock.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
clock.o: clock.h stacks.h shm.h klib.h ./include/x86arch.h ./startup.h c_io.h
clock.o: fd.h io.h queues.h windowing.h graphics_font.h vbe.h scheduler.h
clock.o: sio.h syscalls.h
e100.o: pci.h headers.h defs.h types.h support.h system.h mman.h heaps.h
e100.o: pcbs.h clock.h stacks.h shm.h klib.h
fd.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
fd.o: clock.h stacks.h shm.h klib.h scheduler.h queues.h c_io.h fd.h io.h
fd.o: windowing.h graphics_font.h vbe.h
graphics_font.o: graphics_font.h headers.h defs.h types.h support.h system.h
graphics_font.o: mman.h heaps.h pcbs.h clock.h stacks.h shm.h klib.h
heaps.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
heaps.o: clock.h stacks.h shm.h klib.h c_io.h fd.h io.h queues.h windowing.h
heaps.o: graphics_font.h vbe.h
heap_test.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
heap_test.o: pcbs.h clock.h stacks.h shm.h klib.h
init.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
init.o: clock.h stacks.h shm.h klib.h
ioreq.o: fd.h io.h headers.h defs.h types.h support.h system.h mman.h heaps.h
ioreq.o: pcbs.h clock.h stacks.h shm.h klib.h queues.h ata.h ata_pio.h
ioreq.o: ioreq.h
klibc.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
klibc.o: clock.h stacks.h shm.h klib.h c_io.h fd.h io.h queues.h windowing.h
klibc.o: graphics_font.h vbe.h
mmanc.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
mmanc.o: clock.h stacks.h shm.h klib.h ./include/x86arch.h c_io.h fd.h io.h
mmanc.o: queues.h windowing.h graphics_font.h vbe.h bootstrap.h syscalls.h
mmanc.o: ./startup.h
mman_test.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
mman_test.o: pcbs.h clock.h stacks.h shm.h klib.h
offsets.o: heaps.h pcbs.h headers.h defs.h types.h support.h system.h mman.h
offsets.o: klib.h clock.h stacks.h shm.h /usr/include/stdio.h
offsets.o: /usr/include/features.h /usr/include/libio.h
offsets.o: /usr/include/_G_config.h /usr/include/wchar.h
pcbs.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
pcbs.o: clock.h stacks.h shm.h klib.h queues.h c_io.h fd.h io.h windowing.h
pcbs.o: graphics_font.h vbe.h
pci.o: pci.h headers.h defs.h types.h support.h system.h mman.h heaps.h
pci.o: pcbs.h clock.h stacks.h shm.h klib.h ./startup.h
queues.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
queues.o: clock.h stacks.h shm.h klib.h queues.h c_io.h fd.h io.h windowing.h
queues.o: graphics_font.h vbe.h
scheduler.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
scheduler.o: pcbs.h clock.h stacks.h shm.h klib.h c_io.h fd.h io.h queues.h
scheduler.o: windowing.h graphics_font.h vbe.h scheduler.h
shm.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
shm.o: clock.h stacks.h shm.h klib.h c_io.h fd.h io.h queues.h windowing.h
shm.o: graphics_font.h vbe.h
shm_test.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
shm_test.o: clock.h stacks.h shm.h klib.h
sio.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
sio.o: clock.h stacks.h shm.h klib.h fd.h io.h queues.h sio.h c_io.h
sio.o: windowing.h graphics_font.h vbe.h scheduler.h ./startup.h
sio.o: ./include/uart.h ./include/x86arch.h
stacks.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
stacks.o: clock.h stacks.h shm.h klib.h c_io.h fd.h io.h queues.h windowing.h
stacks.o: graphics_font.h vbe.h
support.o: ./startup.h support.h c_io.h fd.h io.h headers.h defs.h types.h
support.o: system.h mman.h heaps.h pcbs.h clock.h stacks.h shm.h klib.h
support.o: queues.h windowing.h graphics_font.h vbe.h ./include/x86arch.h
support.o: bootstrap.h syscalls.h
syscalls.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
syscalls.o: clock.h stacks.h shm.h klib.h scheduler.h queues.h sio.h
syscalls.o: syscalls.h ./include/x86arch.h c_io.h fd.h io.h windowing.h
syscalls.o: graphics_font.h vbe.h ata.h ./startup.h
system.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
system.o: clock.h stacks.h shm.h klib.h bootstrap.h syscalls.h queues.h
system.o: ./include/x86arch.h sio.h scheduler.h fd.h io.h vbe.h windowing.h
system.o: graphics_font.h c_io.h ata.h
ulibc.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
ulibc.o: clock.h stacks.h shm.h klib.h
umem.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
umem.o: clock.h stacks.h shm.h klib.h
user_a.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
user_a.o: clock.h stacks.h shm.h klib.h
user_disk.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
user_disk.o: pcbs.h clock.h stacks.h shm.h klib.h
users.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
users.o: clock.h stacks.h shm.h klib.h users.h
user_shell.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
user_shell.o: pcbs.h clock.h stacks.h shm.h klib.h ulib.h io.h u_windowing.h
user_shell.o: windowing.h graphics_font.h vbe.h user_shell.h
u_windowing.o: u_windowing.h windowing.h headers.h defs.h types.h support.h
u_windowing.o: system.h mman.h heaps.h pcbs.h clock.h stacks.h shm.h klib.h
u_windowing.o: graphics_font.h vbe.h
vbe.o: vbe.h headers.h defs.h types.h support.h system.h mman.h heaps.h
vbe.o: pcbs.h clock.h stacks.h shm.h klib.h windowing.h graphics_font.h
vbe.o: vbe_structs.h sio.h queues.h bootstrap.h
windowing.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
windowing.o: pcbs.h clock.h stacks.h shm.h klib.h windowing.h graphics_font.h
windowing.o: vbe.h
window_test.o: headers.h defs.h types.h support.h system.h mman.h heaps.h
window_test.o: pcbs.h clock.h stacks.h shm.h klib.h ulib.h io.h u_windowing.h
window_test.o: windowing.h graphics_font.h vbe.h
bootstrap.o: vbe_boot.h bootstrap.h vbe_boot.S
isr_stubs.o: bootstrap.h offsets.h
mmans.o: headers.h defs.h types.h support.h system.h mman.h heaps.h pcbs.h
mmans.o: clock.h stacks.h shm.h klib.h offsets.h
startup.o: bootstrap.h
ulibs.o: syscalls.h headers.h defs.h types.h support.h system.h mman.h
ulibs.o: heaps.h pcbs.h clock.h stacks.h shm.h klib.h queues.h
ulibs.o: ./include/x86arch.h
vbe_boot.o: vbe_boot.h bootstrap.h
