#include <wasmEmitter.c>

int main(int argc, char **argv)
{
	// Str source = STR("export u0 add(i32 a, i32 b) { return a*a + b+1; }");
	Str source = STR("export u0 add() {} i32 another(){}");
	WlParser p = wlParserCreate(source);
	wlParse(&p);

	WlBinder b = wlBind(p.topLevelDeclarations, p.topLevelCount);

	for (int i = 0; i < p.topLevelCount; i++) {
		wlPrint(p.topLevelDeclarations[i]);
	}

	for (int i = 0; i < b.functionCount; i++) {
		WlBoundFunction fn = b.functions[i];
		printf("%s%s %.*s(){}\n", fn.exported ? "export " : "", WlBTypeText[fn.returnType], STRPRINT(fn.name));
	}

	Buf wasm = emitWasm(&b);
	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");

	wlParserFree(&p);
}
