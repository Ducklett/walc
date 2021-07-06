#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_wasm
#endif

#include "../src/wasm.c"
#include "test.h"

void test_wasm_empty_module()
{
	Wasm module = wasmModuleCreate();

	Buf wasm = wasmModuleCompile(module);
	u8 expectedBytes[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
	test("Empty module produces correct bytecode", bufEqual(BUF(expectedBytes), wasm));
}

void test_wasm_memory()
{
	{
		Wasm module = wasmModuleCreate();
		test("Empty module doesn't have memory", module.hasMemory == false);

		Str name = STR("foo");
		int pages = 1;
		int maxPages = 2;
		wasmModuleAddMemory(&module, name, pages, maxPages);
		test("Module has memory after is is added", module.hasMemory == true);

		WasmMemory mem = module.memory;
		test("Memory has the correct values",
			 mem.pages == pages && mem.maxPages == maxPages && strEqual(mem.name, name));

		Buf wasm = wasmModuleCompile(module);
		u8 expectedBytes[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x05, 0x04, 0x01, 0x01,
							  0x01, 0x02, 0x07, 0x07, 0x01, 0x03, 0x66, 0x6F, 0x6F, 0x02, 0x00};

		test("Module with memory produces the correct bytecode", bufEqual(BUF(expectedBytes), wasm));
	}

	{
		Wasm module = wasmModuleCreate();

		wasmModuleAddMemory(&module, STR("foo"), 1, 2);
		wasmModuleAddMemory(&module, STR("bar"), 3, 4);
		test("Application will panic when adding memory a second time", didPanic);
	}
}
void test_wasm_func()
{
	{
		Wasm module = wasmModuleCreate();
		u8 returns[] = {WasmType_i32};
		u8 opcodes[] = {WasmOp_I32Const, 42};
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));
		test("Single function produces a single function type", module.typeCount == 1);

		Buf bytecode = wasmModuleCompile(module);
		u8 expectedBytecode[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x00, 0x01,
								 0x7F, 0x03, 0x02, 0x01, 0x00, 0x0A, 0x06, 0x01, 0x04, 0x00, 0x41, 0x2A, 0x0B};

		test("Single function produces the correct bytecode", bufEqual(bytecode, BUF(expectedBytecode)));
	}

	{
		Wasm module = wasmModuleCreate();
		u8 returns[] = {WasmType_i32};
		u8 opcodes[] = {WasmOp_I32Const, 42};
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));
		test("Two functions of the same type share a function type", module.typeCount == 1);
	}

	{
		Wasm module = wasmModuleCreate();
		u8 returns[] = {WasmType_i32};
		u8 returns2[] = {WasmType_I64};
		u8 opcodes[] = {WasmOp_I32Const, 42};
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes));
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns2), BUFEMPTY, BUF(opcodes));
		test("Two functions of different types get their own function type", module.typeCount == 2);
	}
}

void test_wasm()
{
	test_section("Wasm");
	test_wasm_empty_module();
	test_wasm_memory();
	test_wasm_func();
}
