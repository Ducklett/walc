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
	assert(tk.kind == WlKind_Symbol);

	for (int i = 0; i < WlBType_end; i++) {
		Str fuck = strFromCstr(WlBTypeText[i]);
		// TODO: cache Str
		if (strEqual(tk.valueStr, fuck)) {
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
	WlSFlag_Constant = 1,
} WlSymbolFlags;

typedef struct {
	int index;
	Str name;
	WlBType type;
	WlSymbolFlags flags;
} WlSymbol;

typedef struct {
	List(WlSymbol) symbols;
} WlScope;

typedef struct {
	WlBKind kind;
	WlBType type;
	union {
		void *data;
		Str dataStr;
		int dataNum;
	};
} WlbNode;

typedef struct {
	WlbNode arg;
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

typedef struct {
	WlScope *scope;
	bool exported;
	int paramCount;
	WlBType returnType;
	Str name;
	WlbNode body;
} WlBoundFunction;

typedef struct {
	WlToken *unboundDeclarations;
	int unboundCount;

	List(WlBoundFunction) functions;
	ArenaAllocator arena;
	WlScope *currentScope;
	WlBType currentReturnType;
} WlBinder;

WlSymbol *wlFindVariable(WlBinder *b, Str name);

WlSymbol *wlPushVariable(WlBinder *b, Str name, WlBType type)
{
	WlSymbol *existing = wlFindVariable(b, name);
	if (existing) PANIC("variable by name %.*s already exists");

	List(WlSymbol) symbols = b->currentScope->symbols;
	WlSymbol newSymbol = (WlSymbol){.name = name, .type = type, .flags = WlSFlag_None, .index = listLen(symbols)};
	listPush(&symbols, newSymbol);
	b->currentScope->symbols = symbols;
}

WlSymbol *wlFindVariable(WlBinder *b, Str name)
{
	WlSymbol *found = NULL;

	WlScope *s = b->currentScope;
	for (int i = 0; i < listLen(s->symbols); i++) {
		if (strEqual(name, s->symbols[i].name)) {
			found = s->symbols + i;
			break;
		}
	}

	return found;
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
		return (WlbNode){.kind = WlBKind_NumberLiteral, .dataNum = expression.valueNum, .type = type};
	}
	case WlKind_FloatNumber: {
		if (!wlIsFloatType(type)) PANIC("Expected type %d but got float literal", type);
		return (WlbNode){.kind = WlBKind_NumberLiteral, .dataNum = expression.valueNum, .type = type};
	}
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
		if (call.arg.kind == WlKind_String) {
			bcall.arg = (WlbNode){.kind = WlBKind_StringLiteral, .dataStr = call.arg.valueStr, .type = WlBType_str};
		} else {
			bcall.arg = (WlbNode){.kind = WlKind_Missing, .data = NULL, .type = WlBType_unknown};
		}

		WlBoundCallExpression *bcallp = arenaMalloc(sizeof(WlBoundCallExpression), &b->arena);
		*bcallp = bcall;

		return (WlbNode){.kind = WlBKind_Call, .data = bcallp, .type = WlBType_u0};
	}

	case WlKind_StRef: {
		Str name = expression.valueStr;
		// NOTE: the reference will be lost when the list reallocates
		// we should either store symbol pointers and allocate symbols in the arena
		// or determine the amount of symbols in the scope ahead of time
		WlSymbol *variable = wlFindVariable(b, name);
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
	default: PANIC("Unhandled statement kind %s", WlKindText[statement.kind]);
	}
}

WlScope *WlCreateScope(WlBinder *b)
{
	WlScope *scope = arenaMalloc(sizeof(WlScope), &b->arena);
	*scope = (WlScope){.symbols = listNew()};
	b->currentScope = scope;
	return scope;
};

WlbNode wlBindBlock(WlBinder *b, WlSyntaxBlock body, bool createScope)
{
	WlBoundBlock *blk = arenaMalloc(sizeof(WlBoundBlock), &b->arena);
	*blk = (WlBoundBlock){0};
	if (createScope) {
		blk->scope = WlCreateScope(b);
		b->currentScope = blk->scope;
	} else {
		blk->scope = b->currentScope;
	}

	blk->nodeCount = body.statementCount;
	size_t foo = sizeof(WlbNode) * blk->nodeCount;
	WlbNode *nodes = arenaMalloc(foo, &b->arena);
	for (int i = 0; i < body.statementCount; i++) {
		nodes[i] = wlBindStatement(b, body.statements[i]);
	}
	blk->nodes = nodes;

	return (WlbNode){.kind = WlBKind_Block, .type = WlBType_u0, .data = blk};
}

WlBinder wlBind(WlToken *declarations, int declarationCount)
{
	WlBinder b = {
		.unboundDeclarations = declarations,
		.unboundCount = declarationCount,
		.functions = listNew(),
		.arena = arenaCreate(),
	};

	for (int i = 0; i < declarationCount; i++) {
		WlToken tk = declarations[i];
		assert(tk.kind == WlKind_StFunction);

		WlSyntaxFunction fn = *(WlSyntaxFunction *)(tk.valuePtr);

		WlScope *s = WlCreateScope(&b);

		int paramCount = 0;
		for (int i = 0; i < listLen(fn.parameterList); i += 2) {
			paramCount++;
			WlToken paramToken = fn.parameterList[i];
			WlSyntaxParameter param = *(WlSyntaxParameter *)paramToken.valuePtr;

			WlBType paramType = wlBindType(param.type);
			wlPushVariable(&b, param.name.valueStr, paramType);
		}

		WlBType returnType = wlBindType(fn.type);
		b.currentReturnType = returnType;

		WlBoundFunction bf = {
			.scope = s,
			.paramCount = paramCount,
			.exported = fn.export.kind == WlKind_KwExport,
			.returnType = returnType,
			.name = fn.name.valueStr,
			.body = wlBindBlock(&b, fn.body, false),
		};
		listPush(&b.functions, bf);
	}

	return b;
}
