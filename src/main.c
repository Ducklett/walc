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
	int headerIndex;
	u8 *opcodes;
	int len;
} WasmBody;

typedef struct {
	WasmHeader *headers;
	int headerCount;
	WasmBody *bodies;
	int bodyCount;
} Wasm;

Wasm wasmModuleCreate()
{
	Wasm module = {
		.bodies = malloc(sizeof(WasmBody *) * 0xFF),
		.headers = malloc(sizeof(WasmHeader *) * 0xFF),
		0,
	};

	return module;
}

// adds a new function to the module
// the provided buffers shouldn't be freed until you are done with the {Wasm} object
wasmModuleAddFunction(Wasm *module, Buf args, Buf rets, Buf opcodes)
{
	// TODO: compare headers
	// TODO: use some kind of bump/arena allocator

	module->headers[module->headerCount++] = (WasmHeader){args.len, args.buf, rets.len, rets.buf};
	module->bodies[module->bodyCount++] = (WasmBody){
		.headerIndex = module->headerCount - 1,
		.opcodes = opcodes.buf,
		.len = opcodes.len,
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

	// memory {}
	// exports {}

	if (module.bodyCount) {
		wasmPushByte(&bytecode, 0x0A);
		u8 *lenAddr = wasmReserveByte(&bytecode);
		int bodiesStart = bytecode.len;
		wasmPushByte(&bytecode, module.bodyCount);
		for (int i = 0; i < module.bodyCount; i++) {
			WasmBody body = module.bodies[i];
			int bodyLen = 1 + /*locals*/ 1 + body.len;
			wasmPushByte(&bytecode, bodyLen);
			wasmPushByte(&bytecode, 0 /*locals*/);
			wasmPushBytes(&bytecode, (Buf){body.opcodes, body.len});
			wasmPushByte(&bytecode, 0x0B);
		}
		*lenAddr = bytecode.len - bodiesStart;
	}

	return bytecode;
}

int main(int argc, char **argv)
{
	Wasm source = wasmModuleCreate();

	WasmType returns[] = {WasmType_i32};
	u8 opcodes[] = {WasmOp_I32Const, 10, WasmOp_I32Const, 20, WasmOp_I32Add, WasmOp_I32Const, 2, WasmOp_I32Add};
	wasmModuleAddFunction(&source, BUFEMPTY, BUF(returns), BUF(opcodes));

	Buf wasm = wasmModuleCompile(source);

	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
}
