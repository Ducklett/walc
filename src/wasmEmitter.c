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

void emitOperator(WlBType type, WlBOperator op, DynamicBuf *opcodes)
{
	switch (op) {
	case WlBOperator_Add:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Add(opcodes); break;
		case WlBType_u32: wasmPushOpi32Add(opcodes); break;
		case WlBType_i64: wasmPushOpi64Add(opcodes); break;
		case WlBType_u64: wasmPushOpi64Add(opcodes); break;
		case WlBType_f32: wasmPushOpf32Add(opcodes); break;
		case WlBType_f64: wasmPushOpf64Add(opcodes); break;
		}
		break;
	case WlBOperator_Subtract:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Sub(opcodes); break;
		case WlBType_u32: wasmPushOpi32Sub(opcodes); break;
		case WlBType_i64: wasmPushOpi64Sub(opcodes); break;
		case WlBType_u64: wasmPushOpi64Sub(opcodes); break;
		case WlBType_f32: wasmPushOpf32Sub(opcodes); break;
		case WlBType_f64: wasmPushOpf64Sub(opcodes); break;
		}
		break;
	case WlBOperator_Multiply:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Mul(opcodes); break;
		case WlBType_u32: wasmPushOpi32Mul(opcodes); break;
		case WlBType_i64: wasmPushOpi64Mul(opcodes); break;
		case WlBType_u64: wasmPushOpi64Mul(opcodes); break;
		case WlBType_f32: wasmPushOpf32Mul(opcodes); break;
		case WlBType_f64: wasmPushOpf64Mul(opcodes); break;
		}
		break;
	case WlBOperator_Divide:
		switch (type) {
		case WlBType_i32: wasmPushOpi32DivS(opcodes); break;
		case WlBType_u32: wasmPushOpi32DivU(opcodes); break;
		case WlBType_i64: wasmPushOpi64DivS(opcodes); break;
		case WlBType_u64: wasmPushOpi64DivU(opcodes); break;
		case WlBType_f32: wasmPushOpf32Div(opcodes); break;
		case WlBType_f64: wasmPushOpf64Div(opcodes); break;
		}
		break;
	case WlBOperator_Modulo:
		switch (type) {
		case WlBType_i32: wasmPushOpi32RemS(opcodes); break;
		case WlBType_u32: wasmPushOpi32RemU(opcodes); break;
		case WlBType_i64: wasmPushOpi64RemS(opcodes); break;
		case WlBType_u64: wasmPushOpi64RemU(opcodes); break;
		case WlBType_f32: TODO("Support modulo on float32"); break;
		case WlBType_f64: TODO("Support modulo on float64"); break;
		}
		break;
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
		emitOperator(expression.type, bin.operator, opcodes);

	} break;
	case WlBKind_NumberLiteral: {
		switch (expression.type) {
		case WlBType_i32: wasmPushOpi32Const(opcodes, expression.dataNum); break;
		case WlBType_u32: wasmPushOpi32Const(opcodes, expression.dataNum); break;
		case WlBType_i64: wasmPushOpi64Const(opcodes, expression.dataNum); break;
		case WlBType_u64: wasmPushOpi64Const(opcodes, expression.dataNum); break;
		case WlBType_f32: wasmPushOpf32Const(opcodes, expression.dataNum); break;
		case WlBType_f64: wasmPushOpf64Const(opcodes, expression.dataNum); break;
		default: PANIC("UnHandled constant type %d", expression.type);
		}
	} break;
	case WlBKind_Ref: {
		WlSymbol *sym = expression.data;
		wasmPushOpLocalGet(opcodes, sym->index);
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
	source = wasmModuleCreate();

	wasmModuleAddMemory(&source, STR("memory"), 1, 2);

	u8 args[] = {WasmType_I32, WasmType_I32};

	// import print (hardcoded for now)
	fnPrint = wasmModuleAddImport(&source, STR("print"), BUF(args), BUFEMPTY);

	int functionCount = listLen(b->functions);
	for (int i = 0; i < functionCount; i++) {
		WlBoundFunction fn = b->functions[i];

		DynamicBuf args = dynamicBufCreate();
		for (int i = 0; i < fn.paramCount; i++) {
			WlSymbol param = fn.scope->symbols[i];
			dynamicBufPush(&args, boundTypeToWasm(param.type));
		}

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

		wasmModuleAddFunction(&source, fn.exported ? fn.name : STREMPTY, dynamicBufToBuf(args), dynamicBufToBuf(rets),
							  locals, dynamicBufToBuf(opcodes));
	}

	Buf wasm = wasmModuleCompile(source);
	return wasm;
}
