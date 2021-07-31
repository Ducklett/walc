#include <walc.h>
#include <wasm.c>

WasmType boundTypeToWasm(WlBType t)
{
	switch (t) {
	case WlBType_bool: return WasmType_I32;
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
int varOffset;

void emitOperator(WlBType type, WlBOperator op, DynamicBuf *opcodes)
{
	if (type == WlBType_bool) type = WlBType_i32;

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

	case WlBOperator_ShiftLeft:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Shl(opcodes); break;
		case WlBType_u32: wasmPushOpi32Shl(opcodes); break;
		case WlBType_i64: wasmPushOpi64Shl(opcodes); break;
		case WlBType_u64: wasmPushOpi64Shl(opcodes); break;
		case WlBType_f32: TODO("Support shift left on float32"); break;
		case WlBType_f64: TODO("Support shift left on float64"); break;
		}
		break;
	case WlBOperator_ShiftRight:
		switch (type) {
		case WlBType_i32: wasmPushOpi32ShrS(opcodes); break;
		case WlBType_u32: wasmPushOpi32ShrU(opcodes); break;
		case WlBType_i64: wasmPushOpi64ShrS(opcodes); break;
		case WlBType_u64: wasmPushOpi64ShrU(opcodes); break;
		case WlBType_f32: TODO("Support shift right on float32"); break;
		case WlBType_f64: TODO("Support shift right on float64"); break;
		}
		break;
	case WlBOperator_Greater:
		switch (type) {
		case WlBType_i32: wasmPushOpi32GtS(opcodes); break;
		case WlBType_u32: wasmPushOpi32GtU(opcodes); break;
		case WlBType_i64: wasmPushOpi64GtS(opcodes); break;
		case WlBType_u64: wasmPushOpi64GtU(opcodes); break;
		case WlBType_f32: wasmPushOpf32Gt(opcodes); break;
		case WlBType_f64: wasmPushOpf64Gt(opcodes); break;
		}
		break;
	case WlBOperator_GreaterOrEqual:
		switch (type) {
		case WlBType_i32: wasmPushOpi32GeS(opcodes); break;
		case WlBType_u32: wasmPushOpi32GeU(opcodes); break;
		case WlBType_i64: wasmPushOpi64GeS(opcodes); break;
		case WlBType_u64: wasmPushOpi64GeU(opcodes); break;
		case WlBType_f32: wasmPushOpf32Ge(opcodes); break;
		case WlBType_f64: wasmPushOpf64Ge(opcodes); break;
		}
		break;
	case WlBOperator_Less:
		switch (type) {
		case WlBType_i32: wasmPushOpi32LtS(opcodes); break;
		case WlBType_u32: wasmPushOpi32LtU(opcodes); break;
		case WlBType_i64: wasmPushOpi64LtS(opcodes); break;
		case WlBType_u64: wasmPushOpi64LtU(opcodes); break;
		case WlBType_f32: wasmPushOpf32Lt(opcodes); break;
		case WlBType_f64: wasmPushOpf64Lt(opcodes); break;
		}
		break;
	case WlBOperator_LessOrEqual:
		switch (type) {
		case WlBType_i32: wasmPushOpi32LeS(opcodes); break;
		case WlBType_u32: wasmPushOpi32LeU(opcodes); break;
		case WlBType_i64: wasmPushOpi64LeS(opcodes); break;
		case WlBType_u64: wasmPushOpi64LeU(opcodes); break;
		case WlBType_f32: wasmPushOpf32Le(opcodes); break;
		case WlBType_f64: wasmPushOpf64Le(opcodes); break;
		}
		break;
	case WlBOperator_Equal:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Eq(opcodes); break;
		case WlBType_u32: wasmPushOpi32Eq(opcodes); break;
		case WlBType_i64: wasmPushOpi64Eq(opcodes); break;
		case WlBType_u64: wasmPushOpi64Eq(opcodes); break;
		case WlBType_f32: wasmPushOpf32Eq(opcodes); break;
		case WlBType_f64: wasmPushOpf64Eq(opcodes); break;
		}
		break;
	case WlBOperator_NotEqual:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Ne(opcodes); break;
		case WlBType_u32: wasmPushOpi32Ne(opcodes); break;
		case WlBType_i64: wasmPushOpi64Ne(opcodes); break;
		case WlBType_u64: wasmPushOpi64Ne(opcodes); break;
		case WlBType_f32: wasmPushOpf32Ne(opcodes); break;
		case WlBType_f64: wasmPushOpf64Ne(opcodes); break;
		}
		break;
	case WlBOperator_And:
	case WlBOperator_BitwiseAnd:
		switch (type) {
		case WlBType_i32: wasmPushOpi32And(opcodes); break;
		case WlBType_u32: wasmPushOpi32And(opcodes); break;
		case WlBType_i64: wasmPushOpi64And(opcodes); break;
		case WlBType_u64: wasmPushOpi64And(opcodes); break;
		case WlBType_f32: TODO("Support and on float32"); break;
		case WlBType_f64: TODO("Support and on float64"); break;
		}
		break;
	case WlBOperator_Xor:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Xor(opcodes); break;
		case WlBType_u32: wasmPushOpi32Xor(opcodes); break;
		case WlBType_i64: wasmPushOpi64Xor(opcodes); break;
		case WlBType_u64: wasmPushOpi64Xor(opcodes); break;
		case WlBType_f32: TODO("Support xor on float32"); break;
		case WlBType_f64: TODO("Support xor on float64"); break;
		}
		break;
	case WlBOperator_Or:
	case WlBOperator_BitwiseOr:
		switch (type) {
		case WlBType_i32: wasmPushOpi32Or(opcodes); break;
		case WlBType_u32: wasmPushOpi32Or(opcodes); break;
		case WlBType_i64: wasmPushOpi64Or(opcodes); break;
		case WlBType_u64: wasmPushOpi64Or(opcodes); break;
		case WlBType_f32: TODO("Support or on float32"); break;
		case WlBType_f64: TODO("Support or on float64"); break;
		}
		break;
	default: PANIC("Unhandled operator %d", op); break;
	}
}

void emitStatement(WlbNode statement, DynamicBuf *opcodes);
void emitBlock(WlBoundBlock b, DynamicBuf *opcodes)
{
	for (int j = 0; j < listLen(b.nodes); j++) {
		WlbNode statementNode = b.nodes[j];
		emitStatement(statementNode, opcodes);
	}
}

void emitStatement(WlbNode statement, DynamicBuf *opcodes)
{
	switch (statement.kind) {
	case WlBKind_Return: {
		WlBoundReturn ret = *(WlBoundReturn *)statement.data;
		if (ret.expression.kind != WlBKind_None) {
			emitStatement(ret.expression, opcodes);
		}
		wasmPushOpBrReturn(opcodes);

	} break;
	case WlBKind_VariableDeclaration: {
		WlBoundVariable var = *(WlBoundVariable *)statement.data;
		if (var.initializer.kind != WlBKind_None) {
			emitStatement(var.initializer, opcodes);
			wasmPushOpLocalSet(opcodes, var.symbol->index);
		}
	} break;
	case WlBKind_VariableAssignment: {
		WlBoundAssignment var = *(WlBoundAssignment *)statement.data;
		emitStatement(var.expression, opcodes);
		wasmPushOpLocalSet(opcodes, var.symbol->index);
	} break;
	case WlBKind_Function: {
		// do nothing! These are added to the module globally
	} break;
	case WlBKind_None: break;
	case WlBKind_BinaryExpression: {
		WlBoundBinaryExpression bin = *(WlBoundBinaryExpression *)statement.data;
		emitStatement(bin.left, opcodes);
		emitStatement(bin.right, opcodes);
		emitOperator(bin.left.type, bin.operator, opcodes);
	} break;
	case WlBKind_Block: {
		WlBoundBlock blk = *(WlBoundBlock *)statement.data;
		emitBlock(blk, opcodes);
	} break;
	case WlBKind_If: {
		WlBoundIf tr = *(WlBoundIf *)statement.data;
		if (tr.thenBlock.kind == WlBKind_NumberLiteral && tr.thenBlock.kind == WlBKind_NumberLiteral) {
			emitStatement(tr.thenBlock, opcodes);
			emitStatement(tr.thenBlock, opcodes);
			emitStatement(tr.condition, opcodes);
			wasmPushOpSelect(opcodes);
		} else {
			emitStatement(tr.condition, opcodes);
			wasmPushOpIf(opcodes, boundTypeToWasm(statement.type));
			emitStatement(tr.thenBlock, opcodes);
			if (tr.elseBlock.kind != WlBKind_None) {
				wasmPushOpElse(opcodes);
				emitStatement(tr.elseBlock, opcodes);
			}
			wasmPushOpEnd(opcodes);
		}
	} break;
	case WlBKind_WhileLoop: {
		WlBoundWhile whl = *(WlBoundWhile *)statement.data;
		wasmPushOpBlock(opcodes, WasmType_Void);
		wasmPushOpLoop(opcodes, WasmType_Void);

		// (br_if 1 (eqz (condition)))
		emitStatement(whl.condition, opcodes);
		wasmPushOpi32Eqz(opcodes);
		wasmPushOpBrIf(opcodes, 1);

		emitStatement(whl.block, opcodes);

		wasmPushOpBr(opcodes, 0);

		wasmPushOpEnd(opcodes); // end loop
		wasmPushOpEnd(opcodes); // end block

	} break;
	case WlBKind_DoWhileLoop: {
		WlBoundDoWhile whl = *(WlBoundDoWhile *)statement.data;
		wasmPushOpBlock(opcodes, WasmType_Void);
		wasmPushOpLoop(opcodes, WasmType_Void);

		emitStatement(whl.block, opcodes);

		// (br_if 1 (eqz (condition)))
		emitStatement(whl.condition, opcodes);
		wasmPushOpi32Eqz(opcodes);
		wasmPushOpBrIf(opcodes, 1);

		wasmPushOpBr(opcodes, 0);

		wasmPushOpEnd(opcodes); // end loop
		wasmPushOpEnd(opcodes); // end block

	} break;
	case WlBKind_NumberLiteral: {
		switch (statement.type) {
		case WlBType_bool: /*FALLTHROUGH*/
		case WlBType_i32:  /*FALLTHROUGH*/
		case WlBType_u32: wasmPushOpi32Const(opcodes, statement.dataNum); break;
		case WlBType_i64: /*FALLTHROUGH*/
		case WlBType_u64: wasmPushOpi64Const(opcodes, statement.dataNum); break;
		case WlBType_f32: wasmPushOpf32Const(opcodes, statement.dataFloat); break;
		case WlBType_f64: wasmPushOpf64Const(opcodes, statement.dataFloat); break;
		default: PANIC("UnHandled constant type %d", statement.type);
		}
	} break;
	case WlBKind_StringLiteral: {
		int offset = wasmModuleAddData(&source, STRTOBUF(statement.dataStr));
		int length = statement.dataStr.len;
		wasmPushOpi32Const(opcodes, offset);
		wasmPushOpi32Const(opcodes, length);
	} break;
	case WlBKind_Ref: {
		WlSymbol *sym = statement.data;
		wasmPushOpLocalGet(opcodes, sym->index);
	} break;
	case WlBKind_Call: {
		WlBoundCallExpression call = *(WlBoundCallExpression *)statement.data;
		for (int i = 0; i < listLen(call.args); i++) {
			emitStatement(call.args[i], opcodes);
		}

		if (call.function->index == -1) {
			call.function->index = wasmModuleReserveFunctionId(&source);
		}

		wasmPushOpCall(opcodes, call.function->index);
	} break;
	case WlBKind_PreUnaryExpression: {
		WlBoundPreUnaryExpression un = *(WlBoundPreUnaryExpression *)statement.data;

		switch (un.operator) {
		case WlBOperator_Negate:
			emitStatement(un.expression, opcodes);
			wasmPushOpi32Eqz(opcodes);
			break;

		case WlBOperator_Subtract: {
			switch (un.expression.type) {
			case WlBType_u32:
			case WlBType_i32: {
				wasmPushOpi32Const(opcodes, 0);
				emitStatement(un.expression, opcodes);
				wasmPushOpi32Sub(opcodes);
			} break;
			case WlBType_u64:
			case WlBType_i64: {
				wasmPushOpi64Const(opcodes, 0);
				emitStatement(un.expression, opcodes);
				wasmPushOpi64Sub(opcodes);
			} break;
			case WlBType_f32:
				emitStatement(un.expression, opcodes);
				wasmPushOpf32Neg(opcodes);
				break;
			case WlBType_f64:
				emitStatement(un.expression, opcodes);
				wasmPushOpf64Neg(opcodes);
				break;
			}
			break;
		}
		default: PANIC("Unhandled pre unary operator %d", un.operator); break;
		}
	} break;
	default: PANIC("Unhandled statement kind %d", statement.kind); break;
	}
}

Buf emitWasm(WlBinder *b)
{
	source = wasmModuleCreate();

	wasmModuleAddMemory(&source, STR("memory"), 1, 2);

	// u8 args[] = {WasmType_I32, WasmType_I32};

	// // import print (hardcoded for now)
	// fnPrint = wasmModuleAddImport(&source, STR("print"), BUF(args), BUFEMPTY);

	int functionCount = listLen(b->functions);
	for (int i = 0; i < functionCount; i++) {
		varOffset = 0;
		WlBoundFunction *fn = b->functions[i];

		int index = fn->symbol->index;
		if (index == -1) {
			index = wasmModuleReserveFunctionId(&source);
			fn->symbol->index = index;
		}

		DynamicBuf args = dynamicBufCreate();
		for (int i = 0; i < fn->paramCount; i++) {
			WlSymbol *param = fn->scope->symbols[i];
			param->index = varOffset;
			if (param->type == WlBType_str) {
				dynamicBufPush(&args, WasmType_I32);
				dynamicBufPush(&args, WasmType_I32);
				varOffset += 2;
			} else {
				dynamicBufPush(&args, boundTypeToWasm(param->type));
				varOffset += 1;
			}
		}

		DynamicBuf rets = dynamicBufCreate();
		WasmType returnType = boundTypeToWasm(fn->symbol->type);
		if (returnType != WasmType_Void) dynamicBufPush(&rets, returnType);

		if (fn->symbol->flags & WlSFlag_Import) {
			wasmModuleAddImport(&source, fn->symbol->name, dynamicBufToBuf(args), dynamicBufToBuf(rets), index);
		} else {

			DynamicBuf locals = dynamicBufCreate();

			for (int i = fn->paramCount; i < listLen(fn->scope->symbols); i++) {
				WlSymbol *local = fn->scope->symbols[i];
				if ((local->flags & WlSFlag_TypeBits) == WlSFlag_Function) continue;
				local->index = varOffset;
				if (local->type == WlBType_str) {
					dynamicBufPush(&locals, WasmType_I32);
					dynamicBufPush(&locals, WasmType_I32);
					varOffset += 2;
				} else {
					dynamicBufPush(&locals, boundTypeToWasm(local->type));
					varOffset += 1;
				}
			}

			DynamicBuf opcodes = dynamicBufCreate();

			emitStatement(fn->body, &opcodes);

			wasmModuleAddFunction(&source, (fn->symbol->flags & WlSFlag_Export) ? fn->symbol->name : STREMPTY,
								  dynamicBufToBuf(args), dynamicBufToBuf(rets), dynamicBufToBuf(locals),
								  dynamicBufToBuf(opcodes), index);
		}
	}

	Buf wasm = wasmModuleCompile(source);
	return wasm;
}
