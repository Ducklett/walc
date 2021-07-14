#include <binder.c>
#include <wasm.c>

WasmType boundTypeToWasm(WlBType t)
{
	switch (t) {
	case WlBType_i32: return WasmType_I32;
	case WlBType_u32: return WasmType_I32;
	case WlBType_i64: return WasmType_I64;
	case WlBType_u64: return WasmType_I64;
	case WlBType_f32: return WasmType_F32;
	case WlBType_f64: return WasmType_F64;
	case WlBType_u0: return WasmType_Void;
	default: PANIC("Unhandled type %d", t);
	}
}

Buf emitWasm(WlBinder *b)
{
	Wasm source = wasmModuleCreate();

	wasmModuleAddMemory(&source, STR("memory"), 1, 2);

	u8 args[] = {WasmType_I32, WasmType_I32};

	// import print (hardcoded for now)
	int fnPrint = wasmModuleAddImport(&source, STR("print"), BUF(args), BUFEMPTY);

	for (int i = 0; i < b->functionCount; i++) {
		WlBoundFunction fn = b->functions[i];

		Buf args = BUFEMPTY;
		DynamicBuf rets = dynamicBufCreate();
		WasmType returnType = boundTypeToWasm(fn.returnType);
		if (returnType != WasmType_Void) dynamicBufPush(&rets, returnType);

		Buf locals = BUFEMPTY;
		DynamicBuf opcodes = dynamicBufCreate();

		WlbNode bodyNode = fn.body;
		WlBoundBlock body = *(WlBoundBlock *)bodyNode.data;
		for (int j = 0; j < body.nodeCount; j++) {
			WlbNode statementNode = body.nodes[j];
			assert(statementNode.kind == WlBKind_Call);
			WlBoundCallExpression call = *(WlBoundCallExpression *)statementNode.data;
			WlbNode arg = call.arg;
			if (arg.kind == WlBKind_StringLiteral) {
				int offset = wasmModuleAddData(&source, STRTOBUF(arg.dataStr));
				int length = arg.dataStr.len;
				printf("str dims %d %d\n", offset, length);
				wasmPushOpi32Const(&opcodes, offset);
				wasmPushOpi32Const(&opcodes, length);
				wasmPushOpCall(&opcodes, fnPrint);
			} else {
				printf("arg kind %d\n", arg.kind);
				PANIC("Print expects a string");
			}
		}

		wasmModuleAddFunction(&source, fn.exported ? fn.name : STREMPTY, args, dynamicBufToBuf(rets), locals,
							  dynamicBufToBuf(opcodes));
	}

	Buf wasm = wasmModuleCompile(source);
	return wasm;
}
