#include <wasmEmitter.c>

int main(int argc, char **argv)
{
	Str source = STR("export i32 main() { return 10+20*3/2; }");

	fileReadAllText("examples/02_expressions.wl", &source) || PANIC("Failed to open file");

	printf("source %.*s\n", STRPRINT(source));

	WlParser p = wlParserCreate(source);
	wlParse(&p);

	WlBinder b = wlBind(p.topLevelDeclarations, p.topLevelCount);

	for (int i = 0; i < p.topLevelCount; i++) {
		wlPrint(p.topLevelDeclarations[i]);
	}

	int functionCount = listLen(b.functions);
	for (int i = 0; i < functionCount; i++) {
		WlBoundFunction fn = b.functions[i];
		printf("%s%s %.*s(){}\n", fn.exported ? "export " : "", WlBTypeText[fn.returnType], STRPRINT(fn.name));
	}

	Buf wasm = emitWasm(&b);
	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");

	wlParserFree(&p);
}
