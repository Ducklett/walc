#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_wasm
#endif

#include "test.h"
#include <string.h>

void test_sti_str()
{
	test("str length is the length of the string excluding the null terminator",
		 STR("Hello world").len == sizeof("Hello world") - 1);
	test("str buffer is equal to its string counterpart", strncmp(STR("Foo").buf, "Foo", 3) == 0);
	test("String with no characters is equal to STREMPTY", strEqual(STR(""), STREMPTY));
	test("Strings with the same characters are equal", strEqual(STR("Hello"), STR("Hello")));
	test("Strings with different characters are not equal", NOT strEqual(STR("Hello"), STR("Goodbye")));
}

void test_sti_buf()
{
	{
		u8 bytesA[] = {0xFF, 0x56, 0x83};
		u8 bytesB[] = {0xFC, 0x66, 0x83};
		u8 bytesC[] = {0xFF, 0x56, 0x83};

		Buf A = BUF(bytesA);
		Buf B = BUF(bytesB);
		Buf C = BUF(bytesC);

		test("Buf is equal to itself", bufEqual(A, A));
		test("Buf with the same bytes are equal", bufEqual(A, C));
		test("Buf with different bytes are not equal", NOT bufEqual(A, B));
	}

	{
		Str text = STR("foo");
		Buf textBuf = STRTOBUF(text);
		u8 expectedBytes[] = {'f', 'o', 'o'};
		Buf expected = BUF(expectedBytes);

		test("Converting a Str to Buf keeps data and length intact", bufEqual(textBuf, expected));
	}
}

void test_sti_panic()
{
	test("didPanic is false if we haven't called panic", didPanic == false);
	PANIC("panic test");
	test("didPanic is true after we called panic", didPanic == true);
	test("didPanic gets cleared after running a test", didPanic == false);
}

void test_sti()
{
	test_section("sti");
	test_section("sti str");
	test_sti_str();
	test_section("sti buf");
	test_sti_buf();
	test_section("sti panic");
	test_sti_panic();
}
