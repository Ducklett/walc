#include "sti.h"

u8 bytes[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};

int main(int argc, char **argv)
{
	Buf wasm = BUF(bytes);
	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
}
