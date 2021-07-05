#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_main
#endif

#include "leb128.test.c"
#include "sti.test.c"
#include "test.h"
#include "wasm.test.c"

void test_main()
{
	test_sti();
	test_leb128();
	test_wasm();
}
