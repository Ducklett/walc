#include <sti_base.h>

typedef enum
{
	WlKind_Bad,
	WlKind_EOF,
	WlKind_Missing,
	WlKind_Symbol,
	WlKind_Number,
	WlKind_String,
	WlKind_OpPlus,
	WlKind_OpMinus,
	WlKind_OpStar,
	WlKind_OpSlash,
	WlKind_OpEquals,
	WlKind_OpDoubleEquals,
	WlKind_OpBangEquals,

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
	WlKind_StExpressionStatement,
	WlKind_StCall,
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
	"<string>",
	"+",
	"-",
	"*",
	"/",
	"=",
	"==",
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
	"WlKind_StExpressionStatement",
	"WlKind_StCall",
	"WlKind_StType",
	"WlKind_StBlock",
	"WlKind_StExpression",
	"WlKind_StExtern",
	"<syntax end>",
};

typedef struct {
	WlKind kind;
	union {
		int valueNum;
		Str valueStr;
		void *valuePtr;
	};
} WlToken;

typedef struct {
	Str source;
	int index;
} WlLexer;

bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isBinaryDigit(char c) { return c == '0' || c == '1'; }
bool isHexDigit(char c) { return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
bool isLowercaseLetter(char c) { return c >= 'a' && c <= 'z'; }
bool isUppercaseLetter(char c) { return c >= 'A' && c <= 'Z'; }
bool isLetter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool isCompilerReserved(char c)
{
	return (c >= 0 && c <= 47) || c == 64 || (c >= 91 && c <= 94) || (c >= 123 && c <= 127) || (c == 255);
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
	wlLexerTrim(l);
	char current = wlLexerCurrent(l);

	if (!current) return wlLexerBasic(l, 0, WlKind_EOF);

	switch (current) {
	case '+': return wlLexerBasic(l, 1, WlKind_OpPlus);
	case '-': return wlLexerBasic(l, 1, WlKind_OpMinus); break;
	case '*': return wlLexerBasic(l, 1, WlKind_OpStar); break;
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
			}
			}

		} else {
			while (isDigit(wlLexerCurrent(l))) {
				u8 digitValue = wlLexerCurrent(l) - '0';
				value *= 10;
				value += digitValue;
				l->index++;
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
	WlToken export;
	WlToken type;
	WlToken name;
	WlToken parenOpen;
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

WlToken wlParserMatch(WlParser *p, WlKind kind)
{
	WlToken t = wlParserTake(p);

	if (t.kind != kind) {
		printf("unexpected token %s, expected %s", WlKindText[t.kind], WlKindText[kind]);
		TODO("Report diagnostic on mismatched token");
	}
	return t;
}

void wlParserAddTopLevelStatement(WlParser *p, WlToken tk)
{
	if (p->topLevelCount > 0xFF) TODO("Make top level count growable");
	p->topLevelDeclarations[p->topLevelCount++] = tk;
}

WlToken wlParseExpression(WlParser *p)
{
	WlSyntaxCall call = {0};
	call.name = wlParserMatch(p, WlKind_Symbol);
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
}

WlToken wlParseStatement(WlParser *p)
{
	WlExpressionStatement st = {0};
	st.expression = wlParseExpression(p);
	st.semicolon = wlParserMatch(p, WlKind_TkSemicolon);
	WlExpressionStatement *stp = arenaMalloc(sizeof(WlExpressionStatement), &p->arena);
	*stp = st;
	return (WlToken){.kind = WlKind_StExpressionStatement, .valuePtr = stp};
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
		assert(st.kind == WlKind_StExpressionStatement);
		WlExpressionStatement ex = *(WlExpressionStatement *)st.valuePtr;
		assert(ex.expression.kind == WlKind_StCall);
		wlPrint(ex.expression);
		wlPrint(ex.semicolon);
		printf("\n");
	}
	wlPrint(blk.curlyClose);
	printf("\n");
}

void wlPrint(WlToken tk)
{
	if (tk.kind < WlKind_Syntax_Start) {
		if (tk.kind == WlKind_String) {
			printf("%s::\"%.*s\"", WlKindText[tk.kind], STRPRINT(tk.valueStr));
		} else if (tk.kind == WlKind_Symbol) {
			printf("%s::%.*s", WlKindText[tk.kind], STRPRINT(tk.valueStr));
		} else if (tk.kind == WlKind_Number) {
			printf("%s::%d", WlKindText[tk.kind], tk.valueNum);
		} else {
			printf("%s", WlKindText[tk.kind]);
		}
		return;
	}

	switch (tk.kind) {
	case WlKind_StFunction: {
		WlSyntaxFunction *fn = tk.valuePtr;
		if (fn->export.kind == WlKind_KwExport) {
			wlPrint(fn->export);
			printf(" ");
		}
		wlPrint(fn->type);
		printf(" ");
		wlPrint(fn->name);
		wlPrint(fn->parenOpen);
		wlPrint(fn->parenClose);
		printf(" ");
		wlPrintBlock(fn->body);
	} break;
	case WlKind_StCall: {
		WlSyntaxCall call = *(WlSyntaxCall *)tk.valuePtr;
		wlPrint(call.name);
		wlPrint(call.parenOpen);
		wlPrint(call.arg);
		wlPrint(call.parenClose);
	} break;

	default: TODO("Handle %s %d", WlKindText[tk.kind], tk.kind); break;
	}
}
