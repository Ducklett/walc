#ifndef LEB128_H
#define LEB128_H
#include <sti_base.h>

typedef uint8_t u8;

uint32_t leb128DecodeU(u8 *bytes)
{
	uint32_t result = 0;
	int shift = 0;
	int i = 0;
	while (true) {
		u8 byte = bytes[i++];
		result |= (byte & 0x7F) << shift;
		if ((byte & 0x80) == 0) {
			break;
		}
		shift += 7;
	}
	return result;
}

void leb128EncodeU(uint32_t number, DynamicBuf *buf)
{
	int n = 0;

	int i = 0;
	do {
		u8 byte = number & 0x7F;
		number >>= 7;
		if (number != 0) {
			/* more bytes to come */
			byte |= 0x80;
		}

		dynamicBufPush(buf, byte);
		n++;
		if (i++ > 8) break;
	} while (number != 0);
}

int leb128DecodeS(u8 *bytes)
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

void leb128EncodeS(int number, DynamicBuf *buf)
{
	const int maxBytes = 8;
	int i;
	for (i = 0; i < maxBytes; i++) {
		u8 byte = number & 0x7f;
		number >>= 7;

		const u8 SIGNBIT = 0x40;
		bool isSigned = byte & SIGNBIT;
		bool atEndOfNumber = (number == 0 && !isSigned) || (number == -1 && isSigned);
		if (atEndOfNumber) {
			dynamicBufPush(buf, byte);
			break;
		} else {
			const u8 MSB = 0x80;
			dynamicBufPush(buf, byte | MSB);
		}
	}

	assert((number == 0 || number == -1) && "Number should be fully consumed");
}

#endif // LEB128_H
