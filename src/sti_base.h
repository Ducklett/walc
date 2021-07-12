/*
STI: The standard interface library

Provides some essentials to make working with C less of a pain
*/
#ifndef STI_H
#define STI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdarg.h"
#include <assert.h>
#include <inttypes.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define ABS(a)				 ((a) < 0 ? (-a) : (a))
#define min(a, b)			 ((a) < (b) ? (a) : (b))
#define max(a, b)			 ((a) > (b) ? (a) : (b))
#define clamp(v, minv, maxv) min(maxv, max(minv, v))

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

void *smalloc(size_t size);

// A string encdoded as a character buffer and a length
// The str may or may not be null terminated depending on how it was created
// The null terminator is not included in the string lenght
typedef struct {
	char *buf;
	size_t len;
} Str;

// creates {Str} from a string literal
#define STR(x) ((Str){x, sizeof(x) - 1})

// makes is so you can print a string in one go with %.*s
#define STRPRINT(x) ((int)(x.len)), (x.buf)

// an empty {Str}
const Str STREMPTY = {0};

// creates {Str} from a null terminated {char*}
// if NULL is passed to it an empty Str is returned
Str strFromCstr(char *s) { return (Str){s, s == NULL ? 0 : strlen(s)}; }

// returns {true} if the two strings a equal
bool strEqual(Str a, Str b)
{
	if (a.len != b.len) return false;
	for (int i = 0; i < a.len; i++) {
		if (a.buf[i] != b.buf[i]) return false;
	}
	return true;
}

// creates a string view from the specified range
// the range is clamped if it would fall outside of the parent Str's bounds
// the returned {Str} is *not* null terminated
// and the parent Str {a} must be kept alive while the slice is used
Str strSlice(Str a, int from, int len)
{
	int clampedFrom = clamp(from, 0, a.len);
	// if {from} was shifted forward the length should shink accordingly
	len += (from - clampedFrom);
	int clampedLen = min(a.len - clampedFrom, len);
	return (Str){a.buf + clampedFrom, clampedLen};
}

// copies the Str onto the heap and adds a null terminator to the end
Str strAlloc(Str a)
{
	char *buf = smalloc(a.len + 1);
	memcpy(buf, a.buf, a.len);
	buf[a.len] = '\0';
	return (Str){.buf = buf, .len = a.len};
}

// allocates formatted string
Str strFormat(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	char *s = smalloc(len + 1);
	s[len] = 0;

	va_start(args, fmt);
	vsnprintf(s, len, fmt, args);
	va_end(args);

	return (Str){s, len};
}

// frees a Str that was allocated on the heap
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

typedef struct {
	char *buf;
	int len;
	int capacity;
} String;

static inline String stringCreateWithCapacity(int capacity)
{
	return (String){.buf = smalloc(capacity), .capacity = capacity, .len = 0};
}
static inline String stringCreate() { return stringCreateWithCapacity(0); }
void stringPush(String *b, char byte)
{
	bool shouldGrow = b->capacity == b->len;
	if (shouldGrow) {
		int newCapacity = b->capacity < 16 ? 16 : b->capacity * 2;
		char *buf = smalloc(newCapacity);
		if (b->capacity) {
			memcpy(buf, b->buf, b->len);
			free(b->buf);
		}
		b->buf = buf;
		b->capacity = newCapacity;
	}
	b->buf[b->len++] = byte;
}
void stringAppend(String *b, Str content)
{
	for (int i = 0; i < content.len; i++) {
		stringPush(b, content.buf[i]);
	}
}
Str stringToStr(String b) { return (Str){.buf = b.buf, .len = b.len}; }
// converts the string into a null terminated cstr
// the null termination will be removed again when new data is appended
char *stringToCStr(String *b)
{
	stringPush(b, '\0');
	b->len--;
	return b->buf;
}
void stringFree(String *b)
{
	free(b->buf);
	(*b) = (String){0};
}

// prints the provided message and then kills the program
// always returns {false} so it can be used in binary expressions: {doThis() || panic("Failed")}
bool panic_impl(const char *msg, const char *filename, const int line, ...)
{
	printf("%s%s %d PANIC: %s", TERMRED, filename, line, TERMCLEAR);
	va_list args;
	va_start(args, line);
	vfprintf(stderr, msg, args);
	va_end(args);

	printf("\n");
	exit(1);
	return false;
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

const size_t arenaPageSize = 0xFFFF;

typedef struct arena_page_t {
	void *memory;
	size_t capacity;
	size_t length;
	struct arena_page_t *next;
} ArenaPage;

typedef struct {
	ArenaPage *first;
	ArenaPage *current;
} ArenaAllocator;

ArenaPage *arenaCreatePageWithCapacity(size_t capacity)
{
	ArenaPage *memory = smalloc(capacity);

	memory[0] = (ArenaPage){memory, capacity, sizeof(ArenaPage), NULL};
	return memory;
}

ArenaPage *arenaCreatePage() { return arenaCreatePageWithCapacity(arenaPageSize); }

ArenaAllocator arenaCreate()
{
	ArenaPage *p = arenaCreatePage();
	return (ArenaAllocator){p, p};
}

void *arenaMalloc(size_t bytes, ArenaAllocator *alloc)
{
	ArenaPage *currentPage = alloc->current;

	size_t availableSpace = currentPage->capacity - currentPage->length;
	if (availableSpace < bytes) {
		size_t pageSize = max(arenaPageSize, bytes + sizeof(ArenaPage));
		ArenaPage *newPage = arenaCreatePageWithCapacity(pageSize);
		alloc->current = newPage;
		currentPage->next = newPage;
		currentPage = newPage;
	}

	void *offset = ((u8 *)currentPage->memory) + currentPage->length;
	currentPage->length += bytes;
	return offset;
}

void arenaFree(ArenaAllocator *alloc)
{
	ArenaPage *page = alloc->first;

	int i = 0;
	while (page) {
		ArenaPage *nextPage = page->next;
		free(page);
		page = nextPage;
		i++;
	}
	alloc->current = NULL;
	alloc->first = NULL;
}

#ifdef __cplusplus
}
#endif

#endif // STI_H
