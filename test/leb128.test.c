#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_leb128
#endif

#include <leb128.h>
#include <sti_test.h>

typedef struct {
	int n;
	u8 bytes[8];
} SignedLeb128TestData;

void test_leb128_signed()
{
	test_section("leb128 signed encoding");

	test_that("Signed numbers under 64 encode as themselves")
	{
		for (int i = 0; i < 64; i++) {
			DynamicBuf bytes = dynamicBufCreate();
			leb128EncodeS(i, &bytes);
			test_assert(cstrFormat("%d length is 1", i), bytes.len == 1);
			test_assert(cstrFormat("%d encodes itself", i), bytes.buf[0] == i);
			dynamicBufFree(&bytes);
		}
	}

	test_that("Signed numbers above 64 require more bytes")
	{
		for (int i = 64; i < 255; i += 8) {
			DynamicBuf bytes = dynamicBufCreate();
			leb128EncodeS(i, &bytes);
			test_assert(cstrFormat("%d length > 1", i), bytes.len > 1);
			dynamicBufFree(&bytes);
		}
	}

	{
		SignedLeb128TestData testData[] = {
			{64, {192, 0}},
			//
			{78, {206, 0}},
			{2943, {255, 22}},
			{0x7FFFFFFF, {255, 255, 255, 255, 7}},
			{-1, {127}},
			{-3884235, {181, 246, 146, 126}},
		};

		test_theory("Signed numbers encode correctly", SignedLeb128TestData, testData)
		{
			SignedLeb128TestData data = testData[i];
			DynamicBuf bytes = dynamicBufCreate();
			leb128EncodeS(data.n, &bytes);

			test_assert(cstrFormat("%d encodes in non-zero bytes", data.n), bytes.len > 0);
			test_assert(cstrFormat("%d encodes correctly", data.n),
						bufEqual(dynamicBufToBuf(bytes), (Buf){data.bytes, bytes.len}));

			dynamicBufFree(&bytes);
		}
	}

	test_section("leb128 signed decoding");

	{
		SignedLeb128TestData testData[] = {
			{0, {0}},
			//
			{1, {1}},
			{63, {63}},
			{64, {192, 0}},
			{78, {206, 0}},
			{2943, {255, 22}},
			{0x7FFFFFFF, {255, 255, 255, 255, 7}},
			{-1, {127}},
			{-3884235, {181, 246, 146, 126}},
		};

		test_theory("Signed numbers decode correctly", SignedLeb128TestData, testData)
		{
			SignedLeb128TestData data = testData[i];
			int value = leb128DecodeS(data.bytes);
			bool pass = value == data.n;

			test_assert(cstrFormat("%d decodes correctly", data.n), value == data.n);
		}
	}
}

typedef struct {
	uint32_t n;
	u8 bytes[8];
} UnsignedLeb128TestData;

void test_leb128_unsigned()
{
	test_section("leb128 unsigned encoding");

	test_that("Unigned numbers under 128 encode as themselves")
	{
		for (int i = 0; i < 128; i++) {
			DynamicBuf bytes = dynamicBufCreate();
			leb128EncodeU(i, &bytes);
			test_assert(cstrFormat("%d length is 1", i), bytes.len == 1);
			test_assert(cstrFormat("%d encodes itself", i), bytes.buf[0] == i);
			dynamicBufFree(&bytes);
		}
	}

	test_that("Unsigned numbers above 128 require more bytes")
	{
		for (int i = 128; i < 255; i += 8) {
			DynamicBuf bytes = dynamicBufCreate();
			leb128EncodeU(i, &bytes);
			test_assert(cstrFormat("%d length > 1", i), bytes.len > 1);
			dynamicBufFree(&bytes);
		}
	}

	{
		UnsignedLeb128TestData testData[] = {
			{127, {127}},
			{128, {128, 1}},
			{325, {197, 2}},
			{28394, {234, 221, 1}},
			{UINT32_MAX, {255, 255, 255, 255, 15}},
		};

		test_theory("Unsigned numbers encode correctly", UnsignedLeb128TestData, testData)
		{
			UnsignedLeb128TestData data = testData[i];
			DynamicBuf bytes = dynamicBufCreate();
			leb128EncodeU(data.n, &bytes);

			test_assert(cstrFormat("%zu encodes in non-zero bytes", data.n), bytes.len > 0);
			test_assert(cstrFormat("%zu encodes correctly", data.n),
						bufEqual(dynamicBufToBuf(bytes), (Buf){data.bytes, bytes.len}));

			dynamicBufFree(&bytes);
		}
	}

	test_section("leb128 unsigned decoding");

	{
		UnsignedLeb128TestData testData[] = {
			{0, {0}},
			//
			{1, {1}},
			{127, {127}},
			{128, {128, 1}},
			{325, {197, 2}},
			{28394, {234, 221, 1}},
			{UINT32_MAX, {255, 255, 255, 255, 15}},
		};

		test_theory("Unsigned numbers decode correctly", UnsignedLeb128TestData, testData)
		{
			UnsignedLeb128TestData data = testData[i];
			int value = leb128DecodeU(data.bytes);
			bool pass = value == data.n;

			test_assert(cstrFormat("%d decodes correctly", data.n), value == data.n);
		}
	}
}

void test_leb128()
{
	test_section("leb128");
	test_leb128_signed();
	test_leb128_unsigned();
}
