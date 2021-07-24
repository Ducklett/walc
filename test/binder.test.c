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

		WlSymbol *a2 = wlFindVariable(&b, SPANEMPTY, STR("a"));

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
		WlSymbol *a2 = wlFindVariable(&b, SPANEMPTY, STR("a"));
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
		WlSymbol *a2 = wlFindVariable(&b, SPANEMPTY, STR("a"));
		test_assert("the variable not found", a2 == NULL);
		WlPopScope(&b);

		wlBinderFree(&b);
	}
}

typedef struct {
	char *source;
	WlBType type;
} ExpressionTypeData;

void test_binder_expression_type_resolution()
{
	test_section("binder expression type resolution");

	ExpressionTypeData implicitTypeExpressions[] = {
		{"1 + 1", WlBType_i64},
		{"1 / 2", WlBType_i64},
		//
		{"1 / 2.", WlBType_f64},
		{"1. / 2", WlBType_f64},
		{"1.1 + 1", WlBType_f64},
		{"1 - 1.1", WlBType_f64},
		{"1.1 * 1.2", WlBType_f64},
		//
		{"true", WlBType_bool},
		{"false", WlBType_bool},
		{"1 == 2", WlBType_bool},
		{"1.1 != 1.2", WlBType_bool},

		{"\"Hello!\"", WlBType_str},
	};

	test_theory("Expressions of implicit type resolve to the correct type", ExpressionTypeData, implicitTypeExpressions)
	{
		ExpressionTypeData data = implicitTypeExpressions[i];
		WlParser p = wlParserCreate(STREMPTY, strFromCstr(data.source));
		WlToken t = wlParseExpression(&p);

		for (int i = 0; i < listLen(p.diagnostics); i++) {
			diagnosticPrint(p.diagnostics[i]);
		}
		test_assert(cstrFormat("%s has no diagnostics", data.source), listLen(p.diagnostics) == 0);

		WlBinder b = wlBinderCreate(NULL, 0);

		WlbNode n = wlBindExpressionOfType(&b, t, WlBType_inferStrong);

		test_assert(cstrFormat("%s: Expression is of type %s", data.source, WlBTypeText[data.type]),
					n.type == data.type);
		test_assert(cstrFormat("%s: No diagnostics were reported", data.source), listLen(b.diagnostics) == 0);
	}

	ExpressionTypeData explicitTypeExpressions[] = {
		{"1 + 1", WlBType_i8},
		{"1 / 2", WlBType_u8},
		{"1 + 1", WlBType_i16},
		{"1 / 2", WlBType_u16},
		{"1 + 1", WlBType_i32},
		{"1 / 2", WlBType_u32},
		{"1 + 1", WlBType_i64},
		{"1 / 2", WlBType_u64},
		//
		{"1 / 2.", WlBType_f32},
		{"1. / 2", WlBType_f32},
		{"1.1 + 1", WlBType_f32},
		{"1 - 1.1", WlBType_f32},
		{"1.1 * 1.2", WlBType_f32},
		//
		{"true", WlBType_bool},
		{"false", WlBType_bool},
		{"1 == 2", WlBType_bool},
		{"1.1 != 1.2", WlBType_bool},

		{"\"Hello!\"", WlBType_str},
	};

	test_theory("Legal expressions of explicit type resolve without errors", ExpressionTypeData,
				explicitTypeExpressions)
	{
		ExpressionTypeData data = explicitTypeExpressions[i];
		WlParser p = wlParserCreate(STREMPTY, strFromCstr(data.source));
		WlToken t = wlParseExpression(&p);
		WlBinder b = wlBinderCreate(NULL, 0);
		WlbNode n = wlBindExpressionOfType(&b, t, data.type);

		test_assert(cstrFormat("%s: Expression is of type %s", data.source, WlBTypeText[data.type]),
					n.type == data.type);
		test_assert(cstrFormat("%s: No diagnostics were reported", data.source), listLen(b.diagnostics) == 0);
	}

	test_that("floating point expression cannot be converted to int")
	{
		WlParser p = wlParserCreate(STREMPTY, STR("10+1.1"));
		WlToken t = wlParseExpression(&p);
		WlBinder b = wlBinderCreate(NULL, 0);
		WlbNode n = wlBindExpressionOfType(&b, t, WlBType_i32);
		test_assert(cstrFormat("A diagnostic was reported"),
					listLen(b.diagnostics) == 1 && b.diagnostics[0].kind == CannotImplicitlyConvertDiagnostic);
	}
}

void test_binder()
{
	test_section("binder");
	test_binder_symbols();
	test_binder_expression_type_resolution();
}
