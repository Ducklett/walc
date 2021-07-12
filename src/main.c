#include <binder.c>
#include <wasm.c>

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
	wlParserFree(&p);

	// Wasm source = wasmModuleCreate();

	// wasmModuleAddMemory(&source, STR("mem"), 1, 2);

	// WasmType returns[] = {WasmType_i32};
	// u8 opcodes[] = {WasmOp_I32Const, 10, WasmOp_I32Const, 20, WasmOp_I32Add};
	// wasmModuleAddFunction(&source, STR("foo"), BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));

	// Buf wasm = wasmModuleCompile(source);

	// fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
}
