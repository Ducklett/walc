#include <assert.h>
#include <inttypes.h>
#include <memory.h>
#include <stdbool.h>
#include <stdlib.h>

typedef uint8_t u8;

int leb128Decode(u8 *bytes)
{
	int value = 0;
	int shift = 0;
	for (int i = 0;; i++) {
		u8 byte = bytes[i];
		value += ((byte & 0x7F) << shift);
		shift += 7;

		bool isLast = !(byte & 0x80);
		if (isLast) {
			bool isSigned = byte & 0x40;
			if (shift < 32 && (isSigned)) {
				return value | (~0 << shift);
			} else {
				return value;
			}
		}
	}
}

u8 *leb128Encode(int number, int *byteCount)
{
	const int maxBytes = 8;
	u8 *bytes = malloc(maxBytes);
	int i;
	for (i = 0; i < maxBytes; i++) {
		u8 byte = number & 0x7f;
		number >>= 7;

		const u8 SIGNBIT = 0x40;
		bool isSigned = byte & SIGNBIT;
		bool atEndOfNumber = (number == 0 && !isSigned) || (number == -1 && isSigned);
		if (atEndOfNumber) {
			bytes[i] = byte;
			break;
		} else {
			const u8 MSB = 0x80;
			bytes[i] = byte | MSB;
		}
	}

	assert((number == 0 || number == -1) && "Number should be fully consumed");

	*byteCount = i + 1;

	return bytes;
}
