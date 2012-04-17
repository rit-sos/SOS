###############################################################################
# Break-a-build ver. -1.0
#
# SOS build system
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
# tl;dr version:
#
# If you want to add a kernel module:
#   1. Add <module-name>.o to KERNEL_BITS
#   2. make depends (maybe)
#   3. make all
#
# If you want to add a user program:
#	0. Make sure your program has a main() function
#   1. Add <program-name> to USERS
#   2. Add <program-name> to map
#   3. make depends (maybe)
#   4. make all
#
# If you want to change the bootloader:
#   1. Don't.
#   
############################################################################END

USER_OPTIONS = -DCLEAR_BSS_SEGMENT -DSP2_CONFIG -DISR_DEBUGGING_CODE
INCLUDES = -I. -I./include
CPP = cpp
CPPFLAGS = $(USER_OPTIONS) -nostdinc $(INCLUDES)
CC = gcc
CFLAGS = -m32 -fno-stack-protector -fno-builtin -Wall -Wstrict-prototypes $(CPPFLAGS)
AS = as --32
ASFLAGS = 
LD = ld -m elf_i386
LDFLAGS = --oformat binary -s

MAPS = umap.h kmap.h kmap.c

# don't remove init from this list, bad things will happen
USERS = init user_a
USER_BITS = ulibc.o ulibs.o
USER_OBJ = $(patsubst %,%.o,$(USERS))
USER_BASE = 0x50000

KERNEL_BITS = startup.o system.o klibc.o klibs.o pcbs.o queues.o scheduler.o \
	clock.o sio.o stacks.o syscalls.o kmap.o isr_stubs.o support.o c_io.o
KERNEL_BASE = 0x10000

BOOT_BITS = bootstrap.o
BOOT_BASE = 0x0

.DEFAULT_GOAL = all

.PHONY: build list all clean realclean
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
	$(AS) -o $*.o $*.s -a=$*.lst
	$(RM) -f $*.s

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
$(USERS): ustrap.o $(USER_BITS) $(USER_OBJ)
	$(CC) $(CFLAGS) -c -o $@.tmp.o $@.c
ifeq ($@,user_a)
	$(LD) -Ttext 0x51000 -o $@.o ustrap.o $@.tmp.o $(USER_BITS)
else
	$(LD) -Ttext $(USER_BASE) -o $@.o ustrap.o $@.tmp.o $(USER_BITS)
endif
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
	dd if=usb.image of=/local/devices/disk

floppy: build
	dd if=floppy.image of=/dev/fd0

#
# namelists
#
list: boot kernel $(USERS)
	nm -Bn bootstrap.o | pr -w80 -3 > boot.nl
	nm -Bn kernel.o | pr -w80 -3 > kernel.nl
	nm -Bn init.o | pr -w80 -3 > init.nl
	nm -Bn user_a.o | pr -w80 -3 > user_a.nl
#
# etcetera
#
clean:
	-rm -f *.o $(MAPS) *.nl *.lst

realclean: clean
	-rm -f boot kernel $(USERS) usb.image floppy.image 

help:
	@head -n `grep -m 1 -n "##END" Makefile | cut -f 1 -d ':'` Makefile \
		| sed -e 's/^#[ ]\?//' -e 's/^##.*$$//'

