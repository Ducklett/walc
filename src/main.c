#include "sti.h"

u8 wasmMagic[] = {0x00, 0x61, 0x73, 0x6D};
u8 wasmModule[] = {0x01, 0x00, 0x00, 0x00};

typedef u8 WasmType;
#define WasmType_F64 0x7C
#define WasmType_F32 0x7D
#define WasmType_I64 0x7E
#define WasmType_i32 0x7F

typedef u8 WasmOp;
#define WasmOp_I32Const 0x41
#define WasmOp_I32Add	0x6A

typedef struct {
	int paramCount;
	WasmType *params;
	int returnCount;
	WasmType *returns;
} WasmHeader;

typedef struct {
	int id;
	int headerIndex;
	Str name;
	WasmType *locals;
	int localsCount;
	WasmOp *opcodes;
	int opcodesCount;
} WasmFunc;

typedef struct {
	int id;
	int headerIndex;
	Str *path;
	int pathCount;
} WasmImport;

typedef struct {
	Str name;
	int pages;
	int maxPages;
} WasmMemory;

typedef struct {
	bool hasMemory;
	bool hasExports;
	WasmMemory memory;
	WasmHeader *headers;
	int headerCount;
	WasmFunc *bodies;
	int bodyCount;
	WasmImport *imports;
	int importCount;

	int funcCount;
} Wasm;

Wasm wasmModuleCreate()
{
	Wasm module = {
		.bodies = malloc(sizeof(WasmFunc *) * 0xFF),
		.headers = malloc(sizeof(WasmHeader *) * 0xFF),
		0,
	};

	return module;
}

int wasmModuleGenerateFunctionId(Wasm *module) { return module->funcCount++; }

void wasmModuleAddMemory(Wasm *module, Str name, int pages, int maxPages)
{
	if (module->hasMemory) PANIC("Memory should only be set once.");
	module->hasMemory = true;
	if (name.len != 0) module->hasExports = true;
	module->memory = (WasmMemory){
		.name = name,
		.pages = pages,
		.maxPages = maxPages,
	};
}

// adds a new function to the module
// the provided buffers shouldn't be freed until you are done with the {Wasm} object
wasmModuleAddFunction(Wasm *module, Str name, Buf args, Buf rets, Buf locals, Buf opcodes)
{
	// TODO: compare headers
	// TODO: use some kind of bump/arena allocator

	if (name.len != 0) module->hasExports = true;

	module->headers[module->headerCount++] = (WasmHeader){args.len, args.buf, rets.len, rets.buf};
	int id = module->bodyCount;
	module->bodyCount++;
	module->bodies[id] = (WasmFunc){
		.id = id,
		.headerIndex = module->headerCount - 1,
		.name = name,
		.locals = locals.buf,
		.localsCount = locals.len,
		.opcodes = opcodes.buf,
		.opcodesCount = opcodes.len,
	};
}

static inline void wasmPushBytes(Buf *buf, const Buf bytesToPush)
{
	memcpy(buf->buf + buf->len, bytesToPush.buf, bytesToPush.len);
	buf->len += bytesToPush.len;
}

static inline void wasmPushByte(Buf *buf, const u8 byte) { buf->buf[buf->len++] = byte; }
static inline u8 *wasmReserveByte(Buf *buf) { return buf->buf + buf->len++; }

Buf wasmModuleCompile(Wasm module)
{
	Buf bytecode = {malloc(0xFFFF), 0};
	wasmPushBytes(&bytecode, BUF(wasmMagic));
	wasmPushBytes(&bytecode, BUF(wasmModule));

	if (module.headerCount) {
		wasmPushByte(&bytecode, 0x01);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int headerstart = bytecode.len;
		wasmPushByte(&bytecode, module.headerCount);
		for (int i = 0; i < module.headerCount; i++) {
			WasmHeader header = module.headers[i];

			wasmPushByte(&bytecode, 0x60);
			wasmPushByte(&bytecode, header.paramCount);
			for (int j = 0; j < header.paramCount; j++) {
				wasmPushByte(&bytecode, header.params[j]);
			}

			wasmPushByte(&bytecode, header.returnCount);
			for (int j = 0; j < header.returnCount; j++) {
				wasmPushByte(&bytecode, header.returns[j]);
			}
		}
		*lenAddr = bytecode.len - headerstart;
	}

	if (module.bodyCount) {
		wasmPushByte(&bytecode, 0x03);
		wasmPushByte(&bytecode, module.bodyCount + 1);
		wasmPushByte(&bytecode, module.bodyCount);

		for (int i = 0; i < module.bodyCount; i++) {
			wasmPushByte(&bytecode, module.bodies[i].headerIndex);
		}
	}

	if (module.hasMemory) {
		wasmPushByte(&bytecode, 0x05);
		wasmPushByte(&bytecode, 0x04);
		wasmPushByte(&bytecode, 0x01);
		wasmPushByte(&bytecode, 0x01);
		wasmPushByte(&bytecode, module.memory.pages);
		wasmPushByte(&bytecode, module.memory.maxPages);
	}

	if (module.hasExports) {
		wasmPushByte(&bytecode, 0x07);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int exportStart = bytecode.len;
		u8 *countAddr = wasmReserveByte(&bytecode);

		if (module.hasMemory) {
			wasmPushByte(&bytecode, module.memory.name.len);
			wasmPushBytes(&bytecode, STRTOBUF(module.memory.name));
			wasmPushByte(&bytecode, 0x02);
			wasmPushByte(&bytecode, 0x00);
			(*countAddr)++;
		}

		for (int i = 0; i < module.bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			if (body.name.len == 0) continue;

			wasmPushByte(&bytecode, body.name.len);
			wasmPushBytes(&bytecode, STRTOBUF(body.name));
			wasmPushByte(&bytecode, 0x00);
			wasmPushByte(&bytecode, body.id);
			(*countAddr)++;
		}

		*lenAddr = bytecode.len - exportStart;
	}

	if (module.bodyCount) {
		wasmPushByte(&bytecode, 0x0A);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int bodiesStart = bytecode.len;
		wasmPushByte(&bytecode, module.bodyCount);
		for (int i = 0; i < module.bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			int bodyLen = 1 + /*locals*/ 1 + body.opcodesCount;
			wasmPushByte(&bytecode, bodyLen);
			wasmPushByte(&bytecode, 0 /*locals*/);
			wasmPushBytes(&bytecode, (Buf){body.opcodes, body.opcodesCount});
			wasmPushByte(&bytecode, 0x0B);
		}
		*lenAddr = bytecode.len - bodiesStart;
	}

	return bytecode;
}

int main(int argc, char **argv)
{
	Wasm source = wasmModuleCreate();

	wasmModuleAddMemory(&source, STR("mem"), 1, 2);

	WasmType returns[] = {WasmType_i32};
	u8 opcodes[] = {WasmOp_I32Const, 10, WasmOp_I32Const, 20, WasmOp_I32Add};
	wasmModuleAddFunction(&source, STR("foo"), BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));

	Buf wasm = wasmModuleCompile(source);

	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
}
