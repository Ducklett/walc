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

Wasm source;
int fnPrint;

void emitOperator(WlBOperator op, DynamicBuf *opcodes)
{
	switch (op) {
	case WlBOperator_Add: wasmPushOpi32Add(opcodes); break;
	case WlBOperator_Subtract: wasmPushOpi32Sub(opcodes); break;
	case WlBOperator_Multiply: wasmPushOpi32Mul(opcodes); break;
	case WlBOperator_Divide: wasmPushOpi32DivS(opcodes); break;
	case WlBOperator_Modulo: wasmPushOpi32RemS(opcodes); break;
	default: PANIC("Unhandled operator %d", op); break;
	}
}

void emitExpression(WlbNode expression, DynamicBuf *opcodes)
{
	switch (expression.kind) {
	case WlBKind_BinaryExpression: {
		WlBoundBinaryExpression bin = *(WlBoundBinaryExpression *)expression.data;
		emitExpression(bin.left, opcodes);
		emitExpression(bin.right, opcodes);
		emitOperator(bin.operator, opcodes);

	} break;
	case WlBKind_NumberLiteral: {
		wasmPushOpi32Const(opcodes, expression.dataNum);
	} break;
	default: PANIC("Unhandled expression kind %d", expression.kind); break;
	}
}

void emitStatement(WlbNode statement, DynamicBuf *opcodes)
{
	switch (statement.kind) {
	case WlBKind_Return: {
		WlBoundReturn ret = *(WlBoundReturn *)statement.data;
		if (ret.expression.kind != WlBKind_None) {
			emitExpression(ret.expression, opcodes);
		}
		wasmPushOpBrReturn(opcodes);

	} break;
	case WlBKind_Call: {
		WlBoundCallExpression call = *(WlBoundCallExpression *)statement.data;
		WlbNode arg = call.arg;
		if (arg.kind == WlBKind_StringLiteral) {
			int offset = wasmModuleAddData(&source, STRTOBUF(arg.dataStr));
			int length = arg.dataStr.len;
			printf("str dims %d %d\n", offset, length);
			wasmPushOpi32Const(opcodes, offset);
			wasmPushOpi32Const(opcodes, length);
			wasmPushOpCall(opcodes, fnPrint);
		} else {
			printf("arg kind %d\n", arg.kind);
			PANIC("Print expects a string");
		}
	} break;
	default: PANIC("Unhandled statement kind %d", statement.kind); break;
	}
}

Buf emitWasm(WlBinder *b)
{
	Wasm source = wasmModuleCreate();

	wasmModuleAddMemory(&source, STR("memory"), 1, 2);

	u8 args[] = {WasmType_I32, WasmType_I32};

	// import print (hardcoded for now)
	fnPrint = wasmModuleAddImport(&source, STR("print"), BUF(args), BUFEMPTY);

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
			emitStatement(statementNode, &opcodes);
		}

		wasmModuleAddFunction(&source, fn.exported ? fn.name : STREMPTY, args, dynamicBufToBuf(rets), locals,
							  dynamicBufToBuf(opcodes));
	}

	Buf wasm = wasmModuleCompile(source);
	return wasm;
}
