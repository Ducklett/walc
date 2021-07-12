#include <wasm.c>

int main(int argc, char **argv)
{
	Wasm source = wasmModuleCreate();

	wasmModuleAddMemory(&source, STR("mem"), 1, 2);

	WasmType returns[] = {WasmType_i32};
	u8 opcodes[] = {WasmOp_I32Const, 10, WasmOp_I32Const, 20, WasmOp_I32Add};
	wasmModuleAddFunction(&source, STR("foo"), BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));

	Buf wasm = wasmModuleCompile(source);

	fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
}
