#include <walc.h>

typedef struct {
	Str filename;
	Str source;
	int index;
	List(WlDiagnostic) diagnostics;
} WlLexer;

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
	case '+':
		return wlLexerLookahead(l, 1) == '+' ? wlLexerBasic(l, 2, WlKind_OpPlusPlus)
											 : wlLexerBasic(l, 1, WlKind_OpPlus);
	case '-':
		return wlLexerLookahead(l, 1) == '-' ? wlLexerBasic(l, 2, WlKind_OpMinusMinus)
											 : wlLexerBasic(l, 1, WlKind_OpMinus);
	case '*': return wlLexerBasic(l, 1, WlKind_OpStar);
	case '%': return wlLexerBasic(l, 1, WlKind_OpPercent);
	case '/': return wlLexerBasic(l, 1, WlKind_OpSlash);
	case '=':
		if (wlLexerLookahead(l, 1) == '=')
			return wlLexerBasic(l, 2, WlKind_OpEuqualsEquals);
		else
			return wlLexerBasic(l, 1, WlKind_OpEquals);
	case '!':
		return wlLexerLookahead(l, 1) == '=' ? wlLexerBasic(l, 2, WlKind_OpBangEquals)
											 : wlLexerBasic(l, 1, WlKind_OpBang);
		break;
	case '<':
		if (wlLexerLookahead(l, 1) == '<')
			return wlLexerBasic(l, 2, WlKind_OpLessLess);
		else if (wlLexerLookahead(l, 1) == '=')
			return wlLexerBasic(l, 2, WlKind_OpLessEquals);
		else
			return wlLexerBasic(l, 1, WlKind_OpLess);
	case '>':
		if (wlLexerLookahead(l, 1) == '>')
			return wlLexerBasic(l, 2, WlKind_OpGreaterGreater);
		else if (wlLexerLookahead(l, 1) == '=')
			return wlLexerBasic(l, 2, WlKind_OpGreaterEquals);
		else
			return wlLexerBasic(l, 1, WlKind_OpGreater);
	case '&':
		return (wlLexerLookahead(l, 1) == '&') //
				   ? wlLexerBasic(l, 2, WlKind_OpAmpersandAmpersand)
				   : wlLexerBasic(l, 1, WlKind_OpAmpersand);
	case '|':
		return (wlLexerLookahead(l, 1) == '|') //
				   ? wlLexerBasic(l, 2, WlKind_OpPipePipe)
				   : wlLexerBasic(l, 1, WlKind_OpPipe);
	case '^': return wlLexerBasic(l, 1, WlKind_OpCaret);
	case '?': return wlLexerBasic(l, 1, WlKind_OpQuestion);
	case ':': return wlLexerBasic(l, 1, WlKind_OpColon);
	case '(': return wlLexerBasic(l, 1, WlKind_TkParenOpen);
	case ')': return wlLexerBasic(l, 1, WlKind_TkParenClose);
	case '{': return wlLexerBasic(l, 1, WlKind_TkCurlyOpen);
	case '}': return wlLexerBasic(l, 1, WlKind_TkCurlyClose);
	case '[': return wlLexerBasic(l, 1, WlKind_TkBracketOpen);
	case ']': return wlLexerBasic(l, 1, WlKind_TkBracketClose);
	case ',': return wlLexerBasic(l, 1, WlKind_TkComma);
	case '.': return wlLexerBasic(l, 1, WlKind_TkDot);
	case ';': return wlLexerBasic(l, 1, WlKind_TkSemicolon);
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
				char afterKeyword = wlLexerLookahead(l, keyword.len);
				if (strEqual(strSlice(l->source, l->index, keyword.len), keyword) && !isSymbol(afterKeyword)) {
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
	WlToken operator;
	WlToken expression;
} WlPreUnaryExpression;

typedef struct {
	WlToken expression;
	WlToken operator;
} WlPostUnaryExpression;

typedef struct {
	WlToken condition;
	WlToken question;
	WlToken thenExpr;
	WlToken colon;
	WlToken elseExpr;
} WlTernaryExpression;

typedef struct {
	WlToken variable;
	WlToken equals;
	WlToken expression;
	WlToken semicolon;
} WlAssignmentExpression;

typedef struct {
	List(WlToken) path;
	WlToken parenOpen;
	List(WlToken) args;
	WlToken parenClose;
} WlSyntaxCall;

typedef struct {
	WlToken curlyOpen;
	List(WlToken) statements;
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
	WlToken namespace;
	List(WlToken) path;
	WlSyntaxBlock body;
} WlSyntaxNamespace;

typedef struct {
	WlToken use;
	List(WlToken) path;
	WlToken semicolon;
} WlSyntaxUse;

typedef struct {
	WlToken ifKeyword;
	WlToken condition;
	WlSyntaxBlock thenBlock;
	WlToken elseKeyword;
	WlSyntaxBlock elseBlock;
} WlSyntaxIf;

typedef struct {
	WlToken doKeyword;
	WlSyntaxBlock block;
} WlSyntaxDo;

typedef struct {
	WlToken whileKeyword;
	WlToken condition;
	WlSyntaxBlock block;
} WlSyntaxWhile;

typedef struct {
	WlToken doKeyword;
	WlSyntaxBlock block;
	WlToken whileKeyword;
	WlToken condition;
	WlToken semicolon;
} WlSyntaxDoWhile;

typedef struct {
	WlToken forKeyword;
	WlToken preCondition;
	WlToken semicolon;
	WlToken condition;
	WlToken semicolon2;
	WlToken postCondition;
	WlSyntaxBlock block;
} WlSyntaxFor;

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
	bool sectionStart;
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

typedef enum
{
	BlockParseStatements = 0,
	BlockParseDeclarations = 1,
} BlockParseOptions;

WlToken wlParseDeclaration(WlParser *p, bool topLevel);
WlToken wlParseExpression(WlParser *p);
WlToken wlParseUse(WlParser *p);
WlSyntaxBlock wlParseBlock(WlParser *p, BlockParseOptions options);

WlToken wlParseParameter(WlParser *p)
{
	WlSyntaxParameter param;
	param.type = wlParserMatch(p, WlKind_Symbol);
	param.name = wlParserMatch(p, WlKind_Symbol);

	WlSyntaxParameter *paramp = arenaMalloc(sizeof(WlSyntaxParameter), &p->arena);
	*paramp = param;
	return (WlToken){
		.kind = WlKind_StFunctionParameter,
		.valuePtr = paramp,
		.span = spanFromTokens(param.type, param.name),
	};
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

List(WlToken) wlParseReferencePath(WlParser *p)
{
	List(WlToken) path = listNew();

	while (true) {
		WlToken segment = wlParserMatch(p, WlKind_Symbol);
		listPush(&path, segment);

		if (wlParserPeek(p).kind != WlKind_TkDot) break;
		WlToken delim = wlParserMatch(p, WlKind_TkDot);
		listPush(&path, delim);
	}
	return path;
}

WlToken wlParsePrimaryExpression(WlParser *p)
{
	switch (wlParserPeek(p).kind) {

	case WlKind_KwDo: {
		WlSyntaxDo st = {0};
		st.doKeyword = wlParserMatch(p, WlKind_KwDo);
		st.block = wlParseBlock(p, BlockParseStatements);

		WlSyntaxDo *stp = arenaMalloc(sizeof(WlSyntaxDo), &p->arena);
		*stp = st;
		return (
			WlToken){.kind = WlKind_StDo, .valuePtr = stp, .span = spanFromTokens(st.doKeyword, st.block.curlyClose)};
	} break;
	case WlKind_OpPlusPlus:
	case WlKind_OpMinusMinus:
	case WlKind_OpMinus:
	case WlKind_OpBang: {
		WlPreUnaryExpression expr = {0};
		expr.operator= wlParserTake(p);
		expr.expression = wlParsePrimaryExpression(p);
		WlPreUnaryExpression *exprp = arenaMalloc(sizeof(WlPreUnaryExpression), &p->arena);
		*exprp = expr;

		return (WlToken){
			.kind = WlKind_StPreUnary,
			.valuePtr = exprp,
			.span = spanFromTokens(expr.operator, expr.expression),
		};
	} break;
	case WlKind_Number: return wlParserTake(p);
	case WlKind_FloatNumber: return wlParserTake(p);
	case WlKind_String: return wlParserTake(p);
	case WlKind_KwTrue: return wlParserTake(p);
	case WlKind_KwFalse: return wlParserTake(p);
	case WlKind_Symbol: {
		List(WlToken) path = wlParseReferencePath(p);
		if (wlParserPeek(p).kind == WlKind_TkParenOpen) {
			WlSyntaxCall call = {0};
			call.path = path;
			call.parenOpen = wlParserMatch(p, WlKind_TkParenOpen);
			call.args = wlParseArgumentList(p);
			call.parenClose = wlParserMatch(p, WlKind_TkParenClose);

			WlSyntaxCall *callp = arenaMalloc(sizeof(WlSyntaxCall), &p->arena);
			*callp = call;

			return (WlToken){
				.kind = WlKind_StCall,
				.valuePtr = callp,
				.span = spanFromTokens(call.path[0], call.parenClose),
			};
		} else {
			WlToken ref = {
				.kind = WlKind_StRef,
				.valuePtr = path,
				.span = spanFromTokens(path[0], path[listLen(path) - 1]),
			};

			if (wlParserPeek(p).kind == WlKind_OpPlusPlus || wlParserPeek(p).kind == WlKind_OpMinusMinus) {
				WlPostUnaryExpression post = {
					.expression = ref,
					.operator= wlParserTake(p),
				};
				WlPostUnaryExpression *postp = arenaMalloc(sizeof(WlPostUnaryExpression), &p->arena);
				*postp = post;

				WlToken ref = {
					.kind = WlKind_StPostUnary,
					.valuePtr = postp,
					.span = spanFromTokens(ref, post.expression),
				};
				return ref;
			} else {
				return ref;
			}
		}
	} break;
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
	case WlKind_OpLessLess: return 11;
	case WlKind_OpGreaterGreater: return 11;
	case WlKind_OpGreater: return 10;
	case WlKind_OpGreaterEquals: return 10;
	case WlKind_OpLess: return 10;
	case WlKind_OpLessEquals: return 10;
	case WlKind_OpEuqualsEquals: return 9;
	case WlKind_OpBangEquals: return 9;
	case WlKind_OpAmpersand: return 8;
	case WlKind_OpCaret: return 7;
	case WlKind_OpPipe: return 6;
	case WlKind_OpAmpersandAmpersand: return 5;
	case WlKind_OpPipePipe: return 4;
	case WlKind_OpQuestion: return 3;
	default: return -1;
	}
}

WlToken parseTernary(WlParser *p, WlToken condition)
{
	WlTernaryExpression tr = {0};
	tr.condition = condition;
	tr.question = wlParserMatch(p, WlKind_OpQuestion);
	tr.thenExpr = wlParseExpression(p);
	tr.colon = wlParserMatch(p, WlKind_OpColon);
	tr.elseExpr = wlParseExpression(p);

	WlTernaryExpression *exprData = arenaMalloc(sizeof(WlTernaryExpression), &p->arena);
	*exprData = tr;
	return (WlToken){.kind = WlKind_StTernaryExpression,
					 .valuePtr = exprData,
					 .span = spanFromTokens(tr.condition, tr.elseExpr)};
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

		if (operator.kind == WlKind_OpQuestion) {
			WlToken tern = parseTernary(p, left);
			left = tern;
			continue;
		}

		operator= wlParserTake(p);

		WlToken right = wlParseBinaryExpression(p, precedence);

		WlBinaryExpression *exprData = arenaMalloc(sizeof(WlBinaryExpression), &p->arena);
		*exprData = (WlBinaryExpression){.left = left, .operator= operator, .right = right };

		WlToken binaryExpression = {.kind = WlKind_StBinaryExpression,
									.valuePtr = exprData,
									.span = spanFromTokens(left, right)};
		left = binaryExpression;
	}

	return left;
}
WlToken wlParseFullBinaryExpression(WlParser *p) { return wlParseBinaryExpression(p, -1); }

WlToken wlParseExpression(WlParser *p) { return wlParseFullBinaryExpression(p); }

WlToken wlParseStatement(WlParser *p)
{
	switch (wlParserPeek(p).kind) {
	case WlKind_KwUse: return wlParseUse(p); break;

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

		return (WlToken){.kind = WlKind_StReturnStatement,
						 .valuePtr = stp,
						 .span = spanFromTokens(st.returnKeyword, st.semicolon)};
	} break;
	case WlKind_KwVar: goto variableDeclaration; break;
	case WlKind_KwLet: goto variableDeclaration; break;
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
			return (WlToken){.kind = WlKind_StVariableAssignement,
							 .valuePtr = varp,
							 .span = spanFromTokens(var.variable, var.semicolon)};
		}
		case WlKind_Symbol: {
			// local function
			// TODO: implicit return type for local functions
			// detecting implicit return type is kinda tricky
			// because foo() {} looks like a call expression at first
			// we'd need to start parsing the argument list
			// and then swap over to parsing a parameter list if any errors occur
			if (wlParserLookahead(p, 2).kind == WlKind_TkParenOpen) {
				return wlParseDeclaration(p, false);
			}
		variableDeclaration : {
		}
			WlSyntaxVariableDeclaration var = {0};
			var.export = (WlToken){.kind = WlKind_Missing};
			if (wlParserPeek(p).kind == WlKind_KwVar || wlParserPeek(p).kind == WlKind_KwLet) {
				var.type = wlParserTake(p);
			} else {
				var.type = wlParserMatch(p, WlKind_Symbol);
			}
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
			return (WlToken){.kind = WlKind_StVariableDeclaration,
							 .valuePtr = varp,
							 .span = spanFromTokens(var.type, var.semicolon)};
		} break;
		default: goto defaultExpression; break;
		}

	} break;
	case WlKind_KwIf: {
		WlSyntaxIf st = {0};
		st.ifKeyword = wlParserMatch(p, WlKind_KwIf);
		st.thenBlock = wlParseBlock(p, BlockParseStatements);
		WlToken last;
		if (wlParserPeek(p).kind == WlKind_KwElse) {
			st.elseKeyword = wlParserMatch(p, WlKind_KwElse);
			st.elseBlock = wlParseBlock(p, BlockParseStatements);
			last = st.elseBlock.curlyClose;
		} else {
			st.elseKeyword = (WlToken){.kind = WlKind_Missing};
			last = st.thenBlock.curlyClose;
		}

		WlSyntaxIf *stp = arenaMalloc(sizeof(WlSyntaxIf), &p->arena);
		*stp = st;
		return (WlToken){.kind = WlKind_StIf, .valuePtr = stp, .span = spanFromTokens(st.ifKeyword, last)};

	} break;
	case WlKind_KwDo: {
		WlSyntaxDoWhile st = {0};
		st.doKeyword = wlParserMatch(p, WlKind_KwDo);
		st.block = wlParseBlock(p, BlockParseStatements);

		st.whileKeyword = wlParserMatch(p, WlKind_KwWhile);
		st.condition = wlParseExpression(p);
		st.semicolon = wlParserMatch(p, WlKind_TkSemicolon);
		WlSyntaxDoWhile *stp = arenaMalloc(sizeof(WlSyntaxDoWhile), &p->arena);
		*stp = st;
		return (WlToken){.kind = WlKind_StDoWhile, .valuePtr = stp, .span = spanFromTokens(st.doKeyword, st.semicolon)};
	} break;
	case WlKind_KwWhile: {
		WlSyntaxWhile st = {0};
		st.whileKeyword = wlParserMatch(p, WlKind_KwIf);
		st.condition = wlParseExpression(p);
		st.block = wlParseBlock(p, BlockParseStatements);

		WlSyntaxWhile *stp = arenaMalloc(sizeof(WlSyntaxWhile), &p->arena);
		*stp = st;
		return (WlToken){.kind = WlKind_StWhile,
						 .valuePtr = stp,
						 .span = spanFromTokens(st.whileKeyword, st.block.curlyClose)};

	} break;
	case WlKind_KwFor: {
		WlSyntaxFor st = {0};
		st.forKeyword = wlParserMatch(p, WlKind_KwIf);
		st.preCondition = wlParseExpression(p);
		st.semicolon = wlParserMatch(p, WlKind_TkSemicolon);
		st.condition = wlParseExpression(p);
		st.semicolon2 = wlParserMatch(p, WlKind_TkSemicolon);
		st.postCondition = wlParseExpression(p);
		st.block = wlParseBlock(p, BlockParseStatements);

		WlSyntaxFor *stp = arenaMalloc(sizeof(WlSyntaxFor), &p->arena);
		*stp = st;
		return (WlToken){.kind = WlKind_StWhile,
						 .valuePtr = stp,
						 .span = spanFromTokens(st.forKeyword, st.block.curlyClose)};

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
			return (WlToken){.kind = WlKind_StReturnStatement,
							 .valuePtr = stp,
							 .span = spanFromTokens(st.expression, st.semicolon)};
		} else {
			WlExpressionStatement st = {0};
			st.expression = expression;
			st.semicolon = wlParserMatch(p, WlKind_TkSemicolon);
			WlExpressionStatement *stp = arenaMalloc(sizeof(WlExpressionStatement), &p->arena);
			*stp = st;
			return (WlToken){.kind = WlKind_StExpressionStatement,
							 .valuePtr = stp,
							 .span = spanFromTokens(st.expression, st.semicolon)};
		}
	} break;
	}
}

WlSyntaxBlock wlParseBlock(WlParser *p, BlockParseOptions options)
{
	WlSyntaxBlock blk = {0};
	blk.curlyOpen = wlParserMatch(p, WlKind_TkCurlyOpen);
	blk.statements = listNew();
	p->sectionStart = true;
	while (wlParserPeek(p).kind != WlKind_TkCurlyClose && wlParserPeek(p).kind != WlKind_EOF) {
		WlToken tk;
		if (options == BlockParseStatements) {
			tk = wlParseStatement(p);
			listPush(&blk.statements, tk);
		} else {
			tk = wlParseDeclaration(p, false);
			listPush(&blk.statements, tk);
		}
		if (tk.kind != WlKind_StUse) p->sectionStart = false;
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

	WlToken tk = {.kind = WlKind_StImport, .valuePtr = imp, .span = spanFromTokens(im.import, im.semicolon)};
	return tk;
}

WlToken wlParseFunction(WlParser *p)
{
	int errorCount = listLen(p->diagnostics);

	WlSyntaxFunction fn = {0};

	WlToken first = {.kind = WlKind_Missing};

	if (wlParserPeek(p).kind == WlKind_KwExport) {
		fn.export = wlParserMatch(p, WlKind_KwExport);
		first = fn.export;
	} else {
		fn.export = (WlToken){.kind = WlKind_Missing};
	}

	fn.type = wlParserMatch(p, WlKind_Symbol);
	if (first.kind == WlKind_Missing) first = fn.type;

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
	fn.body = wlParseBlock(p, BlockParseStatements);
	WlSyntaxFunction *fnp = arenaMalloc(sizeof(WlSyntaxFunction), &p->arena);
	*fnp = fn;

	bool hasError = errorCount != listLen(p->diagnostics);
	WlToken tk = (WlToken){.kind = hasError ? WlKind_Bad : WlKind_StFunction,
						   .valuePtr = fnp,
						   .span = spanFromTokens(first, fn.body.curlyClose)};

	return tk;
}

WlToken wlParseNamespace(WlParser *p)
{
	WlSyntaxNamespace ns = {0};

	ns.namespace = wlParserMatch(p, WlKind_KwNamespace);

	ns.path = wlParseReferencePath(p);
	ns.body = wlParseBlock(p, BlockParseDeclarations);

	WlSyntaxNamespace *nsp = arenaMalloc(sizeof(WlSyntaxNamespace), &p->arena);
	*nsp = ns;

	WlToken tk = (WlToken){.kind = WlKind_StNamespace,
						   .valuePtr = nsp,
						   .span = spanFromTokens(ns.namespace, ns.body.curlyClose)};
	return tk;
}

WlToken wlParseUse(WlParser *p)
{
	WlSyntaxUse us = {0};

	us.use = wlParserMatch(p, WlKind_KwUse);

	us.path = wlParseReferencePath(p);
	us.semicolon = wlParserMatch(p, WlKind_TkSemicolon);

	WlSyntaxUse *usp = arenaMalloc(sizeof(WlSyntaxUse), &p->arena);
	*usp = us;

	WlSpan span = spanFromTokens(us.use, us.semicolon);

	if (!p->sectionStart) {
		WlDiagnostic d = {.kind = useAfterSectionStartDiagnostic, .span = span};
		listPush(&p->diagnostics, d);
	}

	WlToken tk = (WlToken){
		.kind = WlKind_StUse,
		.valuePtr = usp,
		.span = span,
	};
	return tk;
}

WlToken wlParseDeclaration(WlParser *p, bool topLevel)
{
	WlToken tk;
	switch (wlParserPeek(p).kind) {
	case WlKind_KwUse: tk = wlParseUse(p); break;
	case WlKind_KwImport: tk = wlParseImport(p); break;
	case WlKind_KwNamespace: tk = wlParseNamespace(p); break;
	default: tk = wlParseFunction(p); break;
	}
	if (topLevel) wlParserAddTopLevelStatement(p, tk);
	return tk;
}

void wlParse(WlParser *p)
{
	p->sectionStart = true;
	while (wlParserPeek(p).kind != WlKind_EOF) {
		WlToken tk = wlParseDeclaration(p, true);
		if (tk.kind != WlKind_StUse) p->sectionStart = false;
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
	for (int i = 0; i < listLen(blk.statements); i++) {
		WlToken st = blk.statements[i];
		wlPrint(st);
		printf("\n");
	}
	wlPrint(blk.curlyClose);
	printf("\n");
}

void wlPrintReferencePath(List(WlToken) path)
{
	for (int i = 0; i < listLen(path); i++) {
		wlPrint(path[i]);
	}
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
		wlPrintReferencePath(call.path);
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
		printf("<ref>::");
		wlPrintReferencePath(tk.valuePtr);
	} break;

	default: PANIC("Unhandled print function for %s %d", WlKindText[tk.kind], tk.kind); break;
	}
}
