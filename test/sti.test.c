#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_wasm
#endif

#include <string.h>
#include <test.h>

void test_sti_str()
{
	test_section("sti str");

	test("str length is the length of the string excluding the null terminator",
		 STR("Hello world").len == sizeof("Hello world") - 1);
	test("str buffer is equal to its string counterpart", strncmp(STR("Foo").buf, "Foo", 3) == 0);
	test("String with no characters is equal to STREMPTY", strEqual(STR(""), STREMPTY));
	test("Strings with the same characters are equal", strEqual(STR("Hello"), STR("Hello")));
	test("Strings with different characters are not equal", NOT strEqual(STR("Hello"), STR("Goodbye")));
}

void test_sti_buf()
{
	test_section("sti buf");
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

void test_sti_dynamicBuf()
{
	test_section("sti dynamicBuf");

	{
		DynamicBuf buf = dynamicBufCreate();

		test("Newly created buffer is empty", buf.len == 0);
		test("Newly created buffer has zero capacity", buf.capacity == 0);
		dynamicBufPush(&buf, 10);
		dynamicBufPush(&buf, 20);
		dynamicBufPush(&buf, 30);
		test("Buffer capcity grows to the minimun amount", buf.capacity == 16);
		test("Buffer length grows after pushing elements", buf.len == 3);
		test("Newly pushed elements have their expected values",
			 buf.buf[0] == 10 AND buf.buf[1] == 20 AND buf.buf[2] == 30);

		for (int i = buf.len; i < 19; i++) {
			dynamicBufPush(&buf, i);
		}
		test("Buffer capacity doubles when it runs out of space", buf.capacity == 32);

		u8 expected[] = {10, 20, 30, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
		test("DynamicBuf converted to Buf has the expected values", bufEqual(dynamicBufToBuf(buf), BUF(expected)));
	}
}

void test_sti_panic()
{
	test_section("sti panic");

	test("didPanic is false if we haven't called panic", didPanic == false);
	PANIC("panic test");
	test("didPanic is true after we called panic", didPanic == true);
	test("didPanic gets cleared after running a test", didPanic == false);
}

void test_sti()
{
	test_section("sti");
	test_sti_str();
	test_sti_buf();
	test_sti_dynamicBuf();
	test_sti_panic();
}
