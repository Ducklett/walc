#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_sti
#endif

#include <sti_test.h>

void test_sti_math()
{
	test_section("sti math");

	test("min returns the minimum value", min(10, 20) == 10 AND min(-1, 1) == -1 AND min(.33, 1.93) == .33);
	test("max returns the maximum value", max(10, 20) == 20 AND max(-1, 1) == 1 AND max(.33, 1.93) == 1.93);
	test("clamp constrains value to boundry",
		 clamp(11, 10, 20) == 11 AND clamp(9, 10, 20) == 10 AND clamp(24, 10, 20) == 20);
}

void test_sti_str()
{
	test_section("sti str");

	test("str length is the length of the string excluding the null terminator",
		 STR("Hello world").len == sizeof("Hello world") - 1);
	test("str buffer is equal to its string counterpart", strncmp(STR("Foo").buf, "Foo", 3) == 0);
	test("String with no characters is equal to STREMPTY", strEqual(STR(""), STREMPTY));
	test("Strings with the same characters are equal", strEqual(STR("Hello"), STR("Hello")));
	test("strFromCstr and STR produce the same result for string literals",
		 strEqual(STR("Hello"), strFromCstr("Hello")));
	test("strFromCstr returns an empty Str when given a null pointer", strEqual(STREMPTY, strFromCstr(NULL)));

	test("Strings with different characters are not equal", NOT strEqual(STR("Hello"), STR("Goodbye")));

	{
		//                0   4 6             20
		//                v   v v             v
		Str source = STR("Hello world this is a test");
		test("Slice returns the expected value when given a legal range",
			 strEqual(STR("Hello"), strSlice(source, 0, 5)));
		test("Slice is clamped when from is too short", strEqual(STR("Hello"), strSlice(source, -1, 6)));
		test("Slice is clamped when from is too long", strEqual(STREMPTY, strSlice(source, 999, 1)));
		test("Slice is clamped when len is too long", strEqual(STR("a test"), strSlice(source, 20, 999)));
	}

	{
		Str source = STR("Hello world!");
		Str name = strSlice(source, 0, 5); // not null terminated!

		Str nameCopy = strAlloc(name);
		test("strings created with strAlloc are null terminated", nameCopy.buf[nameCopy.len] == '\0');

		strFree(&nameCopy);
		test("Str's become empty after you strFree() them", strEqual(nameCopy, STREMPTY));
	}
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

		dynamicBufFree(&buf);
		test("DynmicBuf's become empty after you dynamicBufFree() them", bufEqual(dynamicBufToBuf(buf), BUFEMPTY));
	}
}

void test_sti_list()
{
	test_section("sti list");

	List(int) numList = listNew();

	test_that("New list is zero initialized")
	{
		test_assert("New list is empty", listLen(numList) == 0);
		test_assert("listIsEmpty is true for empty list", listIsEmpty(numList));
		test_assert("listIsNotEmpty is false for empty list", !listIsNotEmpty(numList));
		test_assert("New list has no capacity", listCapacity(numList) == 0);
		test_assert("List knows element size", listElementSize(numList) == sizeof(int));
	}

	test_that("An element can be pushed into a list")
	{
		listPush(&numList, 10);

		test_assert("The list length becomes 1", listLen(numList) == 1);
		test_assert("listIsEmpty is false for non-empty list", !listIsEmpty(numList));
		test_assert("listIsNotEmpty is true for non-empty list", listIsNotEmpty(numList));
		test_assert("The value can be retrieved with []", numList[0] == 10);
		test_assert("The list capacity becomes the minimum", listCapacity(numList) == 0x10);
	}

	test_that("Additional elements can be added to the list")
	{
		for (int i = 2; i <= 23; i++) {
			listPush(&numList, i);
		}

		test_assert("List length becomes 23", listLen(numList) == 23);
		test_assert("Capacity doubles when you run out", listCapacity(numList) == 0x20);

		for (int i = 1; i < 23; i++) {
			test_assert(strFormat("list[%d]==%d+1", i, i).buf, numList[i] == i + 1);
		}
	}

	test_that("List pop")
	{
		test_assert("Returns last item", listPop(&numList) == 23);
		test_assert("Decrements list length", listLen(numList) == 22);

		List(u8) emptyList = listNew();
		test_assert_panic("Panics when popping an empty list", listPop(&emptyList));
	}

	test_that("List can be freed")
	{
		listFree(&numList);
		test_assert("List becomes NULL", numList == NULL);
		test_assert("Length becomes 0", listLen(numList) == 0);
		test_assert("Capacity becomes 0", listCapacity(numList) == 0);
	}
}

void test_sti_string()
{
	test_section("sti string");

	{
		String str = stringCreate();

		test("Newly created string is empty", str.len == 0);
		test("Newly created buffer has zero capacity", str.capacity == 0);
		stringPush(&str, 'H');
		stringPush(&str, 'e');
		stringPush(&str, 'l');
		test("String capcity grows to the minimun amount", str.capacity == 16);
		test("String length grows after pushing elements", str.len == 3);
		test("Newly pushed elements have their expected values",
			 str.buf[0] == 'H' AND str.buf[1] == 'e' AND str.buf[2] == 'l');

		stringAppend(&str, STR("lo world this is a test"));
		test("String capacity doubles when it runs out of space", str.capacity == 32);

		Str expected = STR("Hello world this is a test");
		test("String converted to Str has the expected values", strEqual(stringToStr(str), expected));

		stringFree(&str);
		test("strings become empty after you stringFree() them", strEqual(stringToStr(str), STREMPTY));
	}
}

void test_sti_arena()
{
	test_section("sti arena");
	ArenaAllocator allocator = arenaCreate();
	test("new areana will have one page", allocator.current != NULL && allocator.current == allocator.first);

	int *addr;
	int *initialAddr;
	addr = arenaMalloc(sizeof(int), &allocator);
	initialAddr = addr;
	*addr = 10;
	addr = arenaMalloc(sizeof(int), &allocator);
	*addr = 20;
	addr = arenaMalloc(sizeof(int), &allocator);
	*addr = 30;

	test("arena allocates values in page contiguously",
		 initialAddr[0] == 10 && initialAddr[1] == 20 && initialAddr[2] == 30);

	addr = arenaMalloc(0xAFFFF, &allocator);

	test("arena allocates a bigger page is the payload wouldn't fit",
		 allocator.first != allocator.current AND allocator.current->capacity >= 0xAFFFF);

	arenaFree(&allocator);
	test("all memory in an arena can be freed at once", allocator.first == NULL && allocator.current == NULL);
}

void test_sti()
{
	test_section("sti");
	test_sti_math();
	test_sti_str();
	test_sti_buf();
	test_sti_dynamicBuf();
	test_sti_list();
	test_sti_string();
	test_sti_arena();
}
