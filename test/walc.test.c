/*
  Tests the .wl example modules to valide that they work as expected
*/
#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_walc
#endif

#include <sti_test.h>

#include <walc.h>

void strPrintBytes(Str s)
{
	for (int i = 0; i < s.len; i++)
		printf("%.2X ", s.buf[i]);
	printf("\n");
}

void test_module_function(char *testName, char *moduleName, char *functionName, char *args, char *expected)
{
	test_that(testName)
	{
		Str filename = strFormat("examples/%s", moduleName);
		Str source;

		test_assert("File opens", fileReadAllText(filename.buf, &source));

		WlParser p = wlParserCreate(filename, source);
		wlParse(&p);

		int topLevelCount = listLen(p.topLevelDeclarations);
		WlBinder b = wlBind(p.topLevelDeclarations, topLevelCount);

		bool hasDiagnostics = listLen(p.diagnostics) || listLen(p.lexer.diagnostics);

		if (hasDiagnostics) {
			for (int i = 0; i < listLen(p.lexer.diagnostics); i++)
				diagnosticPrint(p.lexer.diagnostics[i]);
			for (int i = 0; i < listLen(p.diagnostics); i++)
				diagnosticPrint(p.diagnostics[i]);
		} else {
			Buf wasm = emitWasm(&b);

			test_assert("File saves", fileWriteAllBytes("out.wasm", wasm));

			char *command = cstrFormat("node runwasm.js %s %s", functionName, args);

			Str text;
			int exitcode = commandReadAllText(command, &text);

			if (exitcode != 0) {
				printf("%.*s\n", STRPRINT(text));
			}

			test_assert("exits with 0 exit code", exitcode == 0);

			test_assert("matches expected output", strEqual(strFromCstr(expected), text));

			free(command);
			strFree(&text);
		}

		test_assert("File compiles", !hasDiagnostics);

		wlBinderFree(&b);
		wlParserFree(&p);
		strFree(&filename);
	}
}

void test_walc()
{
	test_section("walc");

	test_section("walc helloworld");
	test_module_function("Hello world is printed", "01_helloworld.wl", "main", "", "Hello wasm 🎉");

	test_section("walc expressions");
	test_module_function("double(10) == 20", "02_expressions.wl", "double", "10", "20");
	test_module_function("halve(10) == 5", "02_expressions.wl", "halve", "10", "5");
	test_module_function("halve(3) == 1", "02_expressions.wl", "halve", "3", "1");
	test_module_function("succ(3) == 4", "02_expressions.wl", "succ", "3", "4");
	test_module_function("dec(3) == 2", "02_expressions.wl", "dec", "3", "2");

	test_module_function("foo0(3,4) == 15", "02_expressions.wl", "foo0", "3 4", "15");
	test_module_function("foo1(3,4) == 15", "02_expressions.wl", "foo1", "3 4", "15");
	test_module_function("foo2(3.2,4) == 16", "02_expressions.wl", "foo2", "3.2 4", "16");
	test_module_function("foo3(3.2,4) == 16", "02_expressions.wl", "foo3", "3.2 4", "16");
}
