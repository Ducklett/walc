#include <sti_base.h>

typedef enum
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
	WlKind_KwExtern,
	WlKind_KwExport,
	WlKind_Keywords_End,

	WlKind_Syntax_Start,
	WlKind_StFunction,
	WlKind_StFunctionParameter,
	WlKind_StExpressionStatement,
	WlKind_StReturnStatement,
	WlKind_StCall,
	WlKind_StRef,
	WlKind_StBinaryExpression,
	WlKind_StType,
	WlKind_StBlock,
	WlKind_StExpression,
	WlKind_StExtern,
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
	"<binary end>",
	"=",
	"!=",
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
	"extern",
	"export",
	"<keywords end>",

	"<syntax start>",
	"WlKind_StFunction",
	"WlKind_StFunctionParameter",
	"WlKind_StExpressionStatement",
	"WlKind_StReturnStatement",
	"WlKind_StCall",
	"WlKind_StRef",
	"WlKind_StBinaryExpression",
	"WlKind_StType",
	"WlKind_StBlock",
	"WlKind_StExpression",
	"WlKind_StExtern",
	"<syntax end>",
};

typedef struct {
	WlKind kind;
	union {
		i64 valueNum;
		f64 valueFloat;
		Str valueStr;
		void *valuePtr;
	};
} WlToken;

typedef struct {
	Str source;
	int index;
} WlLexer;

bool isNewline(char c) { return c == '\r' || c == '\n'; }
bool isEndOfLine(char c) { return c == '\0' || c == '\r' || c == '\n'; }
bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isBinaryDigit(char c) { return c == '0' || c == '1'; }
bool isHexDigit(char c) { return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
bool isLowercaseLetter(char c) { return c >= 'a' && c <= 'z'; }
bool isUppercaseLetter(char c) { return c >= 'A' && c <= 'Z'; }
bool isLetter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool isCompilerReserved(char c)
{
	return (c >= 0 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 94) || (c >= 123 && c <= 127) || (c == 255);
}
bool isSymbol(char c) { return !isCompilerReserved(c); }
bool isSymbolStart(char c) { return !isDigit(c) && !isCompilerReserved(c); }

WlLexer wlLexerCreate(Str source)
{
	// TODO: make dynamic
	WlToken *tokens = malloc(sizeof(WlToken) * 0xFF);

	return (WlLexer){
		.source = source,
		.index = 0,
	};
}

void wlLexerFree(WlLexer *l) {}

char wlLexerCurrent(WlLexer *l) { return l->index < l->source.len ? l->source.buf[l->index] : '\0'; }
char wlLexerLookahead(WlLexer *l, int n) { return l->index + n < l->source.len ? l->source.buf[l->index + n] : '\0'; }

WlToken wlLexerBasic(WlLexer *l, int len, WlKind kind)
{
	l->index += len;
	return (WlToken){.kind = kind};
}

void wlLexerTrim(WlLexer *l)
{
	while (isWhitespace(wlLexerCurrent(l)))
		l->index++;
}

WlToken wlLexerLexToken(WlLexer *l)
{
lexStart:
	wlLexerTrim(l);

	// skip single line comments
	if (wlLexerCurrent(l) == '/' && wlLexerLookahead(l, 1) == '/') {
		while (!isEndOfLine(wlLexerCurrent(l))) {
			l->index++;
		}
		while (isNewline(wlLexerCurrent(l))) {
			l->index++;
		}
		goto lexStart;
	}
	// skip multi line comments
	if (wlLexerCurrent(l) == '/' && wlLexerLookahead(l, 1) == '*') {
		int level = 0;
		l->index += 2;
		while (true) {
			if (wlLexerCurrent(l) == '\0') PANIC("Unexpected EOF");

			if (wlLexerCurrent(l) == '/' && wlLexerLookahead(l, 1) == '*') {
				level++;
				l->index += 2;
			} else if (wlLexerCurrent(l) == '*' && wlLexerLookahead(l, 1) == '/') {
				level--;
				l->index += 2;
				if (level == -1) {
					break;
				}
			} else {
				l->index += 1;
			}
		}
		goto lexStart;
	}

	char current = wlLexerCurrent(l);

	if (!current) return wlLexerBasic(l, 0, WlKind_EOF);

	switch (current) {
	case '+': return wlLexerBasic(l, 1, WlKind_OpPlus);
	case '-': return wlLexerBasic(l, 1, WlKind_OpMinus); break;
	case '*': return wlLexerBasic(l, 1, WlKind_OpStar); break;
	case '%': return wlLexerBasic(l, 1, WlKind_OpPercent); break;
	case '/': return wlLexerBasic(l, 1, WlKind_OpSlash); break;
	case '=':
		if (wlLexerLookahead(l, 1) == '=')
			return wlLexerBasic(l, 2, WlKind_OpDoubleEquals);
		else
			return wlLexerBasic(l, 2, WlKind_OpEquals);
		break;
	case '!':
		if (wlLexerLookahead(l, 1) == '=') {
			return wlLexerBasic(l, 2, WlKind_OpBangEquals);
		} else {
			goto unmatched;
		}
		break;
	case '(': return wlLexerBasic(l, 1, WlKind_TkParenOpen); break;
	case ')': return wlLexerBasic(l, 1, WlKind_TkParenClose); break;
	case '{': return wlLexerBasic(l, 1, WlKind_TkCurlyOpen); break;
	case '}': return wlLexerBasic(l, 1, WlKind_TkCurlyClose); break;
	case ',': return wlLexerBasic(l, 1, WlKind_TkComma); break;
	case ';': return wlLexerBasic(l, 1, WlKind_TkSemicolon); break;
	case '"': {
		l->index++;
		int start = l->index;

		while (wlLexerCurrent(l) != '\0' && wlLexerCurrent(l) != '"')
			l->index++;

		if (wlLexerCurrent(l) == '\0') {
			PANIC("unexpected EOF, expected '\"'");
		}
		Str value = strSlice(l->source, start, l->index - start);
		l->index++;

		return (WlToken){.kind = WlKind_String, .valueStr = value};
	} break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9': {
		int start = l->index;
		int value = 0;
		if (current == '0') {
			switch (wlLexerLookahead(l, 1)) {
			case 'x': {
				l->index += 2;
				while (isHexDigit(wlLexerCurrent(l))) {
					char d = wlLexerCurrent(l);
					u8 digitValue;
					if (isLowercaseLetter(d))
						digitValue = d - 'a' + 10;
					else if (isUppercaseLetter(d))
						digitValue = d - 'A' + 10;
					else
						digitValue = d - '0';

					value <<= 4;
					value += digitValue;
					l->index++;
				}
			}
			case 'b': {
				l->index += 2;
				while (isBinaryDigit(wlLexerCurrent(l))) {
					u8 digitValue = wlLexerCurrent(l) - '0';
					value <<= 1;
					value += digitValue;
					l->index++;
				}
			}
			default: {
				// matched 0
				l->index++;
				goto floatTest;
			}
			}

		} else {
			while (isDigit(wlLexerCurrent(l))) {
				u8 digitValue = wlLexerCurrent(l) - '0';
				value *= 10;
				value += digitValue;
				l->index++;
			}
		floatTest:
			if (wlLexerCurrent(l) == '.') {
				l->index++;
				int floatDivisor = 1;
				int floatValue;
				while (isDigit(wlLexerCurrent(l))) {
					u8 digitValue = wlLexerCurrent(l) - '0';
					floatValue *= 10;
					floatValue += digitValue;
					l->index++;
					floatDivisor *= 10;
				}
				f64 finalValue = value + (floatValue / (f64)floatDivisor);
				return (WlToken){.kind = WlKind_FloatNumber, .valueFloat = finalValue};
			}
		}
		return (WlToken){.kind = WlKind_Number, .valueNum = value};
	} break;
	default: {
	unmatched : {
	}
		if (isLowercaseLetter(current)) {
			for (int i = WlKind_Keywords_Start + 1; i < WlKind_Keywords_End; i++) {
				// TODO: bake into Str's beforehand so we don't have to convert from cstr
				Str keyword = strFromCstr(WlKindText[i]);
				if (strEqual(strSlice(l->source, l->index, keyword.len), keyword)) {
					l->index += keyword.len;
					return (WlToken){.kind = i};
					break;
				}
			}
		}

		if (isSymbolStart(current)) {
			int start = l->index;
			while (isSymbol(wlLexerCurrent(l)))
				l->index++;
			Str symbolName = strSlice(l->source, start, l->index - start);
			return (WlToken){.kind = WlKind_Symbol, .valueStr = symbolName};
		} else {
			l->index++;
			// PANIC("unexpected character '%c'", current);
			return (WlToken){.kind = WlKind_Bad};
		}
	} break;
	}
}

// lexes all tokens and returns them as a list
// the EOF token is not returned
List(WlToken) wlLexerLexTokens(WlLexer *l)
{
	WlToken t;
	List(WlToken) tokens = listNew();

	while (true) {
		t = wlLexerLexToken(l);
		if (t.kind == WlKind_EOF) break;
		listPush(&tokens, t);
	}

	return tokens;
}

typedef struct {
	WlToken expression;
	WlToken semicolon;
} WlExpressionStatement;

typedef struct {
	WlToken returnKeyword;
	WlToken expression;
	WlToken semicolon;
} WlReturnStatement;

typedef struct {
	WlToken left;
	WlToken operator;
	WlToken right;
} WlBinaryExpression;

typedef struct {
	WlToken name;
	WlToken parenOpen;
	WlToken arg;
	WlToken parenClose;
} WlSyntaxCall;

typedef struct {
	WlToken curlyOpen;
	WlToken *statements;
	int statementCount;
	WlToken curlyClose;
} WlSyntaxBlock;

typedef struct {
	WlToken type;
	WlToken name;
} WlSyntaxParameter;

typedef struct {
	WlToken export;
	WlToken type;
	WlToken name;
	WlToken parenOpen;
	List(WlToken) parameterList;
	WlToken parenClose;
	WlSyntaxBlock body;
} WlSyntaxFunction;

typedef struct {
	WlLexer lexer;
	WlToken *topLevelDeclarations;
	int topLevelCount;
	ArenaAllocator arena;
	WlToken current;
	bool hasToken;
} WlParser;

WlParser wlParserCreate(Str source)
{
	WlLexer l = wlLexerCreate(source);
	return (WlParser){
		.lexer = l,
		.topLevelDeclarations = malloc(sizeof(WlToken) * 0xFF),
		.arena = arenaCreate(),
		.hasToken = false,
	};
}

WlToken wlParserPeek(WlParser *p)
{
	if (p->hasToken) return p->current;
	p->hasToken = true;
	p->current = wlLexerLexToken(&p->lexer);
	return p->current;
}

WlToken wlParserTake(WlParser *p)
{
	WlToken t = p->hasToken ? p->current : wlLexerLexToken(&p->lexer);
	p->hasToken = false;
	return t;
}

WlToken wlParserMatch_impl(WlParser *p, WlKind kind, const char *file, int line)
{
	WlToken t = wlParserTake(p);

	if (t.kind != kind) {
		printf("unexpected token %s, expected %s\n", WlKindText[t.kind], WlKindText[kind]);
		todo_impl("Report diagnostic on mismatched token", file, line);
	}
	return t;
}
#define wlParserMatch(p, k) wlParserMatch_impl(p, k, __FILE__, __LINE__)

void wlParserAddTopLevelStatement(WlParser *p, WlToken tk)
{
	if (p->topLevelCount > 0xFF) TODO("Make top level count growable");
	p->topLevelDeclarations[p->topLevelCount++] = tk;
}

WlToken wlParseParameter(WlParser *p)
{
	WlSyntaxParameter param;
	param.type = wlParserMatch(p, WlKind_Symbol);
	param.name = wlParserMatch(p, WlKind_Symbol);

	WlSyntaxParameter *paramp = arenaMalloc(sizeof(WlSyntaxParameter), &p->arena);
	*paramp = param;
	return (WlToken){.kind = WlKind_StFunctionParameter, .valuePtr = paramp};
}

List(WlToken) wlParseParameterList(WlParser *p)
{
	List(WlToken) list = listNew();

	while (true) {
		if (wlParserPeek(p).kind != WlKind_Symbol) break;
		WlToken param = wlParseParameter(p);
		listPush(&list, param);

		if (wlParserPeek(p).kind != WlKind_TkComma) break;
		WlToken delim = wlParserMatch(p, WlKind_TkComma);
		listPush(&list, delim);
	}
	return list;
}

WlToken wlParsePrimaryExpression(WlParser *p)
{
	switch (wlParserPeek(p).kind) {
	case WlKind_Number: return wlParserTake(p);
	case WlKind_FloatNumber: return wlParserTake(p);
	case WlKind_Symbol: {
		WlToken symbol = wlParserMatch(p, WlKind_Symbol);

		if (wlParserPeek(p).kind == WlKind_TkParenOpen) {

			WlSyntaxCall call = {0};
			call.name = symbol;
			call.parenOpen = wlParserMatch(p, WlKind_TkParenOpen);
			if (wlParserPeek(p).kind != WlKind_TkParenClose) {
				call.arg = wlParserMatch(p, WlKind_String);
			} else {
				call.arg = (WlToken){.kind = WlKind_Missing};
			}
			call.parenClose = wlParserMatch(p, WlKind_TkParenClose);

			WlSyntaxCall *callp = arenaMalloc(sizeof(WlSyntaxCall), &p->arena);
			*callp = call;

			return (WlToken){.kind = WlKind_StCall, .valuePtr = callp};
		} else {
			symbol.kind = WlKind_StRef;
			return symbol;
		}
	}
	default: PANIC("unexpected token kind %s", WlKindText[wlParserPeek(p).kind]); break;
	}
}

bool isBinaryOperator(WlKind k) { return k > WlKind_BinaryOperators_start && k < WlKind_BinaryOperators_End; }
int operatorPrecedence(WlKind operator)
{
	switch (operator) {
	case WlKind_OpPlus: return 12;
	case WlKind_OpMinus: return 12;
	case WlKind_OpStar: return 13;
	case WlKind_OpSlash: return 13;
	case WlKind_OpPercent: return 13;
	case WlKind_OpBangEquals: return 9;
	default: return -1;
	}
}

WlToken wlParseBinaryExpression(WlParser *p, int previousPrecedence)
{
	WlToken left = wlParsePrimaryExpression(p);
	while (isBinaryOperator(wlParserPeek(p).kind)) {
		WlToken operator= wlParserPeek(p);
		int precedence = operatorPrecedence(operator.kind);

		if (precedence <= previousPrecedence) {
			return left;
		}

		operator= wlParserTake(p);

		WlToken right = wlParseBinaryExpression(p, precedence);

		WlBinaryExpression *exprData = arenaMalloc(sizeof(WlBinaryExpression), &p->arena);
		*exprData = (WlBinaryExpression){.left = left, .operator= operator, .right = right };

		WlToken binaryExpression = {.kind = WlKind_StBinaryExpression, .valuePtr = exprData};
		left = binaryExpression;
	}

	return left;
}
WlToken wlParseFullBinaryExpression(WlParser *p) { return wlParseBinaryExpression(p, -1); }

WlToken wlParseExpression(WlParser *p) { return wlParseFullBinaryExpression(p); }

WlToken wlParseStatement(WlParser *p)
{
	if (wlParserPeek(p).kind == WlKind_KwReturn) {
		WlReturnStatement st = {0};

		st.returnKeyword = wlParserTake(p);
		if (wlParserPeek(p).kind == WlKind_TkSemicolon) {
			st.expression = (WlToken){WlKind_Missing};
		} else {
			st.expression = wlParseExpression(p);
		}
		st.semicolon = wlParserMatch(p, WlKind_TkSemicolon);

		WlReturnStatement *stp = arenaMalloc(sizeof(WlReturnStatement), &p->arena);
		*stp = st;

		return (WlToken){.kind = WlKind_StReturnStatement, .valuePtr = stp};
	} else {
		WlToken expression = wlParseExpression(p);
		if (wlParserPeek(p).kind == WlKind_TkCurlyClose) {
			// implicit return statement
			WlReturnStatement st = {0};
			st.returnKeyword = (WlToken){.kind = WlKind_Missing};
			st.expression = expression;
			st.semicolon = (WlToken){.kind = WlKind_Missing};
			WlReturnStatement *stp = arenaMalloc(sizeof(WlReturnStatement), &p->arena);
			*stp = st;
			return (WlToken){.kind = WlKind_StReturnStatement, .valuePtr = stp};
		} else {
			WlExpressionStatement st = {0};
			st.expression = expression;
			st.semicolon = wlParserMatch(p, WlKind_TkSemicolon);
			WlExpressionStatement *stp = arenaMalloc(sizeof(WlExpressionStatement), &p->arena);
			*stp = st;
			return (WlToken){.kind = WlKind_StExpressionStatement, .valuePtr = stp};
		}
	}
}

WlSyntaxBlock wlParseBlock(WlParser *p)
{
	WlSyntaxBlock blk = {0};
	blk.curlyOpen = wlParserMatch(p, WlKind_TkCurlyOpen);
	blk.statements = malloc(sizeof(WlToken) * 0xFF);
	while (wlParserPeek(p).kind != WlKind_TkCurlyClose) {
		blk.statements[blk.statementCount++] = wlParseStatement(p);
	}

	blk.curlyClose = wlParserMatch(p, WlKind_TkCurlyClose);
	return blk;
}

void wlParseFunction(WlParser *p)
{
	WlSyntaxFunction fn = {0};

	if (wlParserPeek(p).kind == WlKind_KwExport) {
		fn.export = wlParserMatch(p, WlKind_KwExport);
	}

	fn.type = wlParserMatch(p, WlKind_Symbol);
	fn.name = wlParserMatch(p, WlKind_Symbol);
	fn.parenOpen = wlParserMatch(p, WlKind_TkParenOpen);
	fn.parameterList = wlParseParameterList(p);
	fn.parenClose = wlParserMatch(p, WlKind_TkParenClose);
	fn.body = wlParseBlock(p);

	WlSyntaxFunction *fnp = arenaMalloc(sizeof(WlSyntaxFunction), &p->arena);
	*fnp = fn;
	wlParserAddTopLevelStatement(p, (WlToken){.kind = WlKind_StFunction, .valuePtr = fnp});
}

void wlParse(WlParser *p)
{
	while (wlParserPeek(p).kind != WlKind_EOF) {
		wlParseFunction(p);
	}
}

WlParser wlParserFree(WlParser *p)
{
	arenaFree(&p->arena);
	wlLexerFree(&p->lexer);
	free(p->topLevelDeclarations);
}

void wlPrint(WlToken tk);

void wlPrintBlock(WlSyntaxBlock blk)
{
	wlPrint(blk.curlyOpen);
	printf("\n");
	for (int i = 0; i < blk.statementCount; i++) {
		WlToken st = blk.statements[i];
		wlPrint(st);
		printf("\n");
	}
	wlPrint(blk.curlyClose);
	printf("\n");
}

void wlPrint(WlToken tk)
{
	if (tk.kind < WlKind_Syntax_Start) {
		if (tk.kind == WlKind_String) {
			printf("%s::\"%.*s\" ", WlKindText[tk.kind], STRPRINT(tk.valueStr));
		} else if (tk.kind == WlKind_Symbol) {
			printf("%s::%.*s ", WlKindText[tk.kind], STRPRINT(tk.valueStr));
		} else if (tk.kind == WlKind_Number) {
			printf("%s::%d ", WlKindText[tk.kind], tk.valueNum);
		} else {
			printf("%s ", WlKindText[tk.kind]);
		}
		return;
	}

	switch (tk.kind) {
	case WlKind_StBinaryExpression: {
		WlBinaryExpression bin = *(WlBinaryExpression *)tk.valuePtr;
		wlPrint(bin.left);
		wlPrint(bin.operator);
		wlPrint(bin.right);
	} break;
	case WlKind_StExpressionStatement: {
		WlExpressionStatement ex = *(WlExpressionStatement *)tk.valuePtr;
		wlPrint(ex.expression);
		wlPrint(ex.semicolon);
	} break;
	case WlKind_StReturnStatement: {
		WlReturnStatement ret = *(WlReturnStatement *)tk.valuePtr;
		wlPrint(ret.returnKeyword);
		wlPrint(ret.expression);
		wlPrint(ret.semicolon);
	} break;
	case WlKind_StFunction: {
		WlSyntaxFunction *fn = tk.valuePtr;
		if (fn->export.kind == WlKind_KwExport) {
			wlPrint(fn->export);
		}
		wlPrint(fn->type);
		wlPrint(fn->name);
		wlPrint(fn->parenOpen);
		wlPrint(fn->parenClose);
		wlPrintBlock(fn->body);
	} break;
	case WlKind_StCall: {
		WlSyntaxCall call = *(WlSyntaxCall *)tk.valuePtr;
		wlPrint(call.name);
		wlPrint(call.parenOpen);
		wlPrint(call.arg);
		wlPrint(call.parenClose);
	} break;
	case WlKind_StRef: {
		printf("<ref>::%.*s ", STRPRINT(tk.valueStr));
	} break;

	default: PANIC("Unhandled print function for %s %d", WlKindText[tk.kind], tk.kind); break;
	}
}
