#include <lexer.c>
#include <wasm.c>

int main(int argc, char **argv)
{
	Str source = STR("export u0 add(i32 a, i32 b) { return a*a + b+1; }");
	WlLexer l = wlLexerCreate(source);
	wlLexerLexTokens(&l);

	for (int i = 0; i < l.tokenCount; i++) {
		printf("%s\n", WlKindText[l.tokens[i].kind]);
	}
	wlLexerFree(&l);

	// Wasm source = wasmModuleCreate();

	// wasmModuleAddMemory(&source, STR("mem"), 1, 2);

	// WasmType returns[] = {WasmType_i32};
	// u8 opcodes[] = {WasmOp_I32Const, 10, WasmOp_I32Const, 20, WasmOp_I32Add};
	// wasmModuleAddFunction(&source, STR("foo"), BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));

	// Buf wasm = wasmModuleCompile(source);

	// fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
}
