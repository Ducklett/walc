/*
STI: The standard interface library

Provides some essentials to make working with C less of a pain
*/
#ifndef STI_H
#define STI_H

#include "stdarg.h"
#include <assert.h>
#include <inttypes.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define TERMCLEAR		"\033[0m"
#define TERMRED			"\033[0;31m"
#define TERMGREEN		"\033[0;32m"
#define TERMYELLOW		"\033[0;33m"
#define TERMBLUE		"\033[0;34m"
#define TERMMAGENTA		"\033[0;35m"
#define TERMCYAN		"\033[0;36m"
#define TERMBOLD		"\033[1m"
#define TERMBOLDRED		"\033[1;31m"
#define TERMBOLDGREEN	"\033[1;32m"
#define TERMBOLDYELLOW	"\033[1;33m"
#define TERMBOLDBLUE	"\033[1;34m"
#define TERMBOLDMAGENTA "\033[1;35m"
#define TERMBOLDCYAN	"\033[1;36m"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

void *smalloc(size_t size);

typedef struct {
	char *buf;
	size_t len;
} Str;
#define STR(x)	 ((Str){x, sizeof(x) - 1})
#define STREMPTY ((Str){0, 0})

bool strEqual(Str a, Str b)
{
	if (a.len != b.len) return false;
	for (int i = 0; i < a.len; i++) {
		if (a.buf[i] != b.buf[i]) return false;
	}
	return true;
}

void strFree(Str *b)
{
	free(b->buf);
	(*b) = (Str){0};
}

typedef struct {
	u8 *buf;
	size_t len;
} Buf;
#define BUF(x)	 ((Buf){x, sizeof(x)})
#define BUFEMPTY ((Buf){0})

#define STRTOBUF(x) ((Buf){(u8 *)(x.buf), (x.len)})

bool bufEqual(Buf a, Buf b)
{
	if (a.len != b.len) return false;
	for (int i = 0; i < a.len; i++) {
		if (a.buf[i] != b.buf[i]) {
			return false;
		}
	}
	return true;
}

void bufFree(Buf *b)
{
	free(b->buf);
	(*b) = (Buf){0};
}

typedef struct {
	u8 *buf;
	int len;
	int capacity;
} DynamicBuf;

static inline DynamicBuf dynamicBufCreateWithCapacity(int capacity)
{
	return (DynamicBuf){.buf = smalloc(capacity), .capacity = capacity, .len = 0};
}
static inline DynamicBuf dynamicBufCreate() { return dynamicBufCreateWithCapacity(0); }
void dynamicBufPush(DynamicBuf *b, u8 byte)
{
	bool shouldGrow = b->capacity == b->len;
	if (shouldGrow) {
		int newCapacity = b->capacity < 16 ? 16 : b->capacity * 2;
		u8 *buf = smalloc(newCapacity);
		if (b->capacity) {
			memcpy(buf, b->buf, b->len);
			free(b->buf);
		}
		b->buf = buf;
		b->capacity = newCapacity;
	}
	b->buf[b->len++] = byte;
}
void dynamicBufAppend(DynamicBuf *b, Buf content)
{
	for (int i = 0; i < content.len; i++) {
		dynamicBufPush(b, content.buf[i]);
	}
}
Buf dynamicBufToBuf(DynamicBuf b) { return (Buf){.buf = b.buf, .len = b.len}; }
void dynamicBufFree(DynamicBuf *b)
{
	free(b->buf);
	(*b) = (DynamicBuf){0};
}

// in certain environments like during testing you might want to test if a panic happened
// without actually quitting the application. this can be achieved by defining PANIC_SOFT
#ifdef PANIC_SOFT
static bool didPanic = false;
#endif
// prints the provided message and then kills the program
// always returns {false} so it can be used in binary expressions: {doThis() || panic("Failed")}
bool panic_impl(const char *msg, const char *filename, const int line, ...)
{
#ifdef PANIC_SOFT
	didPanic = true;
	return false;
#else
	printf("%s%s %d PANIC: %s", TERMRED, filename, line, TERMCLEAR);
	va_list args;
	va_start(args, line);
	vfprintf(stderr, msg, args);
	va_end(args);

	printf("\n");
	exit(1);
	return false;
#endif
}

#define PANIC(msg, ...) panic_impl(msg, __FILE__, __LINE__, ##__VA_ARGS__)

// indicates that a piece of code hasn't been implemented yet
// prints the TODO message and then kills the program
void todo_impl(const char *msg, const char *filename, const int line, ...)
{
	printf("%s%s %d TODO: %s", TERMBOLD, filename, line, TERMCLEAR);
	va_list args;
	va_start(args, line);
	vfprintf(stderr, msg, args);
	va_end(args);

	printf("\n");
	exit(1);
}

#define TODO(msg, ...) todo_impl(msg, __FILE__, __LINE__, ##__VA_ARGS__)

// "safe" malloc wrapper that instantly shuts down the application on allocation failure
void *smalloc(size_t size)
{
	void *data = malloc(size);
	if (!data) PANIC("Allocation failed");
	return data;
}

// writes all the bytes from the {buffer} into the file
// returns {false} on failure
bool fileWriteAllBytes(const char *filename, const Buf buffer)
{
	FILE *fp = fopen(filename, "wb+");
	if (!fp) return false;
	fwrite(buffer.buf, 1, buffer.len, fp);
	fclose(fp);
	return true;
}

#endif
