#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_leb128
#endif

#include "../src/leb128.c"
#include "test.h"

void numbers_under_64_encode_as_themselves()
{
	int failI = -1;
	for (int i = 0; i < 64; i++) {
		int byteCount;
		u8 *bytes = leb128Encode(i, &byteCount);
		if (byteCount != 1 || *bytes != i) {
			failI = i;
			free(bytes);
			break;
		}
		free(bytes);
	}
	test("Numbers under 64 encode as themselves", failI == -1);
	if (failI != -1) printf("Failed at %d\n", failI);
}

void numbers_above_64_require_more_bytes()
{
	int failI = -1;
	for (int i = 64; i < 255; i += 8) {
		int byteCount;
		u8 *bytes = leb128Encode(i, &byteCount);
		if (byteCount < 2) {
			failI = i;
			free(bytes);
			break;
		}
		free(bytes);
	}
	test("Numbers above 64 require more bytes", failI == -1);
	if (failI != -1) printf("Failed at %d\n", failI);
}

typedef struct {
	int n;
	u8 bytes[8];
} Leb128TestData;

void number_encodes_correctly(Leb128TestData data)
{
	int byteCount;
	u8 *bytes = leb128Encode(data.n, &byteCount);

	bool pass = true;
	for (int i = 0; i < byteCount; i++) {
		if (bytes[i] != data.bytes[i]) {
			pass = false;
			break;
		}
	}

	free(bytes);
	char msg[64];
	sprintf(msg, "%d encodes correctly", data.n);
	test(msg, pass);
}

void number_decodes_correctly(Leb128TestData data)
{
	int value = leb128Decode(data.bytes);
	bool pass = value == data.n;

	char msg[64];
	sprintf(msg, "%d decodes correctly", data.n);
	test(msg, pass);
}

void test_leb128()
{
	test_section("LEB128 encoding");

	numbers_under_64_encode_as_themselves();
	numbers_above_64_require_more_bytes();

	number_encodes_correctly((Leb128TestData){64, {192, 0}});
	number_encodes_correctly((Leb128TestData){78, {206, 0}});
	number_encodes_correctly((Leb128TestData){2943, {255, 22}});
	number_encodes_correctly((Leb128TestData){0x7FFFFFFF, {255, 255, 255, 255, 7}});

	number_encodes_correctly((Leb128TestData){-1, {127}});
	number_encodes_correctly((Leb128TestData){-3884235, {181, 246, 146, 126}});

	test_section("LEB128 decoding");

	number_decodes_correctly((Leb128TestData){0, {0}});
	number_decodes_correctly((Leb128TestData){1, {1}});
	number_decodes_correctly((Leb128TestData){63, {63}});
	number_decodes_correctly((Leb128TestData){64, {192, 0}});
	number_decodes_correctly((Leb128TestData){78, {206, 0}});
	number_decodes_correctly((Leb128TestData){2943, {255, 22}});
	number_decodes_correctly((Leb128TestData){0x7FFFFFFF, {255, 255, 255, 255, 7}});

	number_decodes_correctly((Leb128TestData){-1, {127}});
	number_decodes_correctly((Leb128TestData){-3884235, {181, 246, 146, 126}});
}
