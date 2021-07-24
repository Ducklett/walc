#include <walc.h>

WlBType wlBindType(WlToken tk)
{
	if (tk.kind == WlKind_Missing) return WlBType_u0;

	assert(tk.kind == WlKind_Symbol);

	for (int i = 0; i < WlBType_end; i++) {
		// TODO: cache Str
		Str typeString = strFromCstr(WlBTypeText[i]);
		if (strEqual(tk.valueStr, typeString)) {
			return i;
		}
	}

	PANIC("Unknown type %.*s %d", STRPRINT(tk.valueStr), WlBType_unknown);
	return WlBType_unknown;
}

typedef enum
{
	WlBKind_None,
	WlBKind_Function,
	WlBKind_VariableDeclaration,
	WlBKind_VariableAssignment,
	WlBKind_Block,
	WlBKind_Call,
	WlBKind_Ref,
	WlBKind_Return,
	WlBKind_BinaryExpression,
	WlBKind_StringLiteral,
	WlBKind_NumberLiteral,
	WlBKind_BoolLiteral,
} WlBKind;

typedef enum
{
	WlBOperator_Add,
	WlBOperator_Subtract,
	WlBOperator_Divide,
	WlBOperator_Multiply,
	WlBOperator_Modulo,
	WlBOperator_Equal,
	WlBOperator_NotEqual,
} WlBOperator;

typedef enum
{
	WlSFlag_None = 0,
	WlSFlag_TypeBits = 7,

	WlSFlag_Any = 0,
	WlSFlag_Variable = 1,
	WlSFlag_Function = 2,
	WlSFlag_Struct = 3,
	WlSFlag_Namespace = 4,

	WlSFlag_Constant = 8,
	WlSFlag_Import = 16,
	WlSFlag_Export = 32,
} WlSymbolFlags;

struct WlBoundFunction;

typedef struct {
	int index;
	Str name;
	WlBType type;
	WlSymbolFlags flags;
	union {
		struct WlBoundFunction *function;
	};
} WlSymbol;

typedef struct WlScope {
	List(WlSymbol *) symbols;
	struct WlScope *parentScope;
} WlScope;

typedef struct {
	WlBKind kind;
	WlBType type;
	WlSpan span;
	union {
		void *data;
		Str dataStr;
		i64 dataNum;
		f64 dataFloat;
	};
} WlbNode;

typedef struct {
	WlSymbol *function;
	List(WlbNode) args;
} WlBoundCallExpression;

typedef struct {
	WlbNode left;
	WlBOperator operator;
	WlbNode right;
} WlBoundBinaryExpression;

typedef struct {
	WlScope *scope;
	WlbNode *nodes;
	int nodeCount;
} WlBoundBlock;

typedef struct {
	WlbNode expression;
} WlBoundReturn;

typedef struct WlBoundFunction {
	WlScope *scope;
	int paramCount;
	WlbNode body;
	WlSymbol *symbol;
} WlBoundFunction;

typedef struct WlBoundVariable {
	WlbNode initializer;
	WlSymbol *symbol;
} WlBoundVariable;

typedef struct {
	WlbNode expression;
	WlSymbol *symbol;
} WlBoundAssignment;

typedef struct {
	WlToken *unboundDeclarations;
	int unboundCount;

	List(WlBoundFunction *) functions;
	List(WlScope *) scopes;
	List(WlDiagnostic) diagnostics;
	WlBType currentReturnType;
	ArenaAllocator arena;
} WlBinder;

WlSymbol *wlFindSymbol(WlBinder *b, Str name, WlSymbolFlags flags, bool recurse);

WlSymbol *wlPushSymbol(WlBinder *b, Str name, WlBType type, WlSymbolFlags flags)
{
	WlSymbol *existing = wlFindSymbol(b, name, WlSFlag_None, false);
	if (existing) {
		return NULL;
	}

	List(WlSymbol *) symbols = listPeek(&b->scopes)->symbols;

	WlSymbol *newSymbol = arenaMalloc(sizeof(WlSymbol), &b->arena);
	*newSymbol = (WlSymbol){.name = name, .type = type, .flags = flags, .index = 0};
	listPush(&symbols, newSymbol);
	listPeek(&b->scopes)->symbols = symbols;

	return newSymbol;
}

WlSymbol *wlPushVariable(WlBinder *b, WlSpan span, Str name, WlBType type)
{
	WlSymbol *s = wlPushSymbol(b, name, type, WlSFlag_Variable);

	if (!s) {
		WlDiagnostic d = {.kind = VariableAlreadyExistsDiagnostic, .span = span, .str1 = name};
		listPush(&b->diagnostics, d);
	}
	return s;
}

WlSymbol *wlFindVariable(WlBinder *b, WlSpan span, Str name)
{
	WlSymbol *variable = wlFindSymbol(b, name, WlSFlag_Variable, true);

	if (!variable) {
		WlDiagnostic d = {.kind = VariableNotFoundDiagnostic, .span = span, .str1 = name};
		listPush(&b->diagnostics, d);
		return NULL;
	}

	// if (type != variable->type) {
	// 	PANIC("Type mismatch! expected %d, got %d", type, variable->type);
	// }

	return variable;
}

WlSymbol *wlFindSymbolInScope(WlBinder *b, WlScope *s, Str name, WlSymbolFlags flags, bool recurse)
{
	WlSymbol *found = NULL;

	WlSymbolFlags type = flags & WlSFlag_TypeBits;

	for (int i = 0; i < listLen(s->symbols); i++) {
		WlSymbol *smb = s->symbols[i];
		if (strEqual(name, smb->name) && (!type || type == (smb->flags & WlSFlag_TypeBits))) {
			found = smb;
			break;
		}
	}

	if (!found && recurse && s->parentScope) {
		return wlFindSymbolInScope(b, s->parentScope, name, flags, true);
	}

	return found;
}

WlSymbol *wlFindSymbol(WlBinder *b, Str name, WlSymbolFlags flags, bool recurse)
{
	WlScope *s = listPeek(&b->scopes);

	return wlFindSymbolInScope(b, s, name, flags, recurse);
}

WlBOperator wlBindOperator(WlToken op)
{
	switch (op.kind) {
	case WlKind_OpPlus: return WlBOperator_Add;
	case WlKind_OpMinus: return WlBOperator_Subtract;
	case WlKind_OpStar: return WlBOperator_Multiply;
	case WlKind_OpSlash: return WlBOperator_Divide;
	case WlKind_OpPercent: return WlBOperator_Modulo;
	case WlKind_OpDoubleEquals: return WlBOperator_Equal;
	case WlKind_OpBangEquals: return WlBOperator_NotEqual;
	default: PANIC("Unhandled operator kind %s", WlKindText[op.kind]);
	}
}

bool wlIsNumberType(WlBType t) { return (t >= WlBType_u8 && t <= WlBType_f64); }
bool wlIsIntegerType(WlBType t) { return (t >= WlBType_u8 && t <= WlBType_i64); }
bool wlIsFloatType(WlBType t) { return (t == WlBType_f32 || t == WlBType_f64); }
bool wlIsConcreteType(WlBType t) { return (t < WlBType_end); }

WlbNode wlBindExpressionOfType(WlBinder *b, WlToken expression, WlBType type);

WlBType resolveBinaryExpressionType(WlBType operandType, WlBOperator op)
{
	switch (op) {
	case WlBOperator_Add: return operandType;
	case WlBOperator_Subtract: return operandType;
	case WlBOperator_Divide: return operandType;
	case WlBOperator_Multiply: return operandType;
	case WlBOperator_Modulo: return operandType;
	case WlBOperator_Equal: return WlBType_bool;
	case WlBOperator_NotEqual: return WlBType_bool;
	default: PANIC("Unhandled binary expression operator %d", op); break;
	}
}

// changes the type of an expression after a more concrete type is found
void propagateType(WlbNode *expr, WlBType type)
{
	assert(!wlIsConcreteType(expr->type));

	switch (expr->kind) {
	case WlBKind_BinaryExpression: {
		WlBoundBinaryExpression *bin = expr->data;
		propagateType(&bin->left, type);
		propagateType(&bin->right, type);
		expr->type = type;
	} break;
	case WlBKind_NumberLiteral: {
		if (wlIsFloatType(type) && expr->type == WlBType_integerNumber) {
			expr->dataFloat = (f64)expr->dataNum;
		}
		expr->type = type;
	} break;
	}
}

WlbNode wlBindExpression(WlBinder *b, WlToken expression)
{
	switch (expression.kind) {
	case WlKind_Number: {
		return (WlbNode){
			.kind = WlBKind_NumberLiteral,
			.dataNum = expression.valueNum,
			.type = WlBType_integerNumber,
			.span = expression.span,
		};
	} break;
	case WlKind_FloatNumber: {
		return (WlbNode){
			.kind = WlBKind_NumberLiteral,
			.dataFloat = expression.valueFloat,
			.type = WlBType_floatingNumber,
			.span = expression.span,
		};
	} break;
	case WlKind_String: {
		return (WlbNode){
			.kind = WlBKind_StringLiteral,
			.dataStr = expression.valueStr,
			.type = WlBType_str,
			.span = expression.span,
		};
	} break;
	case WlKind_KwTrue: {
		return (WlbNode){
			.kind = WlBKind_BoolLiteral,
			.dataNum = 1,
			.type = WlBType_bool,
			.span = expression.span,
		};
	} break;
	case WlKind_KwFalse: {
		return (WlbNode){
			.kind = WlBKind_BoolLiteral,
			.dataNum = 0,
			.type = WlBType_bool,
			.span = expression.span,
		};
	} break;
	case WlKind_StBinaryExpression: {
		WlBinaryExpression ex = *(WlBinaryExpression *)expression.valuePtr;
		WlBoundBinaryExpression bex;

		bex.left = wlBindExpression(b, ex.left);
		bex.operator= wlBindOperator(ex.operator);
		bex.right = wlBindExpression(b, ex.right);

		if (bex.left.type != bex.right.type) {
			if (bex.left.type == WlBType_integerNumber && bex.right.type == WlBType_floatingNumber) {
				propagateType(&bex.left, WlBType_floatingNumber);
			} else if (bex.left.type == WlBType_floatingNumber && bex.right.type == WlBType_integerNumber) {
				propagateType(&bex.right, WlBType_floatingNumber);
			} else {
				PANIC("Binary expression operands must be of same type, got %d %d", bex.left.type, bex.right.type);
			}
		}

		WlBType type = resolveBinaryExpressionType(bex.left.type, bex.operator);

		// todo: get the proper return type based on the operator and operands

		WlBoundBinaryExpression *bexp = arenaMalloc(sizeof(WlBoundBinaryExpression), &b->arena);
		*bexp = bex;

		return (WlbNode){
			.kind = WlBKind_BinaryExpression,
			.data = bexp,
			.type = type,
			.span = expression.span,
		};
	}
	case WlKind_StCall: {
		WlSyntaxCall call = *(WlSyntaxCall *)expression.valuePtr;

		WlBoundCallExpression bcall = {0};
		WlSymbol *function = wlFindSymbol(b, call.name.valueStr, WlSFlag_Function, true);
		List(WlbNode) args = listNew();
		int argLen = (listLen(call.args) + 1) / 2;

		int paramCount;
		if (!function) PANIC("Failed to find function by name %.*s", STRPRINT(call.name.valueStr));

		int expectedArgLen = function->function->paramCount;
		if (argLen != expectedArgLen) PANIC("Expected %d arguments but got %d", expectedArgLen, argLen);

		for (int i = 0; i < argLen; i++) {
			WlbNode arg = wlBindExpressionOfType(b, call.args[i * 2], function->function->scope->symbols[i]->type);
			listPush(&args, arg);
		}
		bcall.args = args;
		bcall.function = function;

		WlBoundCallExpression *bcallp = arenaMalloc(sizeof(WlBoundCallExpression), &b->arena);
		*bcallp = bcall;

		return (WlbNode){.kind = WlBKind_Call, .data = bcallp, .type = WlBType_u0, .span = expression.span};
	}

	case WlKind_StRef: {
		Str name = expression.valueStr;
		WlSymbol *variable = wlFindVariable(b, expression.span, name);

		return (WlbNode){.kind = WlBKind_Ref, .data = variable, .type = variable->type, .span = expression.span};
	}

	default: PANIC("Unhandled expression kind %s", WlKindText[expression.kind]);
	}
}

WlBType makeContreteType(WlBType t)
{
	switch (t) {
	case WlBType_integerNumber: return WlBType_i64;
	case WlBType_floatingNumber: return WlBType_f64;
	default: return t;
	}
}

WlbNode wlBindExpressionOfType(WlBinder *b, WlToken expression, WlBType expectedType)
{
	//
	WlbNode n = wlBindExpression(b, expression);

	if (n.type == expectedType) return n;
	if (expectedType == WlBType_inferWeak) return n;

	if (expectedType == WlBType_inferStrong) {
		if (!wlIsConcreteType(n.type)) {
			propagateType(&n, makeContreteType(n.type));
		}
		return n;
	}

	assert(wlIsConcreteType(expectedType));

	if (!wlIsConcreteType(n.type)) {
		if ((wlIsIntegerType(expectedType) && n.type == WlBType_floatingNumber)) {
			// expected int but got float, make it concrete and report error at the end of the function
			propagateType(&n, makeContreteType(n.type));
		} else {
			propagateType(&n, expectedType);
			return n;
		}
	}

	if (n.type != expectedType) {
		WlDiagnostic d = {.kind = CannotImplicitlyConvertDiagnostic,
						  .span = n.span,
						  .num1 = n.type,
						  .num2 = expectedType};
		listPush(&b->diagnostics, d);
		// PANIC("Unexpected type %d, expected %d", n.type, expectedType);
		n.type = expectedType;
	}

	return n;
}

WlbNode wlBindStatement(WlBinder *b, WlToken statement)
{
	switch (statement.kind) {
	case WlKind_StReturnStatement: {
		WlReturnStatement ret = *(WlReturnStatement *)statement.valuePtr;

		WlBoundReturn bret;

		if (ret.expression.kind != WlKind_Missing) {
			WlBType t = b->currentReturnType;
			bret.expression = wlBindExpressionOfType(b, ret.expression, t);
		} else {
			if (b->currentReturnType != WlBType_u0) {
				PANIC("Expected return to have expression because return type is not u0");
			}
			bret.expression = (WlbNode){.kind = WlBKind_None, .type = b->currentReturnType};
		}

		WlBoundReturn *bretp = arenaMalloc(sizeof(WlBoundReturn), &b->arena);
		*bretp = bret;
		return (WlbNode){.kind = WlBKind_Return, .data = bretp, .type = b->currentReturnType, .span = statement.span};
	} break;
	case WlKind_StExpressionStatement: {
		WlExpressionStatement ex = *(WlExpressionStatement *)statement.valuePtr;
		return wlBindExpressionOfType(b, ex.expression, WlBType_u0);
	} break;
	case WlKind_StVariableDeclaration: {
		WlSyntaxVariableDeclaration var = *(WlSyntaxVariableDeclaration *)statement.valuePtr;
		WlBoundVariable *bvar = arenaMalloc(sizeof(WlBoundVariable), &b->arena);

		WlBType type = wlBindType(var.type);
		Str name = var.name.valueStr;
		WlSpan span = var.name.span;
		bvar->symbol = wlPushVariable(b, span, name, type);

		if (var.initializer.kind == WlKind_Missing) {
			bvar->initializer = (WlbNode){.kind = WlBKind_None};
		} else {
			bvar->initializer = wlBindExpressionOfType(b, var.initializer, type);
		}
		return (WlbNode){.kind = WlBKind_VariableDeclaration, .data = bvar, .type = type, .span = statement.span};
	} break;
	case WlKind_StVariableAssignement: {
		WlAssignmentExpression var = *(WlAssignmentExpression *)statement.valuePtr;
		WlBoundAssignment *bvar = arenaMalloc(sizeof(WlBoundAssignment), &b->arena);

		WlSymbol *variable = wlFindSymbol(b, var.variable.valueStr, WlSFlag_Variable, true);

		bvar->symbol = variable;
		bvar->expression = wlBindExpressionOfType(b, var.expression, variable->type);

		return (
			WlbNode){.kind = WlBKind_VariableAssignment, .data = bvar, .type = variable->type, .span = statement.span};
	} break;
	default: PANIC("Unhandled statement kind %s", WlKindText[statement.kind]);
	}
}

WlScope *WlCreateAndPushScope(WlBinder *b)
{
	WlScope *scope = arenaMalloc(sizeof(WlScope), &b->arena);
	*scope = (WlScope){.symbols = listNew(), .parentScope = listPeek(&b->scopes)};
	listPush(&b->scopes, scope);
	return scope;
};

void WlPopScope(WlBinder *b)
{
	listPop(&b->scopes);
	assert(!listIsEmpty(b->scopes) && "The global scope shouldn't be popped");
}

WlbNode wlBindBlock(WlBinder *b, WlSyntaxBlock body, bool createScope)
{
	WlBoundBlock *blk = arenaMalloc(sizeof(WlBoundBlock), &b->arena);
	*blk = (WlBoundBlock){0};
	if (createScope) {
		blk->scope = WlCreateAndPushScope(b);
	} else {
		blk->scope = listPeek(&b->scopes);
	}

	blk->nodeCount = body.statementCount;
	size_t foo = sizeof(WlbNode) * blk->nodeCount;
	WlbNode *nodes = arenaMalloc(foo, &b->arena);
	for (int i = 0; i < body.statementCount; i++) {
		nodes[i] = wlBindStatement(b, body.statements[i]);
	}
	blk->nodes = nodes;

	if (createScope) WlPopScope(b);

	return (WlbNode){.kind = WlBKind_Block,
					 .type = WlBType_u0,
					 .data = blk,
					 .span = spanFromTokens(body.curlyOpen, body.curlyClose)};
}

WlBinder wlBinderCreate(WlToken *declarations, int declarationCount)
{
	WlBinder b = {
		.unboundDeclarations = declarations,
		.unboundCount = declarationCount,
		.functions = listNew(),
		.scopes = listNew(),
		.diagnostics = listNew(),
		.arena = arenaCreate(),
	};

	//	 push the global scope
	WlCreateAndPushScope(&b);
	return b;
}

void wlBinderFree(WlBinder *b)
{
	arenaFree(&b->arena);
	listFree(&b->functions);
	listFree(&b->scopes);
}

WlBinder wlBind(WlToken *declarations, int declarationCount)
{
	WlBinder b = wlBinderCreate(declarations, declarationCount);

	for (int i = 0; i < declarationCount; i++) {
		WlToken tk = declarations[i];
		switch (tk.kind) {
		case WlKind_StImport: {
			WlSyntaxImport im = *(WlSyntaxImport *)(tk.valuePtr);

			WlBoundFunction *bf = arenaMalloc(sizeof(WlBoundFunction), &b.arena);

			WlSymbol *functionSymbol =
				wlPushSymbol(&b, im.name.valueStr, WlBType_u0, WlSFlag_Function | WlSFlag_Constant | WlSFlag_Import);
			bf->symbol = functionSymbol;
			WlScope *s = WlCreateAndPushScope(&b);
			functionSymbol->function = bf;
			bf->scope = s;
			int paramCount = 0;

			for (int i = 0; i < listLen(im.parameterList); i += 2) {
				paramCount++;
				WlToken paramToken = im.parameterList[i];
				WlSyntaxParameter param = *(WlSyntaxParameter *)paramToken.valuePtr;

				WlBType paramType = wlBindType(param.type);
				wlPushSymbol(&b, param.name.valueStr, paramType, WlSFlag_Variable | WlSFlag_Constant);
			}

			WlPopScope(&b);
			bf->paramCount = paramCount;

			listPush(&b.functions, bf);
		} break;
		case WlKind_StFunction: {
			WlSyntaxFunction fn = *(WlSyntaxFunction *)(tk.valuePtr);

			WlBType returnType = wlBindType(fn.type);
			b.currentReturnType = returnType;

			WlSymbolFlags flags = WlSFlag_Function | WlSFlag_Constant;
			if (fn.export.kind == WlKind_KwExport) flags |= WlSFlag_Export;
			WlSymbol *functionSymbol = wlPushSymbol(&b, fn.name.valueStr, returnType, flags);

			WlScope *s = WlCreateAndPushScope(&b);

			int paramCount = 0;

			WlBoundFunction *bf = arenaMalloc(sizeof(WlBoundFunction), &b.arena);

			functionSymbol->function = bf;
			bf->symbol = functionSymbol;

			for (int i = 0; i < listLen(fn.parameterList); i += 2) {
				paramCount++;
				WlToken paramToken = fn.parameterList[i];
				WlSyntaxParameter param = *(WlSyntaxParameter *)paramToken.valuePtr;

				WlBType paramType = wlBindType(param.type);

				wlPushSymbol(&b, param.name.valueStr, paramType, WlSFlag_Variable | WlSFlag_Constant);
			}

			bf->scope = s;
			bf->paramCount = paramCount;
			bf->body = wlBindBlock(&b, fn.body, false), listPush(&b.functions, bf);

			WlPopScope(&b);
		} break;
		case WlKind_Bad: break;
		default: PANIC("Unhandled declaration kind %s", WlKindText[tk.kind]); break;
		}
	}

	return b;
}
