#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_lexer
#endif

#include <sti_test.h>

#include <parser.c>

void test_lex_individual_operator(Str op, WlKind expected)
{
	WlLexer l = wlLexerCreate(op);
	wlLexerLexTokens(&l);
	test_assert_impl(strFormat("lexes single token '%.*s'", STRPRINT(op)).buf,
					 l.tokenCount == 1 && l.tokens[0].kind == expected);
	wlLexerFree(&l);
}

void test_lex_legal_symbol_name(Str symbol)
{
	WlLexer l = wlLexerCreate(symbol);
	wlLexerLexTokens(&l);
	test_assert_impl(strFormat("symbol is legal '%.*s'", STRPRINT(symbol)).buf,
					 l.tokenCount == 1 && l.tokens[0].kind == WlKind_Symbol);
	wlLexerFree(&l);
}

void test_lex_illegal_symbol_name(Str symbol)
{
	WlLexer l = wlLexerCreate(symbol);
	wlLexerLexTokens(&l);
	test_assert_impl(strFormat("symbol is illegal '%.*s'", STRPRINT(symbol)).buf,
					 l.tokenCount >= 1 && l.tokens[0].kind != WlKind_Symbol);
	wlLexerFree(&l);
}

void test_lex_individual_number(Str number, int expected)
{
	WlLexer l = wlLexerCreate(number);
	wlLexerLexTokens(&l);
	test_assert_impl(strFormat("lexes single number '%.*s' as %d", STRPRINT(number), expected).buf,
					 l.tokenCount == 1 && l.tokens[0].kind == WlKind_Number && l.tokens[0].valueNum == expected);
	wlLexerFree(&l);
}

void test_lexer()
{
	test_section("lexer");

	{
		test_that("lexer lexes individual operators");

		char *operators[] = {
			"+", "-", "*", "/", "=", "==", "!=",
		};
		WlKind expected[] = {
			WlKind_OpPlus,	 WlKind_OpMinus,		WlKind_OpStar,		 WlKind_OpSlash,
			WlKind_OpEquals, WlKind_OpDoubleEquals, WlKind_OpBangEquals,
		};

		for (int i = 0; i < sizeof(operators) / sizeof(char *); i++) {
			test_lex_individual_operator(strFromCstr(operators[i]), expected[i]);
		}
	}

	test_that("lexer lexes space separated operators")
	{

		Str operators = STR("+ - * / = == !=");

		WlKind expected[] = {
			WlKind_OpPlus,	 WlKind_OpMinus,		WlKind_OpStar,		 WlKind_OpSlash,
			WlKind_OpEquals, WlKind_OpDoubleEquals, WlKind_OpBangEquals,
		};

		int tokenCount = sizeof(expected) / sizeof(WlKind);

		WlLexer l = wlLexerCreate(operators);
		wlLexerLexTokens(&l);

		test_assert(strFormat("lexes '%d' tokens").buf, l.tokenCount == tokenCount);

		for (int i = 0; i < sizeof(expected) / sizeof(WlKind); i++) {
			test_assert(strFormat("expected token '%d' to be '%s' but got '%s'", i, WlKindText[expected[i]],
								  WlKindText[l.tokens[i].kind])
							.buf,
						l.tokens[i].kind == expected[i]);
		}

		wlLexerFree(&l);
	}

	test_that("expected symbol names are legal")
	{

		char *names[] = {
			"foo", "_bar", "Baz123", "something_like_this", "a```",
		};

		for (int i = 0; i < sizeof(names) / sizeof(char *); i++)
			test_lex_legal_symbol_name(strFromCstr(names[i]));
	}

	test_that("unexpected symbol names are illegal")
	{

		char *names[] = {"$foo", "~Bar", "()[]!", "@gh"};

		for (int i = 0; i < sizeof(names) / sizeof(char *); i++)
			test_lex_illegal_symbol_name(strFromCstr(names[i]));
	}

	test_that("lexer lexes individual numers")
	{

		char *numbers[] = {"1", "35", "0xF00", "0b10110", "3837"};

		int expected[] = {1, 35, 0xF00, 22, 3837};

		for (int i = 0; i < sizeof(numbers) / sizeof(char *); i++) {
			test_lex_individual_number(strFromCstr(numbers[i]), expected[i]);
		}
	}
}
