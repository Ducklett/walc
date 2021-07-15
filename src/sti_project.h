#ifdef C_PROJECT_FILE
#include "sti_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	Str name;
	void (*function)();
} CpmCommand;

CpmCommand cpmCommands[0xFF];
int cpmCommandCount;

int remainingArgc;
char **remainingArgv;

void addCommandS(Str name, void(*function))
{
	assert(cpmCommandCount < 0xFF);
	cpmCommands[cpmCommandCount++] = (CpmCommand){.name = name, .function = function};
}

#define addCommand(name, fn) addCommandS(STR(name), fn)

Str defaultCommand;
void setDefaultCommandS(Str command) { defaultCommand = command; }
#define setDefaultCommand(command) setDefaultCommandS(STR(command))

Str cpmIncludes[0xFF];
int cpmIncludeCount;

void includeDirS(Str path)
{
	assert(cpmIncludeCount < 0xFF);
	cpmIncludes[cpmIncludeCount++] = path;
}

#define includeDir(path) includeDirS(STR(path))

void compileAndRunS(Str entrypoint)
{
	String command = stringCreate();
	stringAppend(&command, STR("tcc "));
	for (int i = 0; i < cpmIncludeCount; i++) {
		stringAppend(&command, STR("-I"));
		stringAppend(&command, cpmIncludes[i]);
		stringAppend(&command, STR(" "));
	}

	stringAppend(&command, STR("-run "));
	stringAppend(&command, entrypoint);

	if (remainingArgc) {
		stringAppend(&command, STR(" "));
		for (int i = 0; i < remainingArgc; i++) {
			stringPush(&command, '"');
			stringAppend(&command, strFromCstr(remainingArgv[i]));
			stringPush(&command, '"');
			stringAppend(&command, STR(" "));
		}
	}

	char *str = stringToCStr(&command);
	printf("%s>>%s%s\n", TERMBOLDBLUE, TERMCLEAR, str);
	system(str);
}
#define compileAndRun(entrypoint) compileAndRunS(STR(entrypoint))

void describe_project();
int main(int argc, char **argv)
{
	describe_project();

	Str command = STREMPTY;

	if (argc <= 1) {
		command = defaultCommand;
	} else {
		command = (Str){argv[1], strlen(argv[1])};
	}

	for (int i = 0; i < cpmCommandCount; i++) {
		if (strEqual(cpmCommands[i].name, command)) {
			remainingArgc = argc - 2;
			remainingArgv = argv + 2;
			cpmCommands[i].function();
			return 0;
		}
	}

	if (!strEqual(STREMPTY, command) && !strEqual(STR("help"), command)) {
		printf("Unknown command %.*s\n", command.len, command.buf);
	}

	printf("Usage: %s <command>\n", argv[0]);
	printf("Available commands:\n");
	for (int i = 0; i < cpmCommandCount; i++) {
		printf("\t%.*s\n", cpmCommands[i].name.len, cpmCommands[i].name.buf);
	}
	return 1;
}

#ifdef __cplusplus
}
#endif

#endif
