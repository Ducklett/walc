#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_main
#endif

#include "test.h"
#include "leb128.test.c"
#include "wasm.test.c"

void test_main()
{
	test_leb128();
	test_wasm();
}
