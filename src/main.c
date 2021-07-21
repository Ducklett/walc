#include <walc.h>

int main(int argc, char **argv)
{
	Str filename = STR("examples/04_variables.wl");
	Str source;

	fileReadAllText(filename.buf, &source) || PANIC("Failed to open file");

	WlParser p = wlParserCreate(filename, source);
	wlParse(&p);

	int topLevelCount = listLen(p.topLevelDeclarations);
	WlBinder b = wlBind(p.topLevelDeclarations, topLevelCount);

	for (int i = 0; i < topLevelCount; i++) {
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
