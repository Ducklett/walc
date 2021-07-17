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
			test_assert_impl(cstrFormat("lexes single token '%.*s'", STRPRINT(operator)),
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

		test_assert(cstrFormat("lexes '%d' tokens"), listLen(tokens) == tokenCount);

		for (int i = 0; i < sizeof(expected) / sizeof(WlKind); i++) {
			test_assert(cstrFormat("expected token '%d' to be '%s' but got '%s'", i, WlKindText[expected[i]],
								   WlKindText[tokens[i].kind]),
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
			test_assert(cstrFormat("'%.*s' is a single token", STRPRINT(symbol)), listLen(tokens) == 1);
			test_assert(cstrFormat("'%.*s' is a symbol", STRPRINT(symbol)), tokens[0].kind == WlKind_Symbol);

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
			test_assert(cstrFormat("'%.*s' matches at least one token", STRPRINT(symbol)), listLen(tokens) >= 1);
			test_assert(cstrFormat("'%.*s' is not a symbol", STRPRINT(symbol)), tokens[0].kind != WlKind_Symbol);

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
			test_assert(cstrFormat("lexes single number '%.*s' as %d", STRPRINT(number), expected),
						listLen(tokens) == 1 && tokens[0].kind == WlKind_Number && tokens[0].valueNum == expected);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}
}

typedef struct {
	Str source;
	int left;
	WlKind operator;
	int right;
} BinaryExpressionTestData;

typedef struct {
	Str source;
	WlKind operator1;
	WlKind operator2;
	int val1;
	int val2;
	int val3;
	bool firstOperatorHasHigherPrecedence;
} OperatorPrecedenceData;

void test_expression_parsing()
{
	test_section("parser expressions");

	test_that("Parser parses call expression")
	{
		Str source = STR("hello()");
		WlParser p = wlParserCreate(source);

		WlToken t = wlParseExpression(&p);
		test_assert("call expression is found", t.kind == WlKind_StCall);
		test_assert("No arguments are found", ((WlSyntaxCall *)t.valuePtr)->arg.kind == WlKind_Missing);

		wlParserFree(&p);
	}

	BinaryExpressionTestData binaryExpressions[] = {
		{STR("10 + 20"), 10, WlKind_OpPlus, 20},
		//
		{STR("5 - 3"), 5, WlKind_OpMinus, 3},
		{STR("2 * 4"), 2, WlKind_OpStar, 4},
		{STR("10 / 2"), 10, WlKind_OpSlash, 2},
		{STR("13 % 3"), 13, WlKind_OpPercent, 3},
	};

	test_theory("Parser parses binary expressions", BinaryExpressionTestData, binaryExpressions)
	{
		BinaryExpressionTestData data = binaryExpressions[i];

		WlParser p = wlParserCreate(data.source);
		WlToken t = wlParseExpression(&p);

		test_assert(cstrFormat("%.*s parses as binary epxression", STRPRINT(data.source)),
					t.kind == WlKind_StBinaryExpression);

		WlBinaryExpression be = *(WlBinaryExpression *)t.valuePtr;

		test_assert(cstrFormat("%.*s left is a number", STRPRINT(data.source)), be.left.kind == WlKind_Number);
		test_assert(cstrFormat("%.*s left has the right value", STRPRINT(data.source)), be.left.valueNum == data.left);

		test_assert(cstrFormat("%.*s operator has the right kind", STRPRINT(data.source)),
					be.operator.kind == data.operator);

		test_assert(cstrFormat("%.*s right is a number", STRPRINT(data.source)), be.right.kind == WlKind_Number);
		test_assert(cstrFormat("%.*s right has the right value", STRPRINT(data.source)),
					be.right.valueNum == data.right);

		wlParserFree(&p);
	}

	{
		OperatorPrecedenceData expressions[] = {
			{STR("1 + 2 + 3"), WlKind_OpPlus, WlKind_OpPlus, 1, 2, 3, .firstOperatorHasHigherPrecedence = true},
			{STR("2 + 3 * 4"), WlKind_OpPlus, WlKind_OpStar, 2, 3, 4, .firstOperatorHasHigherPrecedence = false},
			{STR("10 / 2 - 1"), WlKind_OpSlash, WlKind_OpMinus, 10, 2, 1, .firstOperatorHasHigherPrecedence = true},
		};
		test_theory("Parser handles operator precedence", OperatorPrecedenceData, expressions)
		{
			// printf("%s", )

			OperatorPrecedenceData data = expressions[i];

			WlParser p = wlParserCreate(data.source);
			WlToken t = wlParseExpression(&p);

			test_assert(cstrFormat("%.*s parses as binary epxression", STRPRINT(data.source)),
						t.kind == WlKind_StBinaryExpression);

			WlBinaryExpression be = *(WlBinaryExpression *)t.valuePtr;

			if (data.firstOperatorHasHigherPrecedence) {
				//     *
				//    / \
				//   *   n
				//  / \
				// n   n

				test_assert(cstrFormat("%.*s top operator has the right kind", STRPRINT(data.source)),
							be.operator.kind = data.operator2);

				test_assert(cstrFormat("%.*s left is a binary expression", STRPRINT(data.source)),
							be.left.kind == WlKind_StBinaryExpression);

				WlBinaryExpression bel = *(WlBinaryExpression *)be.left.valuePtr;

				test_assert(cstrFormat("%.*s left is a number", STRPRINT(data.source)), bel.left.kind == WlKind_Number);

				test_assert(cstrFormat("%.*s left has the right value", STRPRINT(data.source)),
							bel.left.valueNum == data.val1);

				test_assert(cstrFormat("%.*s left operator has the right kind", STRPRINT(data.source)),
							bel.operator.kind == data.operator1);

				test_assert(cstrFormat("%.*s center is a number", STRPRINT(data.source)),
							bel.right.kind == WlKind_Number);

				test_assert(cstrFormat("%.*s center has the right value", STRPRINT(data.source)),
							bel.right.valueNum == data.val2);

				test_assert(cstrFormat("%.*s right is a number", STRPRINT(data.source)),
							be.right.kind == WlKind_Number);

				test_assert(cstrFormat("%.*s right has the right value", STRPRINT(data.source)),
							be.right.valueNum == data.val3);

			} else {
				//     *
				//    / \
				//   n   *
				//      / \
				//     n   n

				test_assert(cstrFormat("%.*s top operator has the right kind", STRPRINT(data.source)),
							be.operator.kind = data.operator1);

				test_assert(cstrFormat("%.*s right is a binary expression", STRPRINT(data.source)),
							be.right.kind == WlKind_StBinaryExpression);

				WlBinaryExpression ber = *(WlBinaryExpression *)be.right.valuePtr;

				test_assert(cstrFormat("%.*s left is a number", STRPRINT(data.source)), be.left.kind == WlKind_Number);

				test_assert(cstrFormat("%.*s left has the right value", STRPRINT(data.source)),
							be.left.valueNum == data.val1);

				test_assert(cstrFormat("%.*s center is a number", STRPRINT(data.source)),
							ber.left.kind == WlKind_Number);

				test_assert(cstrFormat("%.*s center has the right value", STRPRINT(data.source)),
							ber.left.valueNum == data.val2);

				test_assert(cstrFormat("%.*s right operator has the right kind", STRPRINT(data.source)),
							ber.operator.kind == data.operator2);

				test_assert(cstrFormat("%.*s right is a number", STRPRINT(data.source)),
							ber.right.kind == WlKind_Number);

				test_assert(cstrFormat("%.*s right has the right value", STRPRINT(data.source)),
							ber.right.valueNum == data.val3);
			}

			wlParserFree(&p);
		}
	}
}

void test_parser()
{
	test_section("parser");

	test_token_lexing();
	test_expression_parsing();
}
