#define C_PROJECT_FILE

#include "src/cpm.h"

void run();
void test();

void cpm()
{
	includeDir(STR("./"));
	includeDir(STR("src"));

	setDefaultCommand(STR("run"));
	addCommand(STR("run"), run);
	addCommand(STR("test"), test);
}

void run()
{
	//
	compileAndRun(STR("./src/main.c"));
}

void test()
{
	includeDir(STR("test"));

	compileAndRun(STR("./test/all.c"));
}
