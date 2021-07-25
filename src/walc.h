#ifndef WALC_H
#define WALC_H

#include <sti_base.h>

typedef enum WlKind
{
	WlKind_Bad,
	WlKind_EOF,
	WlKind_Missing,
	WlKind_Symbol,
	WlKind_Number,
	WlKind_FloatNumber,
	WlKind_String,

	WlKind_BinaryOperators_start,

	WlKind_OpPlus,
	WlKind_OpMinus,
	WlKind_OpStar,
	WlKind_OpSlash,
	WlKind_OpPercent,
	WlKind_OpLessLess,
	WlKind_OpGreaterGreater,
	WlKind_OpGreater,
	WlKind_OpGreaterEquals,
	WlKind_OpLess,
	WlKind_OpLessEquals,
	WlKind_OpEuqualsEquals,
	WlKind_OpBangEquals,
	WlKind_OpAmpersand,
	WlKind_OpCaret,
	WlKind_OpPipe,
	WlKind_OpAmpersandAmpersand,
	WlKind_OpPipePipe,

	WlKind_BinaryOperators_End,

	WlKind_OpEquals,

	WlKind_TkParenOpen,
	WlKind_TkParenClose,
	WlKind_TkCurlyOpen,
	WlKind_TkCurlyClose,
	WlKind_TkBracketOpen,
	WlKind_TkBracketClose,
	WlKind_TkComma,
	WlKind_TkSemicolon,

	WlKind_Keywords_Start,
	WlKind_KwTrue,
	WlKind_KwFalse,
	WlKind_KwIf,
	WlKind_KwElse,
	WlKind_KwSwitch,
	WlKind_KwCase,
	WlKind_KwDefault,
	WlKind_KwWhile,
	WlKind_KwFor,
	WlKind_KwIn,
	WlKind_KwOut,
	WlKind_KwInout,
	WlKind_KwBreak,
	WlKind_KwContinue,
	WlKind_KwNamespace,
	WlKind_KwEnum,
	WlKind_KwStruct,
	WlKind_KwType,
	WlKind_KwReturn,
	WlKind_KwImport,
	WlKind_KwExport,
	WlKind_Keywords_End,

	WlKind_Syntax_Start,
	WlKind_StFunction,
	WlKind_StFunctionParameter,
	WlKind_StVariableDeclaration,
	WlKind_StVariableAssignement,
	WlKind_StExpressionStatement,
	WlKind_StReturnStatement,
	WlKind_StCall,
	WlKind_StRef,
	WlKind_StBinaryExpression,
	WlKind_StType,
	WlKind_StBlock,
	WlKind_StExpression,
	WlKind_StImport,
	WlKind_Syntax_End,
} WlKind;
char *WlKindText[] = {
	"<bad>",
	"<EOF>",
	"<missing>",
	"<symbol>",
	"<number>",
	"<floatnumber>",
	"<string>",
	"<binary start>",
	"+",
	"-",
	"*",
	"/",
	"%",
	"<<",
	">>",
	">",
	">=",
	"<",
	"<=",
	"==",
	"!=",
	"&",
	"^",
	"|",
	"&&",
	"||",
	"<binary end>",
	"=",
	"(",
	")",
	"{",
	"}",
	"[",
	"]",
	",",
	";",
	"<keywords start>",
	"true",
	"false",
	"if",
	"else",
	"switch",
	"case",
	"default",
	"while",
	"for",
	"in",
	"out",
	"inout",
	"break",
	"continue",
	"namespace",
	"enum",
	"struct",
	"type",
	"return",
	"import",
	"export",
	"<keywords end>",

	"<syntax start>",
	"WlKind_StFunction",
	"WlKind_StFunctionParameter",
	"WlKind_StVariableDeclaration",
	"WlKind_StVariableAssignement",
	"WlKind_StExpressionStatement",
	"WlKind_StReturnStatement",
	"WlKind_StCall",
	"WlKind_StRef",
	"WlKind_StBinaryExpression",
	"WlKind_StType",
	"WlKind_StBlock",
	"WlKind_StExpression",
	"WlKind_Stimport",
	"<syntax end>",
};

bool isDigit(char c);
bool isBinaryDigit(char c);
bool isHexDigit(char c);
bool isLowercaseLetter(char c);
bool isUppercaseLetter(char c);
bool isLetter(char c);
bool isCompilerReserved(char c);
bool isSymbol(char c);
bool isSymbolStart(char c);

typedef enum
{
	WlBType_u0,
	WlBType_bool,
	WlBType_u8,
	WlBType_i8,
	WlBType_u16,
	WlBType_i16,
	WlBType_u32,
	WlBType_i32,
	WlBType_u64,
	WlBType_i64,
	WlBType_f32,
	WlBType_f64,
	WlBType_str,
	WlBType_end,
	WlBType_unknown,
	WlBType_integerNumber,
	WlBType_floatingNumber,
	// infer type but always return a real type
	WlBType_inferStrong,
	// infer type and allow for partially unresolved types (number, floating)
	WlBType_inferWeak,
} WlBType;

char *WlBTypeText[] = {
	"u0", "bool", "u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f32", "f64", "str", "<>", "<>", "<infer>",
};

#define SPANEMPTY ((WlSpan){0})
typedef struct WlSpan {
	Str filename;
	Str source;
	size_t start;
	size_t len;
} WlSpan;

typedef enum
{
	UnterminatedCommentDiagnostic,
	UnexpectedCharacterDiagnostic,
	UnterminatedStringDiagnostic,
	UnexpectedTokenDiagnostic,
	UnexpectedTokenInPrimaryExpressionDiagnostic,
	VariableAlreadyExistsDiagnostic,
	VariableNotFoundDiagnostic,
	CannotImplicitlyConvertDiagnostic,
} WlDiagnosticKind;

typedef struct {
	WlDiagnosticKind kind;
	WlSpan span;
	union {
		double float1;
		void *pointer1;
		int num1;
		WlKind kind1;
		Str str1;
	};
	union {
		double float2;
		void *pointer2;
		int num2;
		WlKind kind2;
		Str str2;
	};
} WlDiagnostic;

typedef struct {
	WlKind kind;
	WlSpan span;
	union {
		i64 valueNum;
		f64 valueFloat;
		Str valueStr;
		void *valuePtr;
	};
} WlToken;

#include <diagnostics.c>

#include <parser.c>

#include <binder.c>

#include <wasmEmitter.c>

#endif // WALC_H
