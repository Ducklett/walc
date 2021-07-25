#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_main
#endif

#include <sti_test.h>

#include <binder.test.c>
#include <leb128.test.c>
#include <parser.test.c>
#include <sti.test.c>
#include <walc.test.c>
#include <wasm.test.c>

void test_main()
{
	test_sti();
	test_leb128();
	test_wasm();
	test_parser();
	test_binder();
	test_walc();
}
