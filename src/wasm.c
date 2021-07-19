/*
Lighweight C API for generating wasm bytecode

The Wasm* structs are auxilary data structures that are used to build up the module using their accompanying functions
Once the module is complete the *Compile() functions are used  to translate these structures into raw bytecode
*/

#include <leb128.h>
#include <sti_base.h>

u8 wasmMagic[] = {0x00, 0x61, 0x73, 0x6D};
u8 wasmModule[] = {0x01, 0x00, 0x00, 0x00};

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
#define WasmType_I32  0x7F

typedef u8 WasmOp;
#define WasmOp_I32Const 0x41
#define WasmOp_I32Add	0x6A

typedef u32 WasmTypeIdx;
typedef u32 WasmFuncIdx;
typedef u32 WasmTableIdx;
typedef u32 WasmMemIdx;
typedef u32 WasmGlobalIdx;
typedef u32 WasmElemIdx;
typedef u32 WasmDataIdx;
typedef u32 WasmLocalIdx;
typedef u32 WasmLabelIdx;

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
	Str name;
} WasmImport;

typedef struct {
	Str name;
	int pages;
	int maxPages;
} WasmMemory;

typedef struct {
	int offset;
	Buf data;
} WasmData;

typedef struct {
	bool hasMemory;
	WasmMemory memory;
	List(WasmFuncType) types;
	List(WasmData) data;
	List(WasmFunc) bodies;
	List(WasmImport) imports;

	int dataOffset;
	int exportCount;
	int funcCount;
} Wasm;

Wasm wasmModuleCreate()
{
	Wasm module = {
		.imports = listNew(),
		.bodies = listNew(),
		.types = listNew(),
		.data = listNew(),
		0,
	};

	return module;
}

void wasmModuleFree(Wasm *module)
{
	listFree(&module->imports);
	listFree(&module->bodies);
	listFree(&module->types);
	listFree(&module->data);
	listFree(&module->imports);
	*module = (Wasm){0};
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

// adds data to the module and returns its offset
// you must also add memory to the wasm module if data is set
int wasmModuleAddData(Wasm *module, Buf data)
{
	int offset = module->dataOffset;
	WasmData d = {
		.offset = offset,
		.data = data,
	};
	listPush(&module->data, d);
	module->dataOffset += data.len;

	return offset;
}

static int wasmModuleFindOrCreateFuncType(Wasm *module, Buf args, Buf rets)
{
	int typeCount = listLen(module->types);
	for (int i = 0; i < typeCount; i++) {
		WasmFuncType t = module->types[i];
		if (bufEqual((Buf){t.params, t.paramCount}, args) && bufEqual((Buf){t.returns, t.returnCount}, rets)) return i;
	}
	WasmFuncType t = {args.len, args.buf, rets.len, rets.buf};
	listPush(&module->types, t);
	return typeCount;
}

// adds a new import to the module
// the provided buffers shouldn't be freed until you are done with the {Wasm} object
// returns the function id
int wasmModuleAddImport(Wasm *module, Str name, Buf args, Buf rets)
{
	int id = module->funcCount++;
	int typeIndex = wasmModuleFindOrCreateFuncType(module, args, rets);
	WasmImport im = {
		.id = id,
		.typeIndex = typeIndex,
		.name = name,
	};
	listPush(&module->imports, im);
	return id;
}

// adds a new function to the module
// the provided buffers shouldn't be freed until you are done with the {Wasm} object
int wasmModuleAddFunction(Wasm *module, Str name, Buf args, Buf rets, Buf locals, Buf opcodes)
{
	if (name.len != 0) module->exportCount++;

	int id = module->funcCount++;
	int typeIndex = wasmModuleFindOrCreateFuncType(module, args, rets);
	WasmFunc fun = {
		.id = id,
		.typeIndex = typeIndex,
		.name = name,
		.locals = locals.buf,
		.localsCount = locals.len,
		.opcodes = opcodes.buf,
		.opcodesCount = opcodes.len,
	};
	listPush(&module->bodies, fun);
	return id;
}

static inline u8 *wasmReserveByte(DynamicBuf *buf) { return buf->buf + buf->len++; }

void wasmAppendSection(DynamicBuf *destination, WasmSection section, Buf sectionContent)
{
	dynamicBufPush(destination, section);
	leb128EncodeU(sectionContent.len, destination);
	dynamicBufAppend(destination, sectionContent);
}

void wasmPushOpi32Const(DynamicBuf *body, i32 value);
void wasmPushOpEnd(DynamicBuf *body);

Buf wasmModuleCompile(Wasm module)
{
	DynamicBuf bytecode = dynamicBufCreateWithCapacity(0xFFFF);
	dynamicBufAppend(&bytecode, BUF(wasmMagic));
	dynamicBufAppend(&bytecode, BUF(wasmModule));

	int typeCount = listLen(module.types);
	if (typeCount) {
		DynamicBuf typeBuf = dynamicBufCreateWithCapacity(0xFF);
		leb128EncodeU(typeCount, &typeBuf);
		for (int i = 0; i < typeCount; i++) {
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

	int importCount = listLen(module.imports);
	if (importCount) {

		Str namespace = STR("env");

		DynamicBuf importBuf = dynamicBufCreateWithCapacity(0xFF);
		leb128EncodeU(importCount, &importBuf);

		for (int i = 0; i < importCount; i++) {

			leb128EncodeU(namespace.len, &importBuf);
			dynamicBufAppend(&importBuf, STRTOBUF(namespace));

			leb128EncodeU(module.imports[i].name.len, &importBuf);
			dynamicBufAppend(&importBuf, STRTOBUF(module.imports[i].name));

			// TODO: also support importing of memory, tables, globals..
			dynamicBufPush(&importBuf, 0x00); // adds function
			leb128EncodeU(module.imports[i].typeIndex, &importBuf);
		}

		wasmAppendSection(&bytecode, WasmSection_Import, dynamicBufToBuf(importBuf));
		dynamicBufFree(&importBuf);
	}

	int bodyCount = listLen(module.bodies);
	if (bodyCount) {
		DynamicBuf bodyBuf = dynamicBufCreateWithCapacity(0xFF);
		leb128EncodeU(bodyCount, &bodyBuf);

		for (int i = 0; i < bodyCount; i++) {
			leb128EncodeU(module.bodies[i].typeIndex, &bodyBuf);
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

		leb128EncodeU(module.exportCount, &exportBuf);

		if (module.hasMemory) {
			dynamicBufPush(&exportBuf, module.memory.name.len);
			dynamicBufAppend(&exportBuf, STRTOBUF(module.memory.name));
			dynamicBufPush(&exportBuf, 0x02);
			dynamicBufPush(&exportBuf, 0x00);
		}

		for (int i = 0; i < bodyCount; i++) {
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

	if (bodyCount) {
		DynamicBuf bodyBuf = dynamicBufCreateWithCapacity(0xFF);

		leb128EncodeU(bodyCount, &bodyBuf);

		for (int i = 0; i < bodyCount; i++) {
			WasmFunc body = module.bodies[i];
			int bodyLen = 1 + /*locals*/ 1 + body.opcodesCount;
			leb128EncodeU(bodyLen, &bodyBuf);
			// TODO: support locals
			dynamicBufPush(&bodyBuf, 0 /*locals*/);
			dynamicBufAppend(&bodyBuf, (Buf){body.opcodes, body.opcodesCount});
			dynamicBufPush(&bodyBuf, 0x0B);
		}

		wasmAppendSection(&bytecode, WasmSection_Code, dynamicBufToBuf(bodyBuf));
		dynamicBufFree(&bodyBuf);
	}

	int dataCount = listLen(module.data);
	if (dataCount) {
		DynamicBuf dataBuf = dynamicBufCreateWithCapacity(0xFF);

		leb128EncodeU(dataCount, &dataBuf);

		for (int i = 0; i < dataCount; i++) {
			WasmData data = module.data[i];
			dynamicBufPush(&dataBuf, 0 /*mode active memory 0*/);
			wasmPushOpi32Const(&dataBuf, data.offset);
			wasmPushOpEnd(&dataBuf);

			leb128EncodeU(data.data.len, &dataBuf);
			dynamicBufAppend(&dataBuf, data.data);
		}

		wasmAppendSection(&bytecode, WasmSection_Data, dynamicBufToBuf(dataBuf));
		dynamicBufFree(&dataBuf);
	}

	return dynamicBufToBuf(bytecode);
}

// The unreachable instruction causes an unconditional trap.
void wasmPushOpUnreachable(DynamicBuf *body) { dynamicBufPush(body, 0x00); }
// The nop instruction does nothing
void wasmPushOpNop(DynamicBuf *body) { dynamicBufPush(body, 0x01); }
// The block, loop and if instructions are structured instructions.
// They bracket nested sequences of instructions, called blocks, terminated with, or separated by, end or else
// pseudo-instructions. As the grammar prescribes, they must be well-nested.
void wasmPushOpBlock(DynamicBuf *body, u32 blocktype)
{
	dynamicBufPush(body, 0x02);
	if (blocktype) leb128EncodeU(blocktype, body);
}
void wasmPushOpLoop(DynamicBuf *body, u32 blocktype)
{
	dynamicBufPush(body, 0x03);
	if (blocktype) leb128EncodeU(blocktype, body);
}
void wasmPushOpIf(DynamicBuf *body, u32 blocktype)
{
	dynamicBufPush(body, 0x04);
	leb128EncodeU(blocktype, body);
}
void wasmPushOpElse(DynamicBuf *body) { dynamicBufPush(body, 0x05); }
void wasmPushOpEnd(DynamicBuf *body) { dynamicBufPush(body, 0x0B); }
// br performs an unconditional branch
void wasmPushOpBr(DynamicBuf *body, WasmLabelIdx l)
{
	dynamicBufPush(body, 0x0C);
	leb128EncodeU(l, body);
}
// br_if performs a conditional branch
void wasmPushOpBrIf(DynamicBuf *body, WasmLabelIdx l)
{
	dynamicBufPush(body, 0x0D);
	leb128EncodeU(l, body);
}
// br_table performs an indirect branch through an operand indexing into the label vector that is an immediate to the
// instruction, or to a default target if the operand is out of bounds
void wasmPushOpBrTable(DynamicBuf *body) { dynamicBufPush(body, 0x0E); }
// The return instruction is a shortcut for an unconditional branch to the outermost block, which implicitly is the body
// of the current function
void wasmPushOpBrReturn(DynamicBuf *body) { dynamicBufPush(body, 0x0F); }
// The call instruction invokes another function, consuming the necessary arguments from the stack and returning the
// result values of the call
void wasmPushOpCall(DynamicBuf *body, WasmFuncIdx x)
{
	dynamicBufPush(body, 0x10);
	leb128EncodeU(x, body);
}
// The call_indirect instruction calls a function indirectly through an operand indexing into a table that is denoted by
// a table index and must have type funcref
void wasmPushOpCallIndirect(DynamicBuf *body, WasmTableIdx x, WasmTypeIdx y)
{
	dynamicBufPush(body, 0x10);
	leb128EncodeU(x, body);
	leb128EncodeU(y, body);
}
// The drop instruction simply throws away a single operand
void wasmPushOpDrop(DynamicBuf *body) { dynamicBufPush(body, 0x1A); }
// The select instruction selects one of its first two operands based on whether its third operand is zero or not.
// It may include a value type determining the type of these operands.
// If missing, the operands must be of numeric type.
void wasmPushOpSelect(DynamicBuf *body) { dynamicBufPush(body, 0x1B); }
void wasmPushOpSelectT(DynamicBuf *body, WasmTypeIdx t)
{
	dynamicBufPush(body, 0x1B);
	leb128EncodeU(t, body);
}

void wasmPushOpLocalGet(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x20);
	leb128EncodeU(x, body);
}
void wasmPushOpLocalSet(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x21);
	leb128EncodeU(x, body);
}
void wasmPushOpLocalTee(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x22);
	leb128EncodeU(x, body);
}
void wasmPushOpGlobalGet(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x23);
	leb128EncodeU(x, body);
}
void wasmPushOpGlobalSet(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x24);
	leb128EncodeU(x, body);
}
void wasmPushOpTableGet(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x25);
	leb128EncodeU(x, body);
}
void wasmPushOpTableSet(DynamicBuf *body, WasmLocalIdx x)
{
	dynamicBufPush(body, 0x26);
	leb128EncodeU(x, body);
}

void wasmPushOpi32Load(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x28);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x29);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpf32Load(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x2A);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpf64Load(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x2B);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi32Load8s(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x2C);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi32Load8u(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x2D);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi32Load16s(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x2E);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi32Load16u(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x2F);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load8s(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x30);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load8u(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x31);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load16s(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x32);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load16u(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x33);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load32s(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x34);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Load32u(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x35);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}

void wasmPushOpi32Store(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x36);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Store(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x37);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpf32Store(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x38);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpf64Store(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x39);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi32Store8(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x3A);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi32Store16(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x3B);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Store8(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x3C);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Store16(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x3D);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}
void wasmPushOpi64Store32(DynamicBuf *body, i32 offset, i32 align)
{
	dynamicBufPush(body, 0x3E);
	leb128EncodeU(offset, body);
	leb128EncodeU(align, body);
}

void wasmPushOpMemorySize(DynamicBuf *body) { dynamicBufPush(body, 0x3F); }
void wasmPushOpMemoryGrow(DynamicBuf *body) { dynamicBufPush(body, 0x40); }

void wasmPushOpi32Const(DynamicBuf *body, i32 value)
{
	dynamicBufPush(body, 0x41);
	leb128EncodeU(value, body);
}
void wasmPushOpi64Const(DynamicBuf *body, i64 value)
{
	dynamicBufPush(body, 0x42);
	leb128EncodeU(value, body);
}
void wasmPushOpf32Const(DynamicBuf *body, f32 value)
{
	dynamicBufPush(body, 0x43);

	u8 *valueP = (u8 *)&value;
	dynamicBufAppend(body, (Buf){valueP, 4});
}
void wasmPushOpf64Const(DynamicBuf *body, f64 value)
{
	dynamicBufPush(body, 0x44);
	u8 *valueP = (u8 *)&value;
	dynamicBufAppend(body, (Buf){valueP, 8});
}

void wasmPushOpi32Eqz(DynamicBuf *body) { dynamicBufPush(body, 0x45); }
void wasmPushOpi32Eq(DynamicBuf *body) { dynamicBufPush(body, 0x46); }
void wasmPushOpi32Ne(DynamicBuf *body) { dynamicBufPush(body, 0x47); }
void wasmPushOpi32LtS(DynamicBuf *body) { dynamicBufPush(body, 0x48); }
void wasmPushOpi32LtU(DynamicBuf *body) { dynamicBufPush(body, 0x49); }
void wasmPushOpi32GtS(DynamicBuf *body) { dynamicBufPush(body, 0x4A); }
void wasmPushOpi32GtU(DynamicBuf *body) { dynamicBufPush(body, 0x4B); }
void wasmPushOpi32LeS(DynamicBuf *body) { dynamicBufPush(body, 0x4C); }
void wasmPushOpi32LeU(DynamicBuf *body) { dynamicBufPush(body, 0x4D); }
void wasmPushOpi32GeS(DynamicBuf *body) { dynamicBufPush(body, 0x4E); }
void wasmPushOpi32GeU(DynamicBuf *body) { dynamicBufPush(body, 0x4F); }

void wasmPushOpi64Eqz(DynamicBuf *body) { dynamicBufPush(body, 0x50); }
void wasmPushOpi64Eq(DynamicBuf *body) { dynamicBufPush(body, 0x51); }
void wasmPushOpi64Ne(DynamicBuf *body) { dynamicBufPush(body, 0x52); }
void wasmPushOpi64LtS(DynamicBuf *body) { dynamicBufPush(body, 0x53); }
void wasmPushOpi64LtU(DynamicBuf *body) { dynamicBufPush(body, 0x54); }
void wasmPushOpi64GtS(DynamicBuf *body) { dynamicBufPush(body, 0x55); }
void wasmPushOpi64GtU(DynamicBuf *body) { dynamicBufPush(body, 0x56); }
void wasmPushOpi64LeS(DynamicBuf *body) { dynamicBufPush(body, 0x57); }
void wasmPushOpi64LeU(DynamicBuf *body) { dynamicBufPush(body, 0x58); }
void wasmPushOpi64GeS(DynamicBuf *body) { dynamicBufPush(body, 0x59); }
void wasmPushOpi64GeU(DynamicBuf *body) { dynamicBufPush(body, 0x5A); }

void wasmPushOpf32Eq(DynamicBuf *body) { dynamicBufPush(body, 0x5B); }
void wasmPushOpf32Ne(DynamicBuf *body) { dynamicBufPush(body, 0x5C); }
void wasmPushOpf32Lt(DynamicBuf *body) { dynamicBufPush(body, 0x5D); }
void wasmPushOpf32Gt(DynamicBuf *body) { dynamicBufPush(body, 0x5E); }
void wasmPushOpf32Le(DynamicBuf *body) { dynamicBufPush(body, 0x5F); }
void wasmPushOpf32Ge(DynamicBuf *body) { dynamicBufPush(body, 0x60); }

void wasmPushOpf64Eq(DynamicBuf *body) { dynamicBufPush(body, 0x61); }
void wasmPushOpf64Ne(DynamicBuf *body) { dynamicBufPush(body, 0x62); }
void wasmPushOpf64Lt(DynamicBuf *body) { dynamicBufPush(body, 0x63); }
void wasmPushOpf64Gt(DynamicBuf *body) { dynamicBufPush(body, 0x64); }
void wasmPushOpf64Le(DynamicBuf *body) { dynamicBufPush(body, 0x65); }
void wasmPushOpf64Ge(DynamicBuf *body) { dynamicBufPush(body, 0x66); }

void wasmPushOpi32Clz(DynamicBuf *body) { dynamicBufPush(body, 0x67); }
void wasmPushOpi32Ctz(DynamicBuf *body) { dynamicBufPush(body, 0x68); }
void wasmPushOpi32Popcnt(DynamicBuf *body) { dynamicBufPush(body, 0x69); }
void wasmPushOpi32Add(DynamicBuf *body) { dynamicBufPush(body, 0x6A); }
void wasmPushOpi32Sub(DynamicBuf *body) { dynamicBufPush(body, 0x6B); }
void wasmPushOpi32Mul(DynamicBuf *body) { dynamicBufPush(body, 0x6C); }
void wasmPushOpi32DivS(DynamicBuf *body) { dynamicBufPush(body, 0x6D); }
void wasmPushOpi32DivU(DynamicBuf *body) { dynamicBufPush(body, 0x6E); }
void wasmPushOpi32RemS(DynamicBuf *body) { dynamicBufPush(body, 0x6F); }
void wasmPushOpi32RemU(DynamicBuf *body) { dynamicBufPush(body, 0x70); }
void wasmPushOpi32And(DynamicBuf *body) { dynamicBufPush(body, 0x71); }
void wasmPushOpi32Or(DynamicBuf *body) { dynamicBufPush(body, 0x72); }
void wasmPushOpi32Xor(DynamicBuf *body) { dynamicBufPush(body, 0x73); }
void wasmPushOpi32Shl(DynamicBuf *body) { dynamicBufPush(body, 0x74); }
void wasmPushOpi32ShrS(DynamicBuf *body) { dynamicBufPush(body, 0x75); }
void wasmPushOpi32ShrU(DynamicBuf *body) { dynamicBufPush(body, 0x76); }
void wasmPushOpi32Rotl(DynamicBuf *body) { dynamicBufPush(body, 0x77); }
void wasmPushOpi32Rotr(DynamicBuf *body) { dynamicBufPush(body, 0x78); }

void wasmPushOpi64Clz(DynamicBuf *body) { dynamicBufPush(body, 0x79); }
void wasmPushOpi64Ctz(DynamicBuf *body) { dynamicBufPush(body, 0x7A); }
void wasmPushOpi64Popcnt(DynamicBuf *body) { dynamicBufPush(body, 0x7B); }
void wasmPushOpi64Add(DynamicBuf *body) { dynamicBufPush(body, 0x7C); }
void wasmPushOpi64Sub(DynamicBuf *body) { dynamicBufPush(body, 0x7D); }
void wasmPushOpi64Mul(DynamicBuf *body) { dynamicBufPush(body, 0x7E); }
void wasmPushOpi64DivS(DynamicBuf *body) { dynamicBufPush(body, 0x7F); }
void wasmPushOpi64DivU(DynamicBuf *body) { dynamicBufPush(body, 0x80); }
void wasmPushOpi64RemS(DynamicBuf *body) { dynamicBufPush(body, 0x81); }
void wasmPushOpi64RemU(DynamicBuf *body) { dynamicBufPush(body, 0x82); }
void wasmPushOpi64And(DynamicBuf *body) { dynamicBufPush(body, 0x83); }
void wasmPushOpi64Or(DynamicBuf *body) { dynamicBufPush(body, 0x84); }
void wasmPushOpi64Xor(DynamicBuf *body) { dynamicBufPush(body, 0x85); }
void wasmPushOpi64Shl(DynamicBuf *body) { dynamicBufPush(body, 0x86); }
void wasmPushOpi64ShrS(DynamicBuf *body) { dynamicBufPush(body, 0x87); }
void wasmPushOpi64ShrU(DynamicBuf *body) { dynamicBufPush(body, 0x88); }
void wasmPushOpi64Rotl(DynamicBuf *body) { dynamicBufPush(body, 0x89); }
void wasmPushOpi64Rotr(DynamicBuf *body) { dynamicBufPush(body, 0x8A); }

void wasmPushOpf32Abs(DynamicBuf *body) { dynamicBufPush(body, 0x8B); }
void wasmPushOpf32Neg(DynamicBuf *body) { dynamicBufPush(body, 0x8C); }
void wasmPushOpf32Ceil(DynamicBuf *body) { dynamicBufPush(body, 0x8D); }
void wasmPushOpf32Floor(DynamicBuf *body) { dynamicBufPush(body, 0x8E); }
void wasmPushOpf32Trunc(DynamicBuf *body) { dynamicBufPush(body, 0x8F); }
void wasmPushOpf32Nearest(DynamicBuf *body) { dynamicBufPush(body, 0x90); }
void wasmPushOpf32Sqrt(DynamicBuf *body) { dynamicBufPush(body, 0x91); }
void wasmPushOpf32Add(DynamicBuf *body) { dynamicBufPush(body, 0x92); }
void wasmPushOpf32Sub(DynamicBuf *body) { dynamicBufPush(body, 0x93); }
void wasmPushOpf32Mul(DynamicBuf *body) { dynamicBufPush(body, 0x94); }
void wasmPushOpf32Div(DynamicBuf *body) { dynamicBufPush(body, 0x95); }
void wasmPushOpf32Min(DynamicBuf *body) { dynamicBufPush(body, 0x96); }
void wasmPushOpf32Max(DynamicBuf *body) { dynamicBufPush(body, 0x97); }
void wasmPushOpf32Copysign(DynamicBuf *body) { dynamicBufPush(body, 0x98); }

void wasmPushOpf64Abs(DynamicBuf *body) { dynamicBufPush(body, 0x99); }
void wasmPushOpf64Neg(DynamicBuf *body) { dynamicBufPush(body, 0x9A); }
void wasmPushOpf64Ceil(DynamicBuf *body) { dynamicBufPush(body, 0x9B); }
void wasmPushOpf64Floor(DynamicBuf *body) { dynamicBufPush(body, 0x9C); }
void wasmPushOpf64Trunc(DynamicBuf *body) { dynamicBufPush(body, 0x9D); }
void wasmPushOpf64Nearest(DynamicBuf *body) { dynamicBufPush(body, 0x9E); }
void wasmPushOpf64Sqrt(DynamicBuf *body) { dynamicBufPush(body, 0x9F); }
void wasmPushOpf64Add(DynamicBuf *body) { dynamicBufPush(body, 0xA0); }
void wasmPushOpf64Sub(DynamicBuf *body) { dynamicBufPush(body, 0xA1); }
void wasmPushOpf64Mul(DynamicBuf *body) { dynamicBufPush(body, 0xA2); }
void wasmPushOpf64Div(DynamicBuf *body) { dynamicBufPush(body, 0xA3); }
void wasmPushOpf64Min(DynamicBuf *body) { dynamicBufPush(body, 0xA4); }
void wasmPushOpf64Max(DynamicBuf *body) { dynamicBufPush(body, 0xA5); }
void wasmPushOpf64Copysign(DynamicBuf *body) { dynamicBufPush(body, 0xA6); }

void wasmPushOpi32WrapI64(DynamicBuf *body) { dynamicBufPush(body, 0xA7); }
void wasmPushOpi32TruncF32S(DynamicBuf *body) { dynamicBufPush(body, 0xA8); }
void wasmPushOpi32TruncF32U(DynamicBuf *body) { dynamicBufPush(body, 0xA9); }
void wasmPushOpi32TrunsF64S(DynamicBuf *body) { dynamicBufPush(body, 0xAA); }
void wasmPushOpi32TruncF64U(DynamicBuf *body) { dynamicBufPush(body, 0xAB); }

void wasmPushOpi64ExtendI32S(DynamicBuf *body) { dynamicBufPush(body, 0xAC); }
void wasmPushOpi64ExtendI32U(DynamicBuf *body) { dynamicBufPush(body, 0xAC); }
void wasmPushOpi64TruncF32S(DynamicBuf *body) { dynamicBufPush(body, 0xAE); }
void wasmPushOpi64TruncF32U(DynamicBuf *body) { dynamicBufPush(body, 0xAF); }
void wasmPushOpi64TruncF64S(DynamicBuf *body) { dynamicBufPush(body, 0xB0); }
void wasmPushOpi64TruncF64U(DynamicBuf *body) { dynamicBufPush(body, 0xB1); }

void wasmPushOpf32ConvertI32S(DynamicBuf *body) { dynamicBufPush(body, 0xB2); }
void wasmPushOpf32ConvertI32U(DynamicBuf *body) { dynamicBufPush(body, 0xB3); }
void wasmPushOpf32ConvertI64S(DynamicBuf *body) { dynamicBufPush(body, 0xB4); }
void wasmPushOpf32ConvertI64U(DynamicBuf *body) { dynamicBufPush(body, 0xB5); }
void wasmPushOpf32DemoteF64(DynamicBuf *body) { dynamicBufPush(body, 0xB6); }

void wasmPushOpf64ConvertI32S(DynamicBuf *body) { dynamicBufPush(body, 0xB7); }
void wasmPushOpf64ConvertI32U(DynamicBuf *body) { dynamicBufPush(body, 0xB8); }
void wasmPushOpf64ConvertI64S(DynamicBuf *body) { dynamicBufPush(body, 0xB9); }
void wasmPushOpf64ConvertI64U(DynamicBuf *body) { dynamicBufPush(body, 0xBA); }
void wasmPushOpf64PromoteF32(DynamicBuf *body) { dynamicBufPush(body, 0xBB); }

void wasmPushOpi32ReinterpretF32(DynamicBuf *body) { dynamicBufPush(body, 0xBC); }
void wasmPushOpi64ReinterpretF64(DynamicBuf *body) { dynamicBufPush(body, 0xBD); }
void wasmPushOpf32ReinterpretI32(DynamicBuf *body) { dynamicBufPush(body, 0xBE); }
void wasmPushOpf64ReinterpretI64(DynamicBuf *body) { dynamicBufPush(body, 0xBF); }

void wasmPushOpfi32Extend8S(DynamicBuf *body) { dynamicBufPush(body, 0xC1); }
void wasmPushOpfi32Extend16S(DynamicBuf *body) { dynamicBufPush(body, 0xC2); }
void wasmPushOpfi64Extend8S(DynamicBuf *body) { dynamicBufPush(body, 0xC3); }
void wasmPushOpfi64Extend16S(DynamicBuf *body) { dynamicBufPush(body, 0xC4); }
void wasmPushOpfi64Extend32S(DynamicBuf *body) { dynamicBufPush(body, 0xC5); }

void wasmPushOpRefNull(DynamicBuf *body, i32 t)
{
	dynamicBufPush(body, 0xD0);
	leb128EncodeU(t, body);
}
void wasmPushOpRefIsNull(DynamicBuf *body) { dynamicBufPush(body, 0xD1); }
void wasmPushOpFunc(DynamicBuf *body, WasmFuncIdx x)
{
	dynamicBufPush(body, 0xD2);
	leb128EncodeU(x, body);
}

void wasmPushOpi32TruncSatF32S(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x00);
}

void wasmPushOpi32TruncSatF32U(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x01);
}

void wasmPushOpi32TruncSatF64S(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x02);
}

void wasmPushOpi32TruncSatF64U(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x03);
}

void wasmPushOpi64TruncSatF32S(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x04);
}

void wasmPushOpi64TruncSatF32U(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x05);
}

void wasmPushOpi64TruncSatF64S(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x06);
}

void wasmPushOpi64TruncSatF64U(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x07);
}

void wasmPushOpMemoryInit(DynamicBuf *body, WasmDataIdx x)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x08);
	leb128EncodeU(x, body);
}

void wasmPushOpDataDrop(DynamicBuf *body, WasmDataIdx x)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x09);
	leb128EncodeU(x, body);
}

void wasmPushOpMemoryCopy(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x0A);
}

void wasmPushOpMemoryFill(DynamicBuf *body)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x0B);
}

void wasmPushOpTableInit(DynamicBuf *body, WasmTableIdx x, WasmElemIdx y)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x0C);
	leb128EncodeU(x, body);
	leb128EncodeU(y, body);
}

void wasmPushOpElemDrop(DynamicBuf *body, WasmElemIdx x)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x0D);
	leb128EncodeU(x, body);
}

void wasmPushOpTableCopy(DynamicBuf *body, WasmTableIdx x, WasmTableIdx y)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x0E);
	leb128EncodeU(x, body);
	leb128EncodeU(y, body);
}

void wasmPushOpTableGrow(DynamicBuf *body, WasmTableIdx x)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x0F);
	leb128EncodeU(x, body);
}

void wasmPushOpTableSize(DynamicBuf *body, WasmTableIdx x)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x10);
	leb128EncodeU(x, body);
}

void wasmPushOpTableFill(DynamicBuf *body, WasmTableIdx x)
{
	dynamicBufPush(body, 0xFC);
	dynamicBufPush(body, 0x11);
	leb128EncodeU(x, body);
}
