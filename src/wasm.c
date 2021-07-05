#include "sti.h"

u8 wasmMagic[] = {0x00, 0x61, 0x73, 0x6D};
u8 wasmModule[] = {0x01, 0x00, 0x00, 0x00};

typedef u8 WasmType;
#define WasmType_Void 0x40
#define WasmType_F64  0x7C
#define WasmType_F32  0x7D
#define WasmType_I64  0x7E
#define WasmType_i32  0x7F

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

static inline u8 *wasmReserveByte(DynamicBuf *buf) { return buf->buf + buf->len++; }

Buf wasmModuleCompile(Wasm module)
{
	DynamicBuf bytecode = dynamicBufCreateWithCapacity(0xFFFF);
	dynamicBufAppend(&bytecode, BUF(wasmMagic));
	dynamicBufAppend(&bytecode, BUF(wasmModule));

	if (module.headerCount) {
		dynamicBufPush(&bytecode, 0x01);
		// TODO: encode this as LEB128 u32
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int headerstart = bytecode.len;
		dynamicBufPush(&bytecode, module.headerCount);
		for (int i = 0; i < module.headerCount; i++) {
			WasmHeader header = module.headers[i];

			dynamicBufPush(&bytecode, 0x60);
			dynamicBufPush(&bytecode, header.paramCount);
			for (int j = 0; j < header.paramCount; j++) {
				dynamicBufPush(&bytecode, header.params[j]);
			}

			dynamicBufPush(&bytecode, header.returnCount);
			for (int j = 0; j < header.returnCount; j++) {
				dynamicBufPush(&bytecode, header.returns[j]);
			}
		}
		*lenAddr = bytecode.len - headerstart;
	}

	if (module.bodyCount) {
		dynamicBufPush(&bytecode, 0x03);
		dynamicBufPush(&bytecode, module.bodyCount + 1);
		dynamicBufPush(&bytecode, module.bodyCount);

		for (int i = 0; i < module.bodyCount; i++) {
			dynamicBufPush(&bytecode, module.bodies[i].headerIndex);
		}
	}

	if (module.hasMemory) {
		dynamicBufPush(&bytecode, 0x05);
		dynamicBufPush(&bytecode, 0x04);
		dynamicBufPush(&bytecode, 0x01);
		dynamicBufPush(&bytecode, 0x01);
		dynamicBufPush(&bytecode, module.memory.pages);
		dynamicBufPush(&bytecode, module.memory.maxPages);
	}

	if (module.hasExports) {
		dynamicBufPush(&bytecode, 0x07);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int exportStart = bytecode.len;
		u8 *countAddr = wasmReserveByte(&bytecode);

		if (module.hasMemory) {
			dynamicBufPush(&bytecode, module.memory.name.len);
			dynamicBufAppend(&bytecode, STRTOBUF(module.memory.name));
			dynamicBufPush(&bytecode, 0x02);
			dynamicBufPush(&bytecode, 0x00);
			(*countAddr)++;
		}

		for (int i = 0; i < module.bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			if (body.name.len == 0) continue;

			dynamicBufPush(&bytecode, body.name.len);
			dynamicBufAppend(&bytecode, STRTOBUF(body.name));
			dynamicBufPush(&bytecode, 0x00);
			dynamicBufPush(&bytecode, body.id);
			(*countAddr)++;
		}

		*lenAddr = bytecode.len - exportStart;
	}

	if (module.bodyCount) {
		dynamicBufPush(&bytecode, 0x0A);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int bodiesStart = bytecode.len;
		dynamicBufPush(&bytecode, module.bodyCount);
		for (int i = 0; i < module.bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			int bodyLen = 1 + /*locals*/ 1 + body.opcodesCount;
			dynamicBufPush(&bytecode, bodyLen);
			dynamicBufPush(&bytecode, 0 /*locals*/);
			dynamicBufAppend(&bytecode, (Buf){body.opcodes, body.opcodesCount});
			dynamicBufPush(&bytecode, 0x0B);
		}
		*lenAddr = bytecode.len - bodiesStart;
	}

	return dynamicBufToBuf(bytecode);
}
