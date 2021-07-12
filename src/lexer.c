#include <sti_base.h>

typedef enum
{
	WlKind_Bad,
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
} WlKind;

char *WlKindText[] = {
	"<bad>",
	// comment to make the formatter behave
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
};

typedef struct {
	WlKind kind;
	union {
		int valueNum;
		Str valueStr;
	};
} WlToken;

typedef struct {
	Str source;
	int index;
	WlToken *tokens;
	int tokenCount;
} WlLexer;

bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\r' == c == '\n'; }
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
		.tokens = tokens,
		.tokenCount = 0,
	};
}

void wlLexerFree(WlLexer *lex)
{
	free(lex->tokens);
	lex->tokens = NULL;
	lex->tokenCount = 0;
}

char wlLexerCurrent(WlLexer *lex) { return lex->index < lex->source.len ? lex->source.buf[lex->index] : NULL; }
char wlLexerLookahead(WlLexer *lex, int n)
{
	return lex->index + n < lex->source.len ? lex->source.buf[lex->index + n] : NULL;
}

void wlLexerPushBasic(WlLexer *lex, int len, WlKind kind)
{
	lex->tokens[lex->tokenCount++] = (WlToken){.kind = kind};
	lex->index += len;
}

void wlLexerTrim(WlLexer *lex)
{
	while (isWhitespace(wlLexerCurrent(lex)))
		lex->index++;
}

void wlLexerLexTokens(WlLexer *lex)
{
	char current;
	do {
		wlLexerTrim(lex);
		current = wlLexerCurrent(lex);
		if (!current) break;
		if (lex->tokenCount >= 0xFF) PANIC("Too many tokens!");

		switch (current) {
		case '+': wlLexerPushBasic(lex, 1, WlKind_OpPlus); break;
		case '-': wlLexerPushBasic(lex, 1, WlKind_OpMinus); break;
		case '*': wlLexerPushBasic(lex, 1, WlKind_OpStar); break;
		case '/': wlLexerPushBasic(lex, 1, WlKind_OpSlash); break;
		case '=':
			if (wlLexerLookahead(lex, 1) == '=')
				wlLexerPushBasic(lex, 2, WlKind_OpDoubleEquals);
			else
				wlLexerPushBasic(lex, 2, WlKind_OpEquals);
			break;
		case '!':
			if (wlLexerLookahead(lex, 1) == '=') {
				wlLexerPushBasic(lex, 2, WlKind_OpBangEquals);
			} else {
				goto unmatched;
			}
			break;
		case '(': wlLexerPushBasic(lex, 1, WlKind_TkParenOpen); break;
		case ')': wlLexerPushBasic(lex, 1, WlKind_TkParenClose); break;
		case '{': wlLexerPushBasic(lex, 1, WlKind_TkCurlyOpen); break;
		case '}': wlLexerPushBasic(lex, 1, WlKind_TkCurlyClose); break;
		case ',': wlLexerPushBasic(lex, 1, WlKind_TkComma); break;
		case ';': wlLexerPushBasic(lex, 1, WlKind_TkSemicolon); break;
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
			int start = lex->index;
			int value = 0;
			if (current == '0') {
				switch (wlLexerLookahead(lex, 1)) {
				case 'x': {
					lex->index += 2;
					while (isHexDigit(wlLexerCurrent(lex))) {
						char d = wlLexerCurrent(lex);
						u8 digitValue;
						if (isLowercaseLetter(d))
							digitValue = d - 'a' + 10;
						else if (isUppercaseLetter(d))
							digitValue = d - 'A' + 10;
						else
							digitValue = d - '0';

						value <<= 4;
						value += digitValue;
						lex->index++;
					}
				}
				case 'b': {
					lex->index += 2;
					while (isBinaryDigit(wlLexerCurrent(lex))) {
						u8 digitValue = wlLexerCurrent(lex) - '0';
						value <<= 1;
						value += digitValue;
						lex->index++;
					}
				}
				default: {
					// matched 0
					lex->index++;
				}
				}

			} else {
				while (isDigit(wlLexerCurrent(lex))) {
					u8 digitValue = wlLexerCurrent(lex) - '0';
					value *= 10;
					value += digitValue;
					lex->index++;
				}
			}
			lex->tokens[lex->tokenCount++] = (WlToken){.kind = WlKind_Number, .valueNum = value};
		} break;
		default: {
		unmatched : {
		}
			bool wasKeyword = false;
			if (isLowercaseLetter(current)) {
				for (int i = WlKind_Keywords_Start + 1; i < WlKind_Keywords_End; i++) {
					// TODO: bake into Str's beforehand so we don't have to convert from cstr
					Str keyword = strFromCstr(WlKindText[i]);
					if (strEqual(strSlice(lex->source, lex->index, keyword.len), keyword)) {
						lex->index += keyword.len;
						lex->tokens[lex->tokenCount++] = (WlToken){.kind = i};
						wasKeyword = true;
						break;
					}
				}
			}

			if (wasKeyword) break;

			if (isSymbolStart(current)) {
				int start = lex->index;
				while (isSymbol(wlLexerCurrent(lex)))
					lex->index++;
				Str symbolName = strSlice(lex->source, start, lex->index - start);
				// printf("sym: %.*s\n", STRPRINT(symbolName));
				lex->tokens[lex->tokenCount++] = (WlToken){.kind = WlKind_Symbol, .valueStr = symbolName};
			} else {
				// PANIC("Unexpected character '%c'", current);
				lex->index++;
				lex->tokens[lex->tokenCount++] = (WlToken){.kind = WlKind_Bad};
			}
		} break;
		}

	} while (current);
}
