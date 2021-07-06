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
	WasmMemory memory;
	WasmFuncType *types;
	int typeCount;
	WasmFunc *bodies;
	int bodyCount;
	WasmImport *imports;
	int importCount;
	int exportCount;

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
	if (name.len != 0) module->exportCount++;
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
	if (name.len != 0) module->exportCount++;

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

void wasmAppendSection(DynamicBuf *destination, WasmSection section, Buf sectionContent)
{
	dynamicBufPush(destination, section);
	leb128EncodeU(sectionContent.len, destination, NULL);
	dynamicBufAppend(destination, sectionContent);
}

Buf wasmModuleCompile(Wasm module)
{
	DynamicBuf bytecode = dynamicBufCreateWithCapacity(0xFFFF);
	dynamicBufAppend(&bytecode, BUF(wasmMagic));
	dynamicBufAppend(&bytecode, BUF(wasmModule));

	if (module.typeCount) {
		DynamicBuf typeBuf = dynamicBufCreateWithCapacity(0xFF);
		leb128EncodeU(module.typeCount, &typeBuf, NULL);
		for (int i = 0; i < module.typeCount; i++) {
			WasmFuncType functype = module.types[i];

			dynamicBufPush(&typeBuf, 0x60);
			dynamicBufPush(&typeBuf, functype.paramCount);
			for (int j = 0; j < functype.paramCount; j++) {
				dynamicBufPush(&typeBuf, functype.params[j]);
			}

			dynamicBufPush(&typeBuf, functype.returnCount);
			for (int j = 0; j < functype.returnCount; j++) {
				dynamicBufPush(&typeBuf, functype.returns[j]);
			}
		}

		wasmAppendSection(&bytecode, WasmSection_Type, dynamicBufToBuf(typeBuf));
		dynamicBufFree(&typeBuf);
	}

	if (module.bodyCount) {
		DynamicBuf bodyBuf = dynamicBufCreateWithCapacity(0xFF);
		leb128EncodeU(module.bodyCount, &bodyBuf, NULL);

		for (int i = 0; i < module.bodyCount; i++) {
			leb128EncodeU(module.bodies[i].typeIndex, &bodyBuf, NULL);
		}

		wasmAppendSection(&bytecode, WasmSection_Function, dynamicBufToBuf(bodyBuf));
		dynamicBufFree(&bodyBuf);
	}

	if (module.hasMemory) {
		DynamicBuf memBuf = dynamicBufCreateWithCapacity(0xF);

		const int memCount = 1;
		dynamicBufPush(&memBuf, memCount);
		dynamicBufPush(&memBuf, module.memory.maxPages ? 1 : 0);
		dynamicBufPush(&memBuf, module.memory.pages);
		if (module.memory.maxPages) dynamicBufPush(&memBuf, module.memory.maxPages);

		wasmAppendSection(&bytecode, WasmSection_Memory, dynamicBufToBuf(memBuf));
		dynamicBufFree(&memBuf);
	}

	if (module.exportCount) {
		DynamicBuf exportBuf = dynamicBufCreateWithCapacity(0xFF);

		leb128EncodeU(module.exportCount, &exportBuf, NULL);

		if (module.hasMemory) {
			dynamicBufPush(&exportBuf, module.memory.name.len);
			dynamicBufAppend(&exportBuf, STRTOBUF(module.memory.name));
			dynamicBufPush(&exportBuf, 0x02);
			dynamicBufPush(&exportBuf, 0x00);
		}

		for (int i = 0; i < module.bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			if (body.name.len == 0) continue;

			dynamicBufPush(&exportBuf, body.name.len);
			dynamicBufAppend(&exportBuf, STRTOBUF(body.name));
			dynamicBufPush(&exportBuf, 0x00);
			dynamicBufPush(&exportBuf, body.id);
		}

		wasmAppendSection(&bytecode, WasmSection_Export, dynamicBufToBuf(exportBuf));
		dynamicBufFree(&exportBuf);
	}

	if (module.bodyCount) {
		DynamicBuf bodyBuf = dynamicBufCreateWithCapacity(0xFF);

		leb128EncodeU(module.bodyCount, &bodyBuf, NULL);

		for (int i = 0; i < module.bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			int bodyLen = 1 + /*locals*/ 1 + body.opcodesCount;
			leb128EncodeU(bodyLen, &bodyBuf, NULL);
			// TODO: support locals
			dynamicBufPush(&bodyBuf, 0 /*locals*/);
			dynamicBufAppend(&bodyBuf, (Buf){body.opcodes, body.opcodesCount});
			dynamicBufPush(&bodyBuf, 0x0B);
		}

		wasmAppendSection(&bytecode, WasmSection_Code, dynamicBufToBuf(bodyBuf));
		dynamicBufFree(&bodyBuf);
	}

	return dynamicBufToBuf(bytecode);
}
