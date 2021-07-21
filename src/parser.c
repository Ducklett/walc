#include <walc.h>

typedef struct {
	Str filename;
	Str source;
	int index;
	List(WlDiagnostic) diagnostics;
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

WlLexer wlLexerCreate(Str filename, Str source)
{
	if (strEqual(filename, STREMPTY)) filename = STR("<compiler generated source>");
	return (WlLexer){
		.filename = filename,
		.source = source,
		.index = 0,
		.diagnostics = listNew(),
	};
}

void wlLexerFree(WlLexer *l) { listFree(&l->diagnostics); }

char wlLexerCurrent(WlLexer *l) { return l->index < l->source.len ? l->source.buf[l->index] : '\0'; }
char wlLexerLookahead(WlLexer *l, int n) { return l->index + n < l->source.len ? l->source.buf[l->index + n] : '\0'; }

WlToken wlLexerBasic(WlLexer *l, int len, WlKind kind)
{
	WlSpan span = {.filename = l->filename, .source = l->source, .start = l->index, .len = len};
	l->index += len;
	return (WlToken){.kind = kind, .span = span};
}

void wlLexerTrim(WlLexer *l)
{
	while (isWhitespace(wlLexerCurrent(l)))
		l->index++;
}

WlToken lexerReport(WlLexer *l, WlDiagnosticKind kind, int start, int end)
{
	WlSpan span = spanFromRange(l->filename, l->source, start, l->index);
	WlDiagnostic d = {.kind = kind, .span = span};
	listPush(&l->diagnostics, d);
	return (WlToken){.kind = WlKind_Bad, .span = span};
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
			if (wlLexerCurrent(l) == '\0') {
				return lexerReport(l, UnterminatedCommentDiagnostic, l->index - 2, l->index);
			}

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
			return wlLexerBasic(l, 1, WlKind_OpEquals);
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
	case '[': return wlLexerBasic(l, 1, WlKind_TkBracketOpen); break;
	case ']': return wlLexerBasic(l, 1, WlKind_TkBracketClose); break;
	case ',': return wlLexerBasic(l, 1, WlKind_TkComma); break;
	case ';': return wlLexerBasic(l, 1, WlKind_TkSemicolon); break;
	case '"': {
		l->index++;
		int start = l->index;

		while (wlLexerCurrent(l) != '\0' && wlLexerCurrent(l) != '"')
			l->index++;

		if (wlLexerCurrent(l) == '\0') {
			return lexerReport(l, UnterminatedStringDiagnostic, start, l->index);
		}
		Str value = strSlice(l->source, start, l->index - start);
		l->index++;

		return (WlToken){.kind = WlKind_String,
						 .span = spanFromRange(l->filename, l->source, start - 1, l->index),
						 .valueStr = value};
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
				return (WlToken){
					.kind = WlKind_FloatNumber,
					.valueFloat = finalValue,
					.span = spanFromRange(l->filename, l->source, start, l->index),
				};
			}
		}
		return (WlToken){
			.kind = WlKind_Number,
			.valueNum = value,
			.span = spanFromRange(l->filename, l->source, start, l->index),
		};
	} break;
	default: {
	unmatched : {
	}
		if (isLowercaseLetter(current)) {
			for (int i = WlKind_Keywords_Start + 1; i < WlKind_Keywords_End; i++) {
				// TODO: bake into Str's beforehand so we don't have to convert from cstr
				Str keyword = strFromCstr(WlKindText[i]);
				if (strEqual(strSlice(l->source, l->index, keyword.len), keyword)) {
					int start = l->index;
					l->index += keyword.len;
					return (WlToken){
						.kind = i,
						.span = spanFromRange(l->filename, l->source, start, l->index),
					};
					break;
				}
			}
		}

		if (isSymbolStart(current)) {
			int start = l->index;
			while (isSymbol(wlLexerCurrent(l)))
				l->index++;
			Str symbolName = strSlice(l->source, start, l->index - start);
			return (WlToken){
				.kind = WlKind_Symbol,
				.valueStr = symbolName,
				.span = spanFromRange(l->filename, l->source, start, l->index),
			};
		} else {
			l->index++;
			return lexerReport(l, UnexpectedCharacterDiagnostic, l->index - 1, l->index);
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
	WlToken variable;
	WlToken equals;
	WlToken expression;
	WlToken semicolon;
} WlAssignmentExpression;

typedef struct {
	WlToken name;
	WlToken parenOpen;
	List(WlToken) args;
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
	WlToken import;
	WlToken type;
	WlToken name;
	WlToken parenOpen;
	List(WlToken) parameterList;
	WlToken parenClose;
	WlToken semicolon;
} WlSyntaxImport;

typedef struct {
	WlToken export;
	WlToken type;
	WlToken name;
	WlToken equals;
	WlToken initializer;
	WlToken semicolon;
} WlSyntaxVariableDeclaration;

#define PARSERMAXLOOKAHEAD 8

typedef struct {
	WlLexer lexer;
	List(WlToken) topLevelDeclarations;
	List(WlDiagnostic) diagnostics;
	ArenaAllocator arena;
	WlToken tokens[PARSERMAXLOOKAHEAD];
	int tokenIndex;
	int lookaheadCount;
} WlParser;

WlParser wlParserCreate(Str filename, Str source)
{
	WlLexer l = wlLexerCreate(filename, source);
	return (WlParser){
		.lexer = l,
		.topLevelDeclarations = listNew(),
		.diagnostics = listNew(),
		.arena = arenaCreate(),
		.tokens = {0},
		.lookaheadCount = 0,
		.tokenIndex = 0,
	};
}

WlToken wlParserLookahead(WlParser *p, int amount)
{
	assert(amount <= PARSERMAXLOOKAHEAD);
	while (amount >= p->lookaheadCount) {
		int index = (p->tokenIndex + p->lookaheadCount) % PARSERMAXLOOKAHEAD;
		p->lookaheadCount++;
		p->tokens[index] = wlLexerLexToken(&p->lexer);
	}
	return p->tokens[(p->tokenIndex + amount) % PARSERMAXLOOKAHEAD];
}

WlToken wlParserPeek(WlParser *p) { return wlParserLookahead(p, 0); }

WlToken wlParserTake(WlParser *p)
{
	WlToken t = wlParserLookahead(p, 0); // p->hasToken ? p->current : wlLexerLexToken(&p->lexer);
	p->lookaheadCount--;
	p->tokenIndex = (p->tokenIndex + 1) % PARSERMAXLOOKAHEAD;
	return t;
}

WlToken parserReport(WlParser *p, WlDiagnosticKind kind, WlToken t, WlKind expected)
{
	if (t.kind != WlKind_Bad) {
		WlDiagnostic d = {.kind = kind, .span = t.span, .kind1 = t.kind, .kind2 = expected};
		listPush(&p->diagnostics, d);
		t.kind = WlKind_Bad;
		return t;
	}
}

WlToken wlParserMatch_impl(WlParser *p, WlKind kind, const char *file, int line)
{
	WlToken t = wlParserTake(p);

	if (t.kind != kind) {
		return parserReport(p, UnexpectedTokenDiagnostic, t, kind);
	}

	return t;
}
#define wlParserMatch(p, k) wlParserMatch_impl(p, k, __FILE__, __LINE__)

void wlParserAddTopLevelStatement(WlParser *p, WlToken tk) { listPush(&p->topLevelDeclarations, tk); }

WlToken wlParseExpression(WlParser *p);

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

List(WlToken) wlParseArgumentList(WlParser *p)
{
	List(WlToken) list = listNew();

	while (true) {
		if (wlParserPeek(p).kind == WlKind_TkParenClose) break;
		WlToken arg = wlParseExpression(p);
		listPush(&list, arg);

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
	case WlKind_String: return wlParserTake(p);
	case WlKind_Symbol: {
		WlToken symbol = wlParserMatch(p, WlKind_Symbol);
		if (wlParserPeek(p).kind == WlKind_TkParenOpen) {
			WlSyntaxCall call = {0};
			call.name = symbol;
			call.parenOpen = wlParserMatch(p, WlKind_TkParenOpen);
			call.args = wlParseArgumentList(p);
			call.parenClose = wlParserMatch(p, WlKind_TkParenClose);

			WlSyntaxCall *callp = arenaMalloc(sizeof(WlSyntaxCall), &p->arena);
			*callp = call;

			return (WlToken){.kind = WlKind_StCall, .valuePtr = callp};
		} else {
			symbol.kind = WlKind_StRef;
			return symbol;
		}
	}
	default: return parserReport(p, UnexpectedTokenInPrimaryExpressionDiagnostic, wlParserTake(p), 0);
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
	switch (wlParserPeek(p).kind) {
	case WlKind_KwReturn: {
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
	} break;
	case WlKind_Symbol: {
		switch (wlParserLookahead(p, 1).kind) {
		case WlKind_OpEquals: {
			WlAssignmentExpression var = {0};
			var.variable = wlParserMatch(p, WlKind_Symbol);
			var.equals = wlParserMatch(p, WlKind_OpEquals);
			var.expression = wlParseExpression(p);
			var.semicolon = wlParserMatch(p, WlKind_TkSemicolon);

			WlAssignmentExpression *varp = arenaMalloc(sizeof(WlAssignmentExpression), &p->arena);
			*varp = var;
			return (WlToken){.kind = WlKind_StVariableAssignement, .valuePtr = varp};
		}
		case WlKind_Symbol: {
			WlSyntaxVariableDeclaration var = {0};
			var.export = (WlToken){.kind = WlKind_Missing};
			var.type = wlParserMatch(p, WlKind_Symbol);
			var.name = wlParserMatch(p, WlKind_Symbol);
			if (wlParserPeek(p).kind == WlKind_OpEquals) {
				var.equals = wlParserMatch(p, WlKind_OpEquals);
				var.initializer = wlParseExpression(p);
			} else {
				var.equals = (WlToken){.kind = WlKind_Missing};
				var.initializer = (WlToken){.kind = WlKind_Missing};
			}
			var.semicolon = wlParserMatch(p, WlKind_TkSemicolon);

			WlSyntaxVariableDeclaration *varp = arenaMalloc(sizeof(WlSyntaxVariableDeclaration), &p->arena);
			*varp = var;
			return (WlToken){.kind = WlKind_StVariableDeclaration, .valuePtr = varp};
		} break;
		default: goto defaultExpression; break;
		}

	} break;
	default: {
	defaultExpression : {
	}
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
	} break;
	}
}

WlSyntaxBlock wlParseBlock(WlParser *p)
{
	WlSyntaxBlock blk = {0};
	blk.curlyOpen = wlParserMatch(p, WlKind_TkCurlyOpen);
	blk.statements = malloc(sizeof(WlToken) * 0xFF);
	while (wlParserPeek(p).kind != WlKind_TkCurlyClose && wlParserPeek(p).kind != WlKind_EOF) {
		blk.statements[blk.statementCount++] = wlParseStatement(p);
	}

	blk.curlyClose = wlParserMatch(p, WlKind_TkCurlyClose);
	return blk;
}

WlToken wlParseImport(WlParser *p)
{
	WlSyntaxImport im = {0};

	im.import = wlParserMatch(p, WlKind_KwImport);
	im.type = wlParserMatch(p, WlKind_Symbol);

	// infer type as u0
	if (wlParserPeek(p).kind == WlKind_TkParenOpen) {
		im.name = im.type;
		im.type.kind = WlKind_Missing;
	} else {
		im.name = wlParserMatch(p, WlKind_Symbol);
	}

	im.parenOpen = wlParserMatch(p, WlKind_TkParenOpen);
	im.parameterList = wlParseParameterList(p);
	im.parenClose = wlParserMatch(p, WlKind_TkParenClose);
	im.semicolon = wlParserMatch(p, WlKind_TkSemicolon);

	WlSyntaxImport *imp = arenaMalloc(sizeof(WlSyntaxImport), &p->arena);
	*imp = im;

	WlToken tk = {.kind = WlKind_StImport, .valuePtr = imp};
	wlParserAddTopLevelStatement(p, tk);
	return tk;
}

WlToken wlParseFunction(WlParser *p)
{
	int errorCount = listLen(p->diagnostics);

	WlSyntaxFunction fn = {0};

	if (wlParserPeek(p).kind == WlKind_KwExport) {
		fn.export = wlParserMatch(p, WlKind_KwExport);
	} else {
		fn.export = (WlToken){.kind = WlKind_Missing};
	}

	fn.type = wlParserMatch(p, WlKind_Symbol);

	// infer type as u0
	if (wlParserPeek(p).kind == WlKind_TkParenOpen) {
		fn.name = fn.type;
		fn.type.kind = WlKind_Missing;
	} else {
		fn.name = wlParserMatch(p, WlKind_Symbol);
	}

	fn.parenOpen = wlParserMatch(p, WlKind_TkParenOpen);
	fn.parameterList = wlParseParameterList(p);
	fn.parenClose = wlParserMatch(p, WlKind_TkParenClose);
	fn.body = wlParseBlock(p);
	WlSyntaxFunction *fnp = arenaMalloc(sizeof(WlSyntaxFunction), &p->arena);
	*fnp = fn;

	bool hasError = errorCount != listLen(p->diagnostics);
	WlToken tk = (WlToken){.kind = hasError ? WlKind_Bad : WlKind_StFunction, .valuePtr = fnp};

	wlParserAddTopLevelStatement(p, tk);
	return tk;
}

WlToken wlParseDeclaration(WlParser *p)
{
	switch (wlParserPeek(p).kind) {
	case WlKind_KwImport: return wlParseImport(p);
	default: return wlParseFunction(p);
	}
}

void wlParse(WlParser *p)
{
	while (wlParserPeek(p).kind != WlKind_EOF) {
		wlParseDeclaration(p);
	}
}

WlParser wlParserFree(WlParser *p)
{
	arenaFree(&p->arena);
	wlLexerFree(&p->lexer);
	listFree(&p->topLevelDeclarations);
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
		if (tk.kind == WlKind_Missing) {
			// print nothing :3c
		} else if (tk.kind == WlKind_String) {
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
	case WlKind_StImport: {
		WlSyntaxImport *fn = tk.valuePtr;
		wlPrint(fn->import);
		wlPrint(fn->type);
		wlPrint(fn->name);
		wlPrint(fn->parenOpen);
		wlPrint(fn->parenClose);
		wlPrint(fn->semicolon);
	} break;
	case WlKind_StCall: {
		WlSyntaxCall call = *(WlSyntaxCall *)tk.valuePtr;
		wlPrint(call.name);
		wlPrint(call.parenOpen);
		for (int i = 0; i < listLen(call.args); i++) {
			wlPrint(call.args[i]);
		}
		wlPrint(call.parenClose);
	} break;
	case WlKind_StVariableDeclaration: {
		WlSyntaxVariableDeclaration var = *(WlSyntaxVariableDeclaration *)tk.valuePtr;
		wlPrint(var.export);
		wlPrint(var.type);
		wlPrint(var.name);
		wlPrint(var.equals);
		wlPrint(var.initializer);
		wlPrint(var.semicolon);
	} break;
	case WlKind_StVariableAssignement: {
		WlAssignmentExpression var = *(WlAssignmentExpression *)tk.valuePtr;
		wlPrint(var.variable);
		wlPrint(var.equals);
		wlPrint(var.expression);
		wlPrint(var.semicolon);
	} break;
	case WlKind_StRef: {
		printf("<ref>::%.*s ", STRPRINT(tk.valueStr));
	} break;

	default: PANIC("Unhandled print function for %s %d", WlKindText[tk.kind], tk.kind); break;
	}
}
