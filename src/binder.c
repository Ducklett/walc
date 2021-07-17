#include <parser.c>

typedef enum
{
	WlBType_start,
	WlBType_u0,
	WlBType_u32,
	WlBType_i32,
	WlBType_u64,
	WlBType_i64,
	WlBType_f32,
	WlBType_f64,
	WlBType_end,
	WlBType_unknown,
} WlBType;

char *WlBTypeText[] = {
	"<>", "u0", "u32", "i32", "u64", "i64", "f32", "f64", "<>", "<>",
};

WlBType wlBindType(WlToken tk)
{
	assert(tk.kind == WlKind_Symbol);

	for (int i = WlBType_start + 1; i < WlBType_end; i++) {
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

typedef struct {
	WlBKind kind;
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
	WlbNode *nodes;
	int nodeCount;
} WlBoundBlock;

typedef struct {
	WlbNode expression;
} WlBoundReturn;

typedef struct {
	bool exported;
	WlBType returnType;
	Str name;
	WlbNode body;
} WlBoundFunction;

typedef struct {
	WlToken *unboundDeclarations;
	int unboundCount;

	WlBoundFunction *functions;
	int functionCount;
	ArenaAllocator arena;
} WlBinder;

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

WlbNode wlBindExpression(WlBinder *b, WlToken expression)
{
	switch (expression.kind) {
	case WlKind_Number: {
		return (WlbNode){.kind = WlBKind_NumberLiteral, .dataNum = expression.valueNum};
	}
	case WlKind_StBinaryExpression: {
		WlBinaryExpression ex = *(WlBinaryExpression *)expression.valuePtr;
		WlBoundBinaryExpression bex;
		bex.left = wlBindExpression(b, ex.left);
		bex.operator= wlBindOperator(ex.operator);
		bex.right = wlBindExpression(b, ex.right);

		WlBoundBinaryExpression *bexp = arenaMalloc(sizeof(WlBoundBinaryExpression), &b->arena);
		*bexp = bex;

		return (WlbNode){.kind = WlBKind_BinaryExpression, .data = bexp};
	}
	case WlKind_StCall: {
		WlSyntaxCall call = *(WlSyntaxCall *)expression.valuePtr;

		WlBoundCallExpression bcall = {0};
		if (call.arg.kind == WlKind_String) {
			bcall.arg = (WlbNode){.kind = WlBKind_StringLiteral, .dataStr = call.arg.valueStr};
		} else {
			bcall.arg = (WlbNode){.kind = WlKind_Missing, .data = NULL};
		}

		WlBoundCallExpression *bcallp = arenaMalloc(sizeof(WlBoundCallExpression), &b->arena);
		*bcallp = bcall;

		return (WlbNode){.kind = WlBKind_Call, .data = bcallp};
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

		// TODO: match return expression type with function type
		if (ret.expression.kind != WlKind_Missing) {
			bret.expression = wlBindExpression(b, ret.expression);
		} else {
			bret.expression = (WlbNode){.kind = WlBKind_None};
		}

		WlBoundReturn *bretp = arenaMalloc(sizeof(WlBoundReturn), &b->arena);
		*bretp = bret;
		return (WlbNode){.kind = WlBKind_Return, .data = bretp};
	} break;
	case WlKind_StExpressionStatement: {
		WlExpressionStatement ex = *(WlExpressionStatement *)statement.valuePtr;
		return wlBindExpression(b, ex.expression);
	} break;
	default: PANIC("Unhandled statement kind %s", WlKindText[statement.kind]);
	}
}

WlbNode wlBindBlock(WlBinder *b, WlSyntaxBlock body)
{
	WlBoundBlock blk = {0};
	blk.nodeCount = body.statementCount;
	size_t foo = sizeof(WlbNode) * blk.nodeCount;
	WlbNode *nodes = arenaMalloc(foo, &b->arena);
	for (int i = 0; i < body.statementCount; i++) {
		nodes[i] = wlBindStatement(b, body.statements[i]);
	}
	blk.nodes = nodes;
	WlBoundBlock *blkp = arenaMalloc(sizeof(WlBoundBlock), &b->arena);
	*blkp = blk;

	return (WlbNode){.kind = WlBKind_Block, .data = blkp};
}

WlBinder wlBind(WlToken *declarations, int declarationCount)
{
	WlBinder b = {
		.unboundDeclarations = declarations,
		.unboundCount = declarationCount,
		.functions = smalloc(sizeof(WlBoundFunction) * 0xFF),
		.functionCount = 0,
		.arena = arenaCreate(),
	};

	for (int i = 0; i < declarationCount; i++) {
		WlToken tk = declarations[i];
		assert(tk.kind == WlKind_StFunction);

		WlSyntaxFunction fn = *(WlSyntaxFunction *)(tk.valuePtr);

		if (b.functionCount >= 0xFF) PANIC("Too many bound functions");

		b.functions[b.functionCount++] = (WlBoundFunction){
			.exported = fn.export.kind == WlKind_KwExport,
			.returnType = wlBindType(fn.type),
			.name = fn.name.valueStr,
			.body = wlBindBlock(&b, fn.body),
		};
	}

	return b;
}
