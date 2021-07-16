#define C_PROJECT_FILE

#include "src/sti_project.h"

void run();
void build();
void test();

void describe_project()
{
	includeDir("./");
	includeDir("src");

	setDefaultCommand("run");
	addCommand("run", run);
	addCommand("build", build);
	addCommand("test", test);
}

void build()
{
	//
	compileAndOutput("./src/main.c", "walc");
}

void run()
{
	//
	compileAndRun("./src/main.c");
}

void test()
{
	includeDir("test");

	compileAndRun("./test/all.c");
}
