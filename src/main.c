#include <wasmEmitter.c>

int main(int argc, char **argv)
{
	Str source;

	fileReadAllText("examples/03_functions.wl", &source) || PANIC("Failed to open file");

	printf("source %.*s\n", STRPRINT(source));

	WlParser p = wlParserCreate(source);
	wlParse(&p);

	WlBinder b = wlBind(p.topLevelDeclarations, p.topLevelCount);

	for (int i = 0; i < p.topLevelCount; i++) {
		wlPrint(p.topLevelDeclarations[i]);
	}

	int functionCount = listLen(b.functions);
	for (int i = 0; i < functionCount; i++) {
		WlBoundFunction *fn = b.functions[i];
		printf("%s%s %.*s(){}\n", (fn->symbol->flags & WlSFlag_Export) ? "export " : "", WlBTypeText[fn->symbol->type],
			   STRPRINT(fn->symbol->name));
	}

	Buf wasm = emitWasm(&b);
	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");

	wlParserFree(&p);
}
