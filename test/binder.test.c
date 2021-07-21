#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_binder
#endif

#include <sti_test.h>

#include <walc.h>

void test_binder_symbols()
{
	test_section("binder symbols");

	test_that("Variables from outer scopes can be shadowed")
	{
		WlBinder b = wlBinderCreate(NULL, 0);

		WlSymbol *a = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);

		test_assert("the first variable is created", a != NULL);

		WlCreateAndPushScope(&b);

		WlSymbol *a2 = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);

		test_assert("the second variable is created", a2 != NULL);

		WlPopScope(&b);

		test_assert("no diagnostics were reported", listLen(b.diagnostics) == 0);

		wlBinderFree(&b);
	}

	test_that("Variables in the same scope cannot be shadowed")
	{
		WlBinder b = wlBinderCreate(NULL, 0);

		WlSymbol *a = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);

		test_assert("the first variable is created", a != NULL);

		WlSymbol *a2 = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);

		test_assert("the second variable is not created", a2 == NULL);

		test_assert("a diagnostic is reported",
					listLen(b.diagnostics) >= 1 && b.diagnostics[0].kind == VariableAlreadyExistsDiagnostic);

		wlBinderFree(&b);
	}

	test_that("The root cannot access variables declared in other scopes")
	{
		WlBinder b = wlBinderCreate(NULL, 0);

		WlCreateAndPushScope(&b);
		WlSymbol *a = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);
		test_assert("the variable is created", a != NULL);
		WlPopScope(&b);

		WlSymbol *a2 = wlFindVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);

		test_assert("the variable is not found", a2 == NULL);

		test_assert("a diagnostic is reported",
					listLen(b.diagnostics) >= 1 && b.diagnostics[0].kind == VariableNotFoundDiagnostic);

		wlBinderFree(&b);
	}

	test_that("Other scopes can access variables declared in the root")
	{
		WlBinder b = wlBinderCreate(NULL, 0);

		WlSymbol *a = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);
		test_assert("the variable is created", a != NULL);

		WlCreateAndPushScope(&b);
		WlSymbol *a2 = wlFindVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);
		test_assert("the is variable not found", a2 != NULL);
		WlPopScope(&b);

		wlBinderFree(&b);
	}

	test_that("Other scopes cannot access variables declared in a different scope")
	{
		WlBinder b = wlBinderCreate(NULL, 0);

		WlCreateAndPushScope(&b);
		WlSymbol *a = wlPushVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);
		test_assert("the variable is created", a != NULL);
		WlPopScope(&b);

		WlCreateAndPushScope(&b);
		WlSymbol *a2 = wlFindVariable(&b, SPANEMPTY, STR("a"), WlBType_i32);
		test_assert("the variable not found", a2 == NULL);
		WlPopScope(&b);

		wlBinderFree(&b);
	}
}

void test_binder()
{
	test_section("binder");
	test_binder_symbols();
}
