#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_leb128
#endif

#include "../src/leb128.c"
#include "test.h"

void signed_numbers_under_64_encode_as_themselves()
{
	int failI = -1;
	for (int i = 0; i < 64; i++) {
		int byteCount;
		u8 *bytes = leb128EncodeS(i, &byteCount);
		if (byteCount != 1 || *bytes != i) {
			failI = i;
			free(bytes);
			break;
		}
		free(bytes);
	}
	test("Signed numbers under 64 encode as themselves", failI == -1);
	if (failI != -1) printf("Failed at %d\n", failI);
}

void signed_numbers_above_64_require_more_bytes()
{
	int failI = -1;
	for (int i = 64; i < 255; i += 8) {
		int byteCount;
		u8 *bytes = leb128EncodeS(i, &byteCount);
		if (byteCount < 2) {
			failI = i;
			free(bytes);
			break;
		}
		free(bytes);
	}
	test("Signed numbers above 64 require more bytes", failI == -1);
	if (failI != -1) printf("Failed at %d\n", failI);
}

typedef struct {
	int n;
	u8 bytes[8];
} SignedLeb128TestData;

void signed_number_encodes_correctly(SignedLeb128TestData data)
{
	int byteCount;
	u8 *bytes = leb128EncodeS(data.n, &byteCount);

	bool pass = true;
	if (byteCount == 0) {
		pass = false;
	} else {
		for (int i = 0; i < byteCount; i++) {
			if (bytes[i] != data.bytes[i]) {
				pass = false;
				break;
			}
		}
	}

	free(bytes);
	char msg[64];
	sprintf(msg, "%d encodes correctly", data.n);
	test(msg, pass);
}

void signed_number_decodes_correctly(SignedLeb128TestData data)
{
	int value = leb128DecodeS(data.bytes);
	bool pass = value == data.n;

	char msg[64];
	sprintf(msg, "%d decodes correctly", data.n);
	test(msg, pass);
}

void test_leb128_signed()
{
	test_section("Signed LEB128 encoding");

	signed_numbers_under_64_encode_as_themselves();
	signed_numbers_above_64_require_more_bytes();

	signed_number_encodes_correctly((SignedLeb128TestData){64, {192, 0}});
	signed_number_encodes_correctly((SignedLeb128TestData){78, {206, 0}});
	signed_number_encodes_correctly((SignedLeb128TestData){2943, {255, 22}});
	signed_number_encodes_correctly((SignedLeb128TestData){0x7FFFFFFF, {255, 255, 255, 255, 7}});

	signed_number_encodes_correctly((SignedLeb128TestData){-1, {127}});
	signed_number_encodes_correctly((SignedLeb128TestData){-3884235, {181, 246, 146, 126}});

	test_section("Signed LEB128 decoding");

	signed_number_decodes_correctly((SignedLeb128TestData){0, {0}});
	signed_number_decodes_correctly((SignedLeb128TestData){1, {1}});
	signed_number_decodes_correctly((SignedLeb128TestData){63, {63}});
	signed_number_decodes_correctly((SignedLeb128TestData){64, {192, 0}});
	signed_number_decodes_correctly((SignedLeb128TestData){78, {206, 0}});
	signed_number_decodes_correctly((SignedLeb128TestData){2943, {255, 22}});
	signed_number_decodes_correctly((SignedLeb128TestData){0x7FFFFFFF, {255, 255, 255, 255, 7}});

	signed_number_decodes_correctly((SignedLeb128TestData){-1, {127}});
	signed_number_decodes_correctly((SignedLeb128TestData){-3884235, {181, 246, 146, 126}});
}

typedef struct {
	uint32_t n;
	u8 bytes[8];
} UnsignedLeb128TestData;

void unsigned_numbers_under_128_encode_as_themselves()
{
	int failI = -1;
	for (int i = 0; i < 128; i++) {
		int byteCount;
		u8 *bytes = leb128EncodeU(i, &byteCount);
		if (byteCount != 1 || *bytes != i) {
			failI = i;
			free(bytes);
			break;
		}
		free(bytes);
	}
	test("Unsigned numbers under 128 encode as themselves", failI == -1);
	if (failI != -1) printf("Failed at %d\n", failI);
}

void unsigned_numbers_above_128_require_more_bytes()
{
	int failI = -1;
	for (int i = 128; i < 255; i += 8) {
		int byteCount;
		u8 *bytes = leb128EncodeU(i, &byteCount);
		if (byteCount < 2) {
			failI = i;
			free(bytes);
			break;
		}
		free(bytes);
	}
	test("Unsigned numbers above 128 require more bytes", failI == -1);
	if (failI != -1) printf("Failed at %d\n", failI);
}

void unsigned_number_encodes_correctly(UnsignedLeb128TestData data)
{
	int byteCount;
	u8 *bytes = leb128EncodeU(data.n, &byteCount);

	bool pass = true;

	if (byteCount == 0) {
		pass = false;
	} else {

		for (int i = 0; i < byteCount; i++) {
			if (bytes[i] != data.bytes[i]) {
				pass = false;
				break;
			}
		}
	}

	free(bytes);
	char msg[64];
	sprintf(msg, "%zu encodes correctly", data.n);
	test(msg, pass);
}

void unsigned_number_decodes_correctly(UnsignedLeb128TestData data)
{
	int value = leb128DecodeU(data.bytes);
	bool pass = value == data.n;

	char msg[64];
	sprintf(msg, "%zu decodes correctly", data.n);
	test(msg, pass);
}

void test_leb128_unsigned()
{
	test_section("Unsigned LEB128 encoding");

	unsigned_numbers_under_128_encode_as_themselves();
	unsigned_numbers_above_128_require_more_bytes();

	unsigned_number_encodes_correctly((UnsignedLeb128TestData){127, {127}});
	unsigned_number_encodes_correctly((UnsignedLeb128TestData){128, {128, 1}});
	unsigned_number_encodes_correctly((UnsignedLeb128TestData){325, {197, 2}});
	unsigned_number_encodes_correctly((UnsignedLeb128TestData){28394, {234, 221, 1}});
	unsigned_number_encodes_correctly((UnsignedLeb128TestData){UINT32_MAX, {255, 255, 255, 255, 15}});

	test_section("Unsigned LEB128 decoding");

	unsigned_number_decodes_correctly((UnsignedLeb128TestData){0, {0}});
	unsigned_number_decodes_correctly((UnsignedLeb128TestData){1, {1}});
	unsigned_number_decodes_correctly((UnsignedLeb128TestData){127, {127}});
	unsigned_number_decodes_correctly((UnsignedLeb128TestData){128, {128, 1}});
	unsigned_number_decodes_correctly((UnsignedLeb128TestData){325, {197, 2}});
	unsigned_number_decodes_correctly((UnsignedLeb128TestData){28394, {234, 221, 1}});
	unsigned_number_decodes_correctly((UnsignedLeb128TestData){UINT32_MAX, {255, 255, 255, 255, 15}});
}

void test_leb128()
{
	test_leb128_signed();
	test_leb128_unsigned();
}
