#include <parser.c>

typedef enum
{
	WlBType_u0,
	WlBType_bool,
	WlBType_u32,
	WlBType_i32,
	WlBType_u64,
	WlBType_i64,
	WlBType_f32,
	WlBType_f64,
	WlBType_str,
	WlBType_end,
	WlBType_unknown,
} WlBType;

char *WlBTypeText[] = {
	"u0", "bool", "u32", "i32", "u64", "i64", "f32", "f64", "str", "<>", "<>",
};

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
	WlBType currentReturnType;
	ArenaAllocator arena;
} WlBinder;

WlSymbol *wlFindSymbol(WlBinder *b, Str name, WlSymbolFlags flags, bool recurse);

WlSymbol *wlPushSymbol(WlBinder *b, Str name, WlBType type, WlSymbolFlags flags)
{
	WlSymbol *existing = wlFindSymbol(b, name, WlSFlag_None, false);
	if (existing) PANIC("variable by name %.*s already exists", existing->name);

	List(WlSymbol *) symbols = listPeek(&b->scopes)->symbols;

	WlSymbol *newSymbol = arenaMalloc(sizeof(WlSymbol), &b->arena);
	*newSymbol = (WlSymbol){.name = name, .type = type, .flags = flags, .index = 0};
	listPush(&symbols, newSymbol);
	listPeek(&b->scopes)->symbols = symbols;

	return newSymbol;
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

bool wlIsNumberType(WlBType t) { return (t >= WlBType_u32 && t <= WlBType_f64); }
bool wlIsFloatType(WlBType t) { return (t == WlBType_f32 || t == WlBType_f64); }

WlbNode wlBindExpression(WlBinder *b, WlToken expression, WlBType type)
{
	switch (expression.kind) {
	case WlKind_Number: {
		if (!wlIsNumberType(type)) PANIC("Expected type %d but got number literal", type);
		if (wlIsFloatType(type)) {
			// reinterpret the integer as a float
			return (WlbNode){.kind = WlBKind_NumberLiteral, .dataFloat = expression.valueNum, .type = type};
		} else {
			return (WlbNode){.kind = WlBKind_NumberLiteral, .dataNum = expression.valueNum, .type = type};
		}
	} break;
	case WlKind_FloatNumber: {
		if (!wlIsFloatType(type)) PANIC("Expected type %d but got float literal", type);
		return (WlbNode){.kind = WlBKind_NumberLiteral, .dataFloat = expression.valueFloat, .type = type};
	} break;
	case WlKind_String: {
		return (WlbNode){.kind = WlBKind_StringLiteral, .dataStr = expression.valueStr, .type = WlBType_str};
	} break;
	case WlKind_StBinaryExpression: {
		WlBinaryExpression ex = *(WlBinaryExpression *)expression.valuePtr;
		WlBoundBinaryExpression bex;
		bex.left = wlBindExpression(b, ex.left, type);
		bex.operator= wlBindOperator(ex.operator);
		bex.right = wlBindExpression(b, ex.right, type);

		// todo: get the proper return type based on the operator and operands

		WlBoundBinaryExpression *bexp = arenaMalloc(sizeof(WlBoundBinaryExpression), &b->arena);
		*bexp = bex;

		return (WlbNode){.kind = WlBKind_BinaryExpression, .data = bexp, .type = type};
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
			WlbNode arg = wlBindExpression(b, call.args[i * 2], function->function->scope->symbols[i]->type);
			listPush(&args, arg);
		}
		bcall.args = args;
		bcall.function = function;

		WlBoundCallExpression *bcallp = arenaMalloc(sizeof(WlBoundCallExpression), &b->arena);
		*bcallp = bcall;

		return (WlbNode){.kind = WlBKind_Call, .data = bcallp, .type = WlBType_u0};
	}

	case WlKind_StRef: {
		Str name = expression.valueStr;
		WlSymbol *variable = wlFindSymbol(b, name, WlSFlag_Variable, true);
		if (type != variable->type) {
			PANIC("Type mismatch! expected %d, got %d", type, variable->type);
		}

		if (!variable) PANIC("failed to find variable by name %.*s", STRPRINT(name));

		return (WlbNode){.kind = WlBKind_Ref, .data = variable, .type = variable->type};
	}

	default: PANIC("Unhandled expression kind %s", WlKindText[expression.kind]);
	}
}

WlbNode wlBindStatement(WlBinder *b, WlToken statement)
{
	switch (statement.kind) {
	case WlKind_StReturnStatement: {
		WlReturnStatement ret = *(WlReturnStatement *)statement.valuePtr;

		WlBoundReturn bret;

		if (ret.expression.kind != WlKind_Missing) {
			WlBType t = b->currentReturnType;
			bret.expression = wlBindExpression(b, ret.expression, t);
		} else {
			if (b->currentReturnType != WlBType_u0) {
				PANIC("Expected return to have expression because return type is not u0");
			}
			bret.expression = (WlbNode){.kind = WlBKind_None, .type = b->currentReturnType};
		}

		WlBoundReturn *bretp = arenaMalloc(sizeof(WlBoundReturn), &b->arena);
		*bretp = bret;
		return (WlbNode){.kind = WlBKind_Return, .data = bretp, .type = b->currentReturnType};
	} break;
	case WlKind_StExpressionStatement: {
		WlExpressionStatement ex = *(WlExpressionStatement *)statement.valuePtr;
		return wlBindExpression(b, ex.expression, WlBType_u0);
	} break;
	case WlKind_StVariableDeclaration: {
		WlSyntaxVariableDeclaration var = *(WlSyntaxVariableDeclaration *)statement.valuePtr;
		WlBoundVariable *bvar = arenaMalloc(sizeof(WlBoundVariable), &b->arena);

		WlBType type = wlBindType(var.type);
		bvar->symbol = wlPushSymbol(b, var.name.valueStr, type, WlSFlag_Variable);
		if (var.initializer.kind == WlKind_Missing) {
			bvar->initializer = (WlbNode){.kind = WlBKind_None};
		} else {
			bvar->initializer = wlBindExpression(b, var.initializer, type);
		}
		return (WlbNode){.kind = WlBKind_VariableDeclaration, .data = bvar, .type = type};
	} break;
	case WlKind_StVariableAssignement: {
		WlAssignmentExpression var = *(WlAssignmentExpression *)statement.valuePtr;
		WlBoundAssignment *bvar = arenaMalloc(sizeof(WlBoundAssignment), &b->arena);

		WlSymbol *variable = wlFindSymbol(b, var.variable.valueStr, WlSFlag_Variable, true);

		bvar->symbol = variable;
		bvar->expression = wlBindExpression(b, var.expression, variable->type);

		return (WlbNode){.kind = WlBKind_VariableAssignment, .data = bvar, .type = variable->type};
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

	return (WlbNode){.kind = WlBKind_Block, .type = WlBType_u0, .data = blk};
}

void bindPrint(WlBinder *b) {}

WlBinder wlBind(WlToken *declarations, int declarationCount)
{
	WlBinder b = {
		.unboundDeclarations = declarations,
		.unboundCount = declarationCount,
		.functions = listNew(),
		.scopes = listNew(),
		.arena = arenaCreate(),
	};

	//	 push the global scope
	WlCreateAndPushScope(&b);

	// hardcoded print extern for now
	bindPrint(&b);

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
		default: PANIC("Unhandled declaration kind %s", WlKindText[tk.kind]); break;
		}
	}

	return b;
}
