#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_parser
#endif

#include <sti_test.h>

#include <walc.h>

void test_token_lexing()
{
	test_section("parser token lexing");

	{

		char *operators[] = {
			"+", "-", "*", "/", "%", "<<", ">>", ">", ">=", "<", "<=", "==", "!=", "&", "^", "|", "&&", "||", "=",
		};

		WlKind expectedKind[] = {
			WlKind_OpPlus,
			WlKind_OpMinus,
			WlKind_OpStar,
			WlKind_OpSlash,
			WlKind_OpPercent,
			WlKind_OpLessLess,
			WlKind_OpGreaterGreater,
			WlKind_OpGreater,
			WlKind_OpGreaterEquals,
			WlKind_OpLess,
			WlKind_OpLessEquals,
			WlKind_OpEuqualsEquals,
			WlKind_OpBangEquals,
			WlKind_OpAmpersand,
			WlKind_OpCaret,
			WlKind_OpPipe,
			WlKind_OpAmpersandAmpersand,
			WlKind_OpPipePipe,
			WlKind_OpEquals,
		};

		test_theory("lexer lexes individual operators", char *, operators)
		{
			Str operator= strFromCstr(operators[i]);
			WlKind expected = expectedKind[i];

			WlLexer l = wlLexerCreate(STREMPTY, operator);
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
			WlKind_OpPlus,	 WlKind_OpMinus,		 WlKind_OpStar,		  WlKind_OpSlash,
			WlKind_OpEquals, WlKind_OpEuqualsEquals, WlKind_OpBangEquals,
		};

		int tokenCount = sizeof(expected) / sizeof(WlKind);

		WlLexer l = wlLexerCreate(STREMPTY, operators);
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
			WlLexer l = wlLexerCreate(STREMPTY, symbol);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert(cstrFormat("'%.*s' is a single token", STRPRINT(symbol)), listLen(tokens) == 1);
			test_assert(cstrFormat("'%.*s' is a symbol", STRPRINT(symbol)), tokens[0].kind == WlKind_Symbol);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}

	{
		char *names[] = {"$foo", "~Bar", "()[]!", "@gh", "foo;"};
		test_theory("unexpected symbol names are illegal", char *, names)
		{
			Str symbol = strFromCstr(names[i]);
			WlLexer l = wlLexerCreate(STREMPTY, symbol);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert(cstrFormat("'%.*s' matches at least one token", STRPRINT(symbol)), listLen(tokens) >= 1);
			test_assert(cstrFormat("'%.*s' is not a symbol", STRPRINT(symbol)),
						listLen(tokens) > 1 || tokens[0].kind != WlKind_Symbol);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}

	{
		char *numbers[] = {"1", "35", "0xF00", "0b10110", "3837", "101010"};
		int expectedValues[] = {1, 35, 0xF00, 22, 3837, 101010};
		test_theory("lexer lexes individual numbers", char *, numbers)
		{
			Str number = strFromCstr(numbers[i]);
			int expected = expectedValues[i];

			WlLexer l = wlLexerCreate(STREMPTY, number);
			List(WlToken) tokens = wlLexerLexTokens(&l);
			test_assert(cstrFormat("lexes single number '%.*s' as %d", STRPRINT(number), expected),
						listLen(tokens) == 1 && tokens[0].kind == WlKind_Number && tokens[0].valueNum == expected);

			listFree(&tokens);
			wlLexerFree(&l);
		}
	}

	test_that("lexer lexes assignment sequence without spaces")
	{
		Str source = STR("i32 a=101010;");
		WlLexer l = wlLexerCreate(STREMPTY, source);
		List(WlToken) tokens = wlLexerLexTokens(&l);

		test_assert("lexes 5 tokens", listLen(tokens) == 5);
		test_assert("token 1 is a symbol", tokens[0].kind == WlKind_Symbol);
		test_assert("token 2 is a symbol", tokens[1].kind == WlKind_Symbol);
		test_assert("token 3 is an equals", tokens[2].kind == WlKind_OpEquals);
		test_assert("token 4 is a number", tokens[3].kind == WlKind_Number);
		test_assert("token 5 is a semicolon", tokens[4].kind == WlKind_TkSemicolon);

		listFree(&tokens);
		wlLexerFree(&l);
	}

	test_that("lexer skips single line comments")
	{
		Str source = STR("// this is a comment\n10");
		WlLexer l = wlLexerCreate(STREMPTY, source);

		List(WlToken) tokens = wlLexerLexTokens(&l);
		test_assert("finds a single token", listLen(tokens) == 1);
		test_assert("token is a number", tokens[0].kind = WlKind_Number);

		listFree(&tokens);
		wlLexerFree(&l);
	}

	test_that("lexer skips multi line comments")
	{
		Str source =
			STR("/* this is a multi line\n\ncomment comment */ \n"
				"10\n/*another /*nested\n*/ one*/\n");

		WlLexer l = wlLexerCreate(STREMPTY, source);

		List(WlToken) tokens = wlLexerLexTokens(&l);
		test_assert("finds a single token", listLen(tokens) == 1);
		test_assert("token is a number", tokens[0].kind = WlKind_Number);

		listFree(&tokens);
		wlLexerFree(&l);
	}

	test_that("lexer reports diagnostic on unterminated multiline comment")
	{
		Str source = STR("/*Hello world");

		WlLexer l = wlLexerCreate(STREMPTY, source);

		List(WlToken) tokens = wlLexerLexTokens(&l);
		test_assert("reported a diagnostic", listLen(l.diagnostics) == 1);
		test_assert("it is an unterminated comment diagnostic", l.diagnostics[0].kind == UnterminatedCommentDiagnostic);

		listFree(&tokens);
		wlLexerFree(&l);
	}

	test_that("lexer reports diagnostic on unterminated string")
	{
		Str source = STR("\"Hello world");

		WlLexer l = wlLexerCreate(STREMPTY, source);

		List(WlToken) tokens = wlLexerLexTokens(&l);
		test_assert("reported a diagnostic", listLen(l.diagnostics) == 1);
		test_assert("it is an unterminated string diagnostic", l.diagnostics[0].kind == UnterminatedStringDiagnostic);
		test_assert("it returned a token", listLen(tokens) == 1);
		test_assert("the token is a bad token", tokens[0].kind == WlKind_Bad);

		listFree(&tokens);
		wlLexerFree(&l);
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
		WlParser p = wlParserCreate(STREMPTY, source);

		WlToken t = wlParseExpression(&p);
		test_assert("call expression is found", t.kind == WlKind_StCall);
		test_assert("No arguments are found", listLen(((WlSyntaxCall *)t.valuePtr)->args) == 0);

		wlParserFree(&p);
	}

	BinaryExpressionTestData binaryExpressions[] = {
		{STR("10 + 20"), 10, WlKind_OpPlus, 20},
		//
		{STR("5 - 3"), 5, WlKind_OpMinus, 3},
		{STR("2 * 4"), 2, WlKind_OpStar, 4},
		{STR("10 / 2"), 10, WlKind_OpSlash, 2},
		{STR("13 % 3"), 13, WlKind_OpPercent, 3},

		{STR("13 == 3"), 13, WlKind_OpEuqualsEquals, 3},
		{STR("1 != 2"), 1, WlKind_OpBangEquals, 2},
	};

	test_theory("Parser parses binary expressions", BinaryExpressionTestData, binaryExpressions)
	{
		BinaryExpressionTestData data = binaryExpressions[i];

		WlParser p = wlParserCreate(STREMPTY, data.source);
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

			WlParser p = wlParserCreate(STREMPTY, data.source);
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

		test_that("parser reports diagnostic on illegal primary expression")
		{
			WlParser p = wlParserCreate(STREMPTY, STR("+"));
			WlToken t = wlParsePrimaryExpression(&p);

			test_assert("lexer had no errors", listLen(p.lexer.diagnostics) == 0);
			test_assert("parser reported a diagnostic", listLen(p.diagnostics) == 1);
			test_assert("it is an unexpected token in primary expression diagnostic",
						p.diagnostics[0].kind == UnexpectedTokenInPrimaryExpressionDiagnostic);
			test_assert("the returned token is bad", t.kind == WlKind_Bad);

			wlParserFree(&p);
		}

		test_that("parser reports diagnostic on unexpected token")
		{
			WlParser p = wlParserCreate(STREMPTY, STR("export u0 main[]"));
			WlToken t = wlParseFunction(&p);

			test_assert("lexer had no errors", listLen(p.lexer.diagnostics) == 0);
			test_assert("parser reported a diagnostic", listLen(p.diagnostics) >= 1);
			test_assert("it is an unexpected token diagnostic", p.diagnostics[0].kind == UnexpectedTokenDiagnostic);
			test_assert("the returned token is bad", t.kind == WlKind_Bad);

			wlParserFree(&p);
		}
	}
}

void test_return_statement_parsing()
{
	test_that("Empty return gets parsed")
	{
		Str source = STR("return;");
		WlParser p = wlParserCreate(STREMPTY, source);
		WlToken t = wlParseStatement(&p);
		test_assert("Parses return statement", t.kind == WlKind_StReturnStatement);

		WlReturnStatement ret = *(WlReturnStatement *)t.valuePtr;

		test_assert("Has no expression", ret.expression.kind == WlKind_Missing);
		wlParserFree(&p);
	}

	test_that("Return with expression gets parsed")
	{
		Str source = STR("return 10 + 20;");
		WlParser p = wlParserCreate(STREMPTY, source);
		WlToken t = wlParseStatement(&p);
		test_assert("Parses return statement", t.kind == WlKind_StReturnStatement);

		WlReturnStatement ret = *(WlReturnStatement *)t.valuePtr;

		test_assert("Has expression", ret.expression.kind != WlKind_Missing);

		wlParserFree(&p);
	}
}

void test_function_parsing()
{
	test_that("parser parses parameter list")
	{
		Str source = STR("u32 a, u32 b, u32 c");
		WlParser p = wlParserCreate(STREMPTY, source);
		WlKind delimeter = WlKind_TkComma;

		List(WlToken) t = wlParseParameterList(&p);

		test_assert("parses 5 tokens", listLen(t) == 5);
		test_assert("1st token is symbol", t[0].kind == WlKind_StFunctionParameter);
		test_assert("2nd token is delimeter", t[1].kind == delimeter);
		test_assert("3rd token is symbol", t[2].kind == WlKind_StFunctionParameter);
		test_assert("4th token is delimeter", t[3].kind == delimeter);
		test_assert("5th token is symbol", t[4].kind == WlKind_StFunctionParameter);

		wlParserFree(&p);
	}

	test_that("parameter list allows optional trailing comma")
	{
		Str source = STR("u32 a, u32 b, u32 c,");
		WlParser p = wlParserCreate(STREMPTY, source);
		WlKind delimeter = WlKind_TkComma;

		List(WlToken) t = wlParseParameterList(&p);

		test_assert("parses 6 tokens", listLen(t) == 6);
		test_assert("1st token is symbol", t[0].kind == WlKind_StFunctionParameter);
		test_assert("2nd token is delimeter", t[1].kind == delimeter);
		test_assert("3rd token is symbol", t[2].kind == WlKind_StFunctionParameter);
		test_assert("4th token is delimeter", t[3].kind == delimeter);
		test_assert("5th token is symbol", t[4].kind == WlKind_StFunctionParameter);
		test_assert("6th token is delimeter", t[5].kind == delimeter);

		wlParserFree(&p);
	}
}

void test_namespace_parsing()
{
	test_that("single level namespace parses")
	{
		WlParser p = wlParserCreate(STREMPTY, STR("namespace foo {}"));
		WlToken t = wlParseNamespace(&p);

		test_assert("is a namespace", t.kind == WlKind_StNamespace);
		test_assert("encounters no errors", listLen(p.diagnostics) == 0);

		wlParserFree(&p);
	}

	test_that("multi level namespace parses")
	{
		WlParser p = wlParserCreate(STREMPTY, STR("namespace foo.bar.baz {}"));
		WlToken t = wlParseNamespace(&p);

		test_assert("is a namespace", t.kind == WlKind_StNamespace);
		test_assert("encounters no errors", listLen(p.diagnostics) == 0);

		wlParserFree(&p);
	}
}

void test_parser()
{
	test_section("parser");

	test_token_lexing();
	test_expression_parsing();
	test_return_statement_parsing();
	test_function_parsing();
	test_namespace_parsing();
}
