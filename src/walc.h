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
	WlKind_OpDoubleEquals,
	WlKind_OpBangEquals,
	WlKind_BinaryOperators_End,

	WlKind_OpEquals,

	WlKind_TkParenOpen,
	WlKind_TkParenClose,
	WlKind_TkCurlyOpen,
	WlKind_TkCurlyClose,
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
	"==",
	"!=",
	"<binary end>",
	"=",
	"(",
	")",
	"{",
	"}",
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

bool isNewline(char c);
bool isEndOfLine(char c);
bool isWhitespace(char c);
bool isDigit(char c);
bool isBinaryDigit(char c);
bool isHexDigit(char c);
bool isLowercaseLetter(char c);
bool isUppercaseLetter(char c);
bool isLetter(char c);
bool isCompilerReserved(char c);
bool isSymbol(char c);
bool isSymbolStart(char c);

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
} WlDiagnosticKind;

typedef struct {
	WlDiagnosticKind kind;
	WlSpan span;
	union {
		double float1;
		void *pointer1;
		int num1;
		WlKind kind1;
	};
	union {
		double float2;
		void *pointer2;
		int num2;
		WlKind kind2;
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
#include <wasmEmitter.c>

#endif // WALC_H
