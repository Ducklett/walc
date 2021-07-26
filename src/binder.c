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
	WlBOperator_ShiftLeft,
	WlBOperator_ShiftRight,
	WlBOperator_Greater,
	WlBOperator_GreaterOrEqual,
	WlBOperator_Less,
	WlBOperator_LessOrEqual,
	WlBOperator_Equal,
	WlBOperator_NotEqual,
	WlBOperator_BitwiseAnd,
	WlBOperator_Xor,
	WlBOperator_BitwiseOr,
	WlBOperator_And,
	WlBOperator_Or,
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

struct WlScope;

typedef struct {
	int index;
	Str name;
	WlBType type;
	WlSymbolFlags flags;
	union {
		struct WlBoundFunction *function;
		struct WlScope *scope;
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
	List(WlbNode) nodes;
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
		if ((flags & WlSFlag_TypeBits) == WlSFlag_Namespace) {
			return existing;
		} else {
			return NULL;
		}
	}

	List(WlSymbol *) symbols = listPeek(&b->scopes)->symbols;

	WlSymbol *newSymbol = arenaMalloc(sizeof(WlSymbol), &b->arena);
	*newSymbol = (WlSymbol){
		.name = name,
		.type = type,
		.flags = flags,
		.index = 0,
		.scope = NULL,
	};
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

WlSymbol *wlFindSymbolInNamespace(WlBinder *b, List(WlToken) path, WlSymbolFlags flags)
{
	int pathLen = listLen(path);

	WlSymbol *s = NULL;
	WlScope *scope;
	assert(pathLen > 0);
	for (int i = 0; i < pathLen; i += 2) {
		bool isLast = i == pathLen - 1;
		if (i == 0) {
			s = wlFindSymbol(b, path[i].valueStr, isLast ? flags : WlSFlag_Namespace, true);
			if (!isLast) {
				assert(s != NULL);
				assert(s->flags == WlSFlag_Namespace);
				assert(s->scope != NULL);
				scope = s->scope;
			}
		} else {
			assert(s != NULL);
			s = wlFindSymbolInScope(b, scope, path[i].valueStr, isLast ? flags : WlSFlag_Namespace, false);
			if (!isLast) {
				assert(s != NULL);
				assert(s->flags == WlSFlag_Namespace);
				assert(s->scope != NULL);
				scope = s->scope;
			}
		}
	}

	if (s == NULL) {
		PANIC("Failed to find symbol");
		return NULL;
	}

	WlSymbolFlags type = flags & WlSFlag_TypeBits;

	if ((flags & WlSFlag_TypeBits) != (s->flags & WlSFlag_TypeBits)) {
		PANIC("expected symbol type %d but got %d", (flags & WlSFlag_TypeBits), (s->flags & WlSFlag_TypeBits));
	}

	return s;
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
	case WlKind_OpLessLess: return WlBOperator_ShiftLeft;
	case WlKind_OpGreaterGreater: return WlBOperator_ShiftRight;
	case WlKind_OpGreater: return WlBOperator_Greater;
	case WlKind_OpGreaterEquals: return WlBOperator_GreaterOrEqual;
	case WlKind_OpLess: return WlBOperator_Less;
	case WlKind_OpLessEquals: return WlBOperator_LessOrEqual;
	case WlKind_OpEuqualsEquals: return WlBOperator_Equal;
	case WlKind_OpBangEquals: return WlBOperator_NotEqual;
	case WlKind_OpAmpersand: return WlBOperator_BitwiseAnd;
	case WlKind_OpCaret: return WlBOperator_Xor;
	case WlKind_OpPipe: return WlBOperator_BitwiseOr;
	case WlKind_OpAmpersandAmpersand: return WlBOperator_And;
	case WlKind_OpPipePipe: return WlBOperator_Or;
	default: PANIC("Unhandled operator kind %s", WlKindText[op.kind]);
	}
}

bool wlIsNumberType(WlBType t) { return (t >= WlBType_u8 && t <= WlBType_f64); }
bool wlIsIntegerType(WlBType t) { return (t >= WlBType_u8 && t <= WlBType_i64); }
bool wlIsFloatType(WlBType t) { return (t == WlBType_f32 || t == WlBType_f64); }
bool wlIsConcreteType(WlBType t) { return (t < WlBType_end); }

WlbNode wlBindFunction(WlBinder *b, WlToken tk);
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
		if (wlIsConcreteType(bex.left.type)) {
			bex.right = wlBindExpressionOfType(b, ex.right, bex.left.type);
		} else {
			bex.right = wlBindExpression(b, ex.right);
		}

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
		static int foo = 0;

		WlSyntaxCall call = *(WlSyntaxCall *)expression.valuePtr;

		WlBoundCallExpression bcall = {0};
		WlSymbol *function = wlFindSymbolInNamespace(b, call.path, WlSFlag_Function);
		List(WlbNode) args = listNew();
		int argLen = (listLen(call.args) + 1) / 2;

		int paramCount;
		assert((function->flags & WlSFlag_TypeBits) == WlSFlag_Function);
		if (!function)
			PANIC("Failed to find function by name %.*s", STRPRINT(call.path[listLen(call.path) - 1].valueStr));

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
		foo++;
		return (WlbNode){.kind = WlBKind_Call, .data = bcallp, .type = WlBType_u0, .span = expression.span};
	}

	case WlKind_StRef: {
		List(WlToken) path = expression.valuePtr;
		WlSymbol *variable = wlFindSymbolInNamespace(b, path, WlSFlag_Variable);

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
		WlbNode n = wlBindExpressionOfType(b, ex.expression, WlBType_u0);
		return n;
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
	case WlKind_StFunction: return wlBindFunction(b, statement);
	default: PANIC("Unhandled statement kind %s", WlKindText[statement.kind]);
	}
}

WlScope *WlCreateAndPushNamespace(WlBinder *b, Str name)
{
	WlSymbol *ns = wlPushSymbol(b, name, WlBType_u0, WlSFlag_Namespace);

	// wlPushSymbol might return an existing namespace
	// in this case we shouldn't create a new scope and instead
	// just keep the existing one
	if (ns->scope == NULL) {
		WlScope *scope = arenaMalloc(sizeof(WlScope), &b->arena);
		*scope = (WlScope){
			.symbols = listNew(),
			.parentScope = listPeek(&b->scopes),
		};
		ns->scope = scope;
	}

	listPush(&b->scopes, ns->scope);
	return ns->scope;
}

WlScope *WlCreateAndPushScope(WlBinder *b)
{
	WlScope *scope = arenaMalloc(sizeof(WlScope), &b->arena);
	*scope = (WlScope){
		.symbols = listNew(),
		.parentScope = listPeek(&b->scopes),
	};
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

	blk->nodes = listNew();

	for (int i = 0; i < listLen(body.statements); i++) {
		WlbNode st = wlBindStatement(b, body.statements[i]);
		listPush(&blk->nodes, st);
	}

	if (createScope) WlPopScope(b);

	return (WlbNode){.kind = WlBKind_Block,
					 .type = WlBType_u0,
					 .data = blk,
					 .span = spanFromTokens(body.curlyOpen, body.curlyClose)};
}

WlBinder wlBinderCreate()
{
	WlBinder b = {
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

void wlBindDeclarations(WlBinder *b, List(WlToken) declarations);

WlbNode wlBindFunction(WlBinder *b, WlToken tk)
{
	WlSyntaxFunction fn = *(WlSyntaxFunction *)(tk.valuePtr);

	WlBType returnType = wlBindType(fn.type);
	b->currentReturnType = returnType;

	WlSymbolFlags flags = WlSFlag_Function | WlSFlag_Constant;
	if (fn.export.kind == WlKind_KwExport) flags |= WlSFlag_Export;
	WlSymbol *functionSymbol = wlPushSymbol(b, fn.name.valueStr, returnType, flags);

	WlScope *s = WlCreateAndPushScope(b);

	int paramCount = 0;

	WlBoundFunction *bf = arenaMalloc(sizeof(WlBoundFunction), &b->arena);

	functionSymbol->function = bf;
	bf->symbol = functionSymbol;

	for (int i = 0; i < listLen(fn.parameterList); i += 2) {
		paramCount++;
		WlToken paramToken = fn.parameterList[i];
		WlSyntaxParameter param = *(WlSyntaxParameter *)paramToken.valuePtr;

		WlBType paramType = wlBindType(param.type);

		wlPushSymbol(b, param.name.valueStr, paramType, WlSFlag_Variable | WlSFlag_Constant);
	}

	bf->scope = s;
	bf->paramCount = paramCount;
	bf->body = wlBindBlock(b, fn.body, false), listPush(&b->functions, bf);

	WlPopScope(b);
	return (WlbNode){.kind = WlBKind_Function};
}

WlBinder wlBind(List(WlToken) declarations)
{
	WlBinder b = wlBinderCreate();
	wlBindDeclarations(&b, declarations);
	return b;
}

void wlBindDeclarations(WlBinder *b, List(WlToken) declarations)
{
	int declarationCount = listLen(declarations);
	for (int i = 0; i < declarationCount; i++) {

		WlToken tk = declarations[i];
		switch (tk.kind) {
		case WlKind_StNamespace: {
			WlSyntaxNamespace ns = *(WlSyntaxNamespace *)(tk.valuePtr);
			Str name = ns.path[listLen(ns.path) - 1].valueStr;
			int depth = 0;

			for (int i = 0; i < listLen(ns.path); i += 2) {
				WlCreateAndPushNamespace(b, ns.path[i].valueStr);
				depth++;
			}

			wlBindDeclarations(b, ns.body.statements);

			for (int i = 0; i < depth; i++) {
				WlPopScope(b);
			}
		} break;
		case WlKind_StImport: {
			WlSyntaxImport im = *(WlSyntaxImport *)(tk.valuePtr);

			WlBoundFunction *bf = arenaMalloc(sizeof(WlBoundFunction), &b->arena);

			WlSymbol *functionSymbol =
				wlPushSymbol(b, im.name.valueStr, WlBType_u0, WlSFlag_Function | WlSFlag_Constant | WlSFlag_Import);
			bf->symbol = functionSymbol;
			WlScope *s = WlCreateAndPushScope(b);
			functionSymbol->function = bf;
			bf->scope = s;
			int paramCount = 0;

			for (int i = 0; i < listLen(im.parameterList); i += 2) {
				paramCount++;
				WlToken paramToken = im.parameterList[i];
				WlSyntaxParameter param = *(WlSyntaxParameter *)paramToken.valuePtr;

				WlBType paramType = wlBindType(param.type);
				wlPushSymbol(b, param.name.valueStr, paramType, WlSFlag_Variable | WlSFlag_Constant);
			}

			WlPopScope(b);
			bf->paramCount = paramCount;

			listPush(&b->functions, bf);
		} break;
		case WlKind_StFunction: {
			wlBindFunction(b, tk);
		} break;
		case WlKind_Bad: break;
		default: PANIC("Unhandled declaration kind %s", WlKindText[tk.kind]); break;
		}
	}
}
