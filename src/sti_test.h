#ifndef STITEST_H
#define STITEST_H

#include <sti_base.h>

#ifdef __cplusplus
extern "C" {
#endif

// for readability

#define NOT		 !
#define NOTEQUAL !=
#define OR		 ||
#define AND		 &&

static Str testPrefix;
static int testPass;
static int testFail;
static const char *current_test;
static const char *current_test_error;
static bool testSkipSection = false;
static bool testPanic = false;
static bool testDidPanic = false;

#undef PANIC
#define PANIC(msg, ...) (testPanic ? testDidPanic = true : panic_impl(msg, __FILE__, __LINE__, ##__VA_ARGS__))

static void test_internal(const char *msg, bool pass);

static void complete_current_test()
{
	if (testSkipSection) return;

	if (current_test) {
		test_internal(current_test, current_test_error == NULL);
		if (current_test_error) printf("\t%sassert failed: %s %s\n", TERMRED, current_test_error, TERMCLEAR);
	}
	current_test = NULL;
	current_test_error = NULL;
}

void test_section(const char *name)
{
	testSkipSection = !strStartsWith(strFromCstr(name), testPrefix);

	if (!testSkipSection) printf("#      %s\n", name);
}
static void test_internal(const char *msg, bool pass)
{
	if (testSkipSection) return;
	if (pass) {
		testPass++;
		printf("\33[032m[PASS]\33[0m %s\n", msg);
	} else {
		testFail++;
		printf("\33[031m[FAIL]\33[0m %s\n", msg);
	}
}
void test(const char *msg, bool pass)
{
	complete_current_test();
	test_internal(msg, pass);
}

#define test_panic(msg, op) (testPanic = true, op, test(msg, testDidPanic), testDidPanic = false, testPanic = false)

#define test_equal(msg, a, b) test(msg, (a) == (b))

void test_that(const char *msg)
{
	if (testSkipSection) return;
	complete_current_test();
	current_test = msg;
	current_test_error = NULL;
}

void test_assert(const char *msg, bool pass)
{
	if (!pass && !current_test_error) current_test_error = msg;
}

#define test_assert_panic(msg, op) \
	(testPanic = true, op, test_assert(msg, testDidPanic), testDidPanic = false, testPanic = false)

#define TESTPLUR(n) (n == 1 ? "test" : "tests")

void TEST_ENTRYPOINT();

int main(int argc, char **argv)
{
	if (argc > 1) testPrefix = strFromCstr(argv[1]);

	printf("==============\n");
	TEST_ENTRYPOINT();
	testSkipSection = false;
	complete_current_test();
	printf("==============\n");
	int test_ran = testPass + testFail;
	printf("%d %s ran\n", test_ran, TESTPLUR(test_ran));
	if (testPass) {

		if (testPass == test_ran) {
			printf("\33[032mALL TESTS PASSED!\33[0m\n");
		} else {
			printf("\33[032m%d %s passed\33[0m\n", testPass, TESTPLUR(testPass));
		}
	}
	if (testFail) {
		printf("\33[031m%d %s failed\33[0m\n", testFail, TESTPLUR(testFail));
		return 1;
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif // STITEST_H
