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
	WlBKind_Function
} WlBKind;

typedef struct {
	bool exported;
	WlBType returnType;
	Str name;
} WlBoundFunction;

typedef struct {
	WlToken *unboundDeclarations;
	int unboundCount;

	WlBoundFunction *functions;
	int functionCount;
} WlBinder;

WlBinder wlBind(WlToken *declarations, int declarationCount)
{
	WlBinder b = {
		.unboundDeclarations = declarations,
		.unboundCount = declarationCount,
		.functions = smalloc(sizeof(WlBoundFunction) * 0xFF),
		.functionCount = 0,
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
		};
	}

	return b;
}
