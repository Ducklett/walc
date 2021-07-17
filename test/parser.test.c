#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_parser
#endif

#include <sti_test.h>

#include <parser.c>

void test_token_lexing()
{
	test_section("parser token lexing");

	{

		char *operators[] = {
			"+", "-", "*", "/", "=", "==", "!=",
		};

		WlKind expectedKind[] = {
			WlKind_OpPlus,	 WlKind_OpMinus,		WlKind_OpStar,		 WlKind_OpSlash,
			WlKind_OpEquals, WlKind_OpDoubleEquals, WlKind_OpBangEquals,
		};

		test_theory("lexer lexes individual operators", char *, operators)
		{
			Str operator= strFromCstr(operators[i]);
			WlKind expected = expectedKind[i];

			WlLexer l = wlLexerCreate(operator);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert_impl(strFormat("lexes single token '%.*s'", STRPRINT(operator)).buf,
							 listLen(tokens) == 1 && tokens[0].kind == expected);
			listFree(&tokens);
			wlLexerFree(&l);
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
		List(WlToken) tokens = wlLexerLexTokens(&l);

		test_assert(strFormat("lexes '%d' tokens").buf, listLen(tokens) == tokenCount);

		for (int i = 0; i < sizeof(expected) / sizeof(WlKind); i++) {
			test_assert(strFormat("expected token '%d' to be '%s' but got '%s'", i, WlKindText[expected[i]],
								  WlKindText[tokens[i].kind])
							.buf,
						tokens[i].kind == expected[i]);
		}

		wlLexerFree(&l);
	}

	{
		char *names[] = {
			"foo", "_bar", "Baz123", "something_like_this", "a```",
		};
		test_theory("expected symbol names are legal", char *, names)
		{
			Str symbol = strFromCstr(names[i]);
			WlLexer l = wlLexerCreate(symbol);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert(strFormat("'%.*s' is a single token", STRPRINT(symbol)).buf, listLen(tokens) == 1);
			test_assert(strFormat("'%.*s' is a symbol", STRPRINT(symbol)).buf, tokens[0].kind == WlKind_Symbol);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}

	{
		char *names[] = {"$foo", "~Bar", "()[]!", "@gh"};
		test_theory("unexpected symbol names are illegal", char *, names)
		{
			Str symbol = strFromCstr(names[i]);
			WlLexer l = wlLexerCreate(symbol);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert(strFormat("'%.*s' matches at least one token", STRPRINT(symbol)).buf, listLen(tokens) >= 1);
			test_assert(strFormat("'%.*s' is not a symbol", STRPRINT(symbol)).buf, tokens[0].kind != WlKind_Symbol);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}

	{
		char *numbers[] = {"1", "35", "0xF00", "0b10110", "3837"};
		int expectedValues[] = {1, 35, 0xF00, 22, 3837};
		test_theory("lexer lexes individual numbers", char *, numbers)
		{
			Str number = strFromCstr(numbers[i]);
			int expected = expectedValues[i];

			WlLexer l = wlLexerCreate(number);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert(strFormat("lexes single number '%.*s' as %d", STRPRINT(number), expected).buf,
						listLen(tokens) == 1 && tokens[0].kind == WlKind_Number && tokens[0].valueNum == expected);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}
}

void test_expression_parsing()
{
	test_section("parser expressions");

	test_that("parser parses call expression")
	{
		Str source = STR("hello()");
		WlParser p = wlParserCreate(source);

		WlToken t = wlParseExpression(&p);
		test_assert("call expression is found", t.kind == WlKind_StCall);
		test_assert("No arguments are found", ((WlSyntaxCall *)t.valuePtr)->arg.kind == WlKind_Missing);

		wlParserFree(&p);
	}
}

void test_parser()
{
	test_section("parser");

	test_token_lexing();
	test_expression_parsing();
}
