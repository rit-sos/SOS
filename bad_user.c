#include "headers.h"

void main(void) {
	char opcodes[256];
	void (*f)(void) = (void(*)(void))opcodes;
	int i;
	float fl;

	for (i = 0; i < 256; i += 2) {
		opcodes[i] = 0x0f;
		opcodes[i+1] = 0x0b;
	}

	asm("cli");

	*(Uint32*)(0xd0000000) = 0xffff;
}
