#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_lexer
#endif

#include <sti_test.h>

#include <lexer.c>

void test_lex_individual_operator(Str op)
{
	char msg[0xFF] = {0};
	test_assert(strFormat("lexes '%.*s'", STRPRINT(op)).buf, false);
}
void test_lexer()
{
	test_section("lexer");

	{
		test_that("lexer lexes individual operators");

		char *operators[] = {
			"+", "-", "*", "/", "=", "==", "!=",
		};

		for (int i = 0; i < sizeof(operators) / sizeof(char *); i++) {
			test_lex_individual_operator(strFromCstr(operators[i]));
		}

		test_assert("this will fail", false);
	}
}
