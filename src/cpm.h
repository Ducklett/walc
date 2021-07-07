#ifdef C_PROJECT_FILE
#include "stin.h"

typedef struct {
	Str name;
	void (*function)();
} CpmCommand;

CpmCommand cpmCommands[0xFF];
int cpmCommandCount;

void addCommand(Str name, void(*function))
{
	assert(cpmCommandCount < 0xFF);
	cpmCommands[cpmCommandCount++] = (CpmCommand){.name = name, .function = function};
}

Str defaultCommand;
void setDefaultCommand(Str command) { defaultCommand = command; }

Str cpmIncludes[0xFF];
int cpmIncludeCount;

void includeDir(Str path)
{
	assert(cpmIncludeCount < 0xFF);
	cpmIncludes[cpmIncludeCount++] = path;
}

void compileAndRun(Str entrypoint)
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
	char *str = stringToCStr(&command);
	printf("%s>>%s%s\n", TERMBOLDBLUE, TERMCLEAR, str);
	system(str);
}

void cpm();
int main(int argc, char **argv)
{
	cpm();

	Str command = STREMPTY;

	if (argc > 2) {
	help:
		printf("Usage: %s <command>\n", argv[0]);
		printf("Available commands:\n");
		for (int i = 0; i < cpmCommandCount; i++) {
			printf("\t%.*s\n", cpmCommands[i].name.len, cpmCommands[i].name.buf);
		}
		return 1;
	}

	if (argc <= 1) {
		command = defaultCommand;
	} else {
		command = (Str){argv[1], strlen(argv[1])};
	}

	for (int i = 0; i < cpmCommandCount; i++) {
		if (strEqual(cpmCommands[i].name, command)) {
			cpmCommands[i].function();
			return 0;
		}
	}

	if (!strEqual(STREMPTY, command) && !strEqual(STR("help"), command)) {
		printf("Unknown command %.*s\n", command.len, command.buf);
	}
	goto help;
}
#endif
