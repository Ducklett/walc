/*
Lighweight C API for generating wasm bytecode

The Wasm* structs are auxilary data structures that are used to build up the module using their accompanying functions
Once the module is complete the *Compile() functions are used  to translate these structures into raw bytecode
*/

#include "leb128.h"
#include "sti.h"

u8 wasmMagic[] = {0x00, 0x61, 0x73, 0x6D};
u8 wasmModule[] = {0x01, 0x00, 0x00, 0x00};

#define WasmType_Void 0x40
#define WasmType_F64  0x7C
#define WasmType_F32  0x7D
#define WasmType_I64  0x7E
#define WasmType_i32  0x7F

typedef u8 WasmSection;
#define WasmSection_Custom	  0x00
#define WasmSection_Type	  0x01
#define WasmSection_Import	  0x02
#define WasmSection_Function  0x03
#define WasmSection_Table	  0x04
#define WasmSection_Memory	  0x05
#define WasmSection_Global	  0x06
#define WasmSection_Export	  0x07
#define WasmSection_Start	  0x08
#define WasmSection_Element	  0x09
#define WasmSection_Code	  0x0A
#define WasmSection_Data	  0x0B
#define WasmSection_DataCount 0x0C

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
} WasmFuncType;

typedef struct {
	int id;
	int typeIndex;
	Str name;
	WasmType *locals;
	int localsCount;
	WasmOp *opcodes;
	int opcodesCount;
} WasmFunc;

typedef struct {
	int id;
	int typeIndex;
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
	WasmFuncType *types;
	int typeCount;
	WasmFunc *bodies;
	int bodyCount;
	WasmImport *imports;
	int importCount;

	int funcCount;
} Wasm;

Wasm wasmModuleCreate()
{
	// TODO: grow these when they run out of space
	Wasm module = {
		.bodies = malloc(sizeof(WasmFunc *) * 0xFF),
		.types = malloc(sizeof(WasmFuncType *) * 0xFF),
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

static int wasmModuleFindOrCreateFuncType(Wasm *module, Buf args, Buf rets)
{
	for (int i = 0; i < module->typeCount; i++) {
		WasmFuncType t = module->types[i];
		if (bufEqual((Buf){t.params, t.paramCount}, args) && bufEqual((Buf){t.returns, t.returnCount}, rets)) return i;
	}
	module->types[module->typeCount++] = (WasmFuncType){args.len, args.buf, rets.len, rets.buf};
	return module->typeCount - 1;
}

// adds a new function to the module
// the provided buffers shouldn't be freed until you are done with the {Wasm} object
void wasmModuleAddFunction(Wasm *module, Str name, Buf args, Buf rets, Buf locals, Buf opcodes)
{
	if (name.len != 0) module->hasExports = true;

	int id = module->bodyCount;
	int typeIndex = wasmModuleFindOrCreateFuncType(module, args, rets);
	module->bodyCount++;
	module->bodies[id] = (WasmFunc){
		.id = id,
		.typeIndex = module->typeCount - 1,
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

	if (module.typeCount) {
		dynamicBufPush(&bytecode, WasmSection_Type);
		// TODO: encode this as LEB128 u32
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int typestart = bytecode.len;
		dynamicBufPush(&bytecode, module.typeCount);
		for (int i = 0; i < module.typeCount; i++) {
			WasmFuncType functype = module.types[i];

			dynamicBufPush(&bytecode, 0x60);
			dynamicBufPush(&bytecode, functype.paramCount);
			for (int j = 0; j < functype.paramCount; j++) {
				dynamicBufPush(&bytecode, functype.params[j]);
			}

			dynamicBufPush(&bytecode, functype.returnCount);
			for (int j = 0; j < functype.returnCount; j++) {
				dynamicBufPush(&bytecode, functype.returns[j]);
			}
		}
		*lenAddr = bytecode.len - typestart;
	}

	if (module.bodyCount) {
		dynamicBufPush(&bytecode, WasmSection_Function);
		dynamicBufPush(&bytecode, module.bodyCount + 1);
		dynamicBufPush(&bytecode, module.bodyCount);

		for (int i = 0; i < module.bodyCount; i++) {
			dynamicBufPush(&bytecode, module.bodies[i].typeIndex);
		}
	}

	if (module.hasMemory) {
		dynamicBufPush(&bytecode, WasmSection_Memory);
		dynamicBufPush(&bytecode, 0x04);
		dynamicBufPush(&bytecode, 0x01);
		dynamicBufPush(&bytecode, 0x01);
		dynamicBufPush(&bytecode, module.memory.pages);
		dynamicBufPush(&bytecode, module.memory.maxPages);
	}

	if (module.hasExports) {
		dynamicBufPush(&bytecode, WasmSection_Export);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int exportStart = bytecode.len;
		u8 *countAddr = wasmReserveByte(&bytecode);
		*countAddr = 0;

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
		dynamicBufPush(&bytecode, WasmSection_Code);
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
