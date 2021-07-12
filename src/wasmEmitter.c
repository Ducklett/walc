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

	// wasmModuleAddMemory(&source, STR("mem"), 1, 2);

	for (int i = 0; i < b->unboundCount; i++) {
		WlBoundFunction fn = b->functions[i];

		Buf args = BUFEMPTY;
		DynamicBuf rets = dynamicBufCreate();
		WasmType returnType = boundTypeToWasm(fn.returnType);
		if (returnType != WasmType_Void) dynamicBufPush(&rets, returnType);

		Buf locals = BUFEMPTY;
		DynamicBuf opcodes = dynamicBufCreate();

		// hack, remove this later
		switch (returnType) {
		case WasmType_I32: wasmPushOpi32Const(&opcodes, 43); break;
		case WasmType_I64: wasmPushOpi64Const(&opcodes, 75); break;
		default: break;
		}

		wasmModuleAddFunction(&source, fn.exported ? fn.name : STREMPTY, args, dynamicBufToBuf(rets), locals,
							  dynamicBufToBuf(opcodes));
	}

	Buf wasm = wasmModuleCompile(source);
	return wasm;
}
