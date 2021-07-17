#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_parser
#endif

#include <sti_test.h>

#include <parser.c>

void test_lex_individual_operator(Str op, WlKind expected)
{
	WlLexer l = wlLexerCreate(op);
	List(WlToken) tokens = wlLexerLexTokens(&l);
	test_assert_impl(strFormat("lexes single token '%.*s'", STRPRINT(op)).buf,
					 listLen(tokens) == 1 && tokens[0].kind == expected);
	listFree(&tokens);
	wlLexerFree(&l);
}

void test_lex_legal_symbol_name(Str symbol)
{
	WlLexer l = wlLexerCreate(symbol);
	List(WlToken) tokens = wlLexerLexTokens(&l);
	test_assert_impl(strFormat("symbol is legal '%.*s'", STRPRINT(symbol)).buf,
					 listLen(tokens) == 1 && tokens[0].kind == WlKind_Symbol);

	listFree(&tokens);
	wlLexerFree(&l);
}

void test_lex_illegal_symbol_name(Str symbol)
{
	WlLexer l = wlLexerCreate(symbol);
	List(WlToken) tokens = wlLexerLexTokens(&l);
	test_assert_impl(strFormat("symbol is illegal '%.*s'", STRPRINT(symbol)).buf,
					 listLen(tokens) >= 1 && tokens[0].kind != WlKind_Symbol);

	listFree(&tokens);
	wlLexerFree(&l);
}

void test_lex_individual_number(Str number, int expected)
{
	WlLexer l = wlLexerCreate(number);
	List(WlToken) tokens = wlLexerLexTokens(&l);
	test_assert_impl(strFormat("lexes single number '%.*s' as %d", STRPRINT(number), expected).buf,
					 listLen(tokens) == 1 && tokens[0].kind == WlKind_Number && tokens[0].valueNum == expected);

	listFree(&tokens);
	wlLexerFree(&l);
}

void test_token_lexing()
{
	test_section("parser token lexing");

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

	test_that("lexer lexes individual numbers")
	{
		char *numbers[] = {"1", "35", "0xF00", "0b10110", "3837"};

		int expected[] = {1, 35, 0xF00, 22, 3837};

		for (int i = 0; i < sizeof(numbers) / sizeof(char *); i++) {
			test_lex_individual_number(strFromCstr(numbers[i]), expected[i]);
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
