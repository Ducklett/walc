#include "src/stin.h"

int main(int argc, char **argv)
{
	char *command = NULL;

	if (argc > 2) {
	help:
		printf("Usage: %s <command>\n", argv[0]);
		printf("Available commands:\n");
		printf("\ttest\n");
		printf("\trun\n");
		return;
	}

	if (argc <= 1) {
		command = "run";
	} else {
		command = argv[1];
	}

	if (!strcmp(command, "run")) {
		system("tcc -run -I./ -Isrc .\\src\\main.c");
	} else if (!strcmp(command, "test")) {
		system("tcc -run -I./ -Itest -Isrc ./test/all.c");
	} else {
		printf("Unknown command %s\n", command);
		goto help;
	}

	return 0;
}
