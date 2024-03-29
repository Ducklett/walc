#ifndef TEST_ENTRYPOINT
#define TEST_ENTRYPOINT test_wasm
#endif

#include <sti_test.h>
#include <wasm.c>

void test_wasm_empty_module()
{
	Wasm module = wasmModuleCreate();

	Buf wasm = wasmModuleCompile(module);
	u8 expectedBytes[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
	test("Empty module produces correct bytecode", bufEqual(BUF(expectedBytes), wasm));

	wasmModuleFree(&module);
}

void test_wasm_memory()
{
	test_that("A simple module with memory compiles")
	{
		Wasm module = wasmModuleCreate();
		test_assert("Empty module doesn't have memory", module.hasMemory == false);

		Str name = STR("foo");
		int pages = 1;
		int maxPages = 2;
		wasmModuleAddMemory(&module, name, pages, maxPages);
		test_assert("Module has memory after is is added", module.hasMemory == true);

		WasmMemory mem = module.memory;
		test_assert("Memory has the correct values",
					mem.pages == pages && mem.maxPages == maxPages && strEqual(mem.name, name));

		Buf wasm = wasmModuleCompile(module);
		u8 expectedBytes[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x05, 0x04, 0x01, 0x01,
							  0x01, 0x02, 0x07, 0x07, 0x01, 0x03, 0x66, 0x6F, 0x6F, 0x02, 0x00};

		test_assert("Module with memory produces the correct bytecode", bufEqual(BUF(expectedBytes), wasm));

		wasmModuleFree(&module);
	}

	test_that("Memory can only be added once")
	{
		Wasm module = wasmModuleCreate();

		wasmModuleAddMemory(&module, STR("foo"), 1, 2);
		test_assert("Memory can be added to the module", module.hasMemory == true);

		test_assert_panic("Application will panic when adding memory a second time",
						  wasmModuleAddMemory(&module, STR("bar"), 3, 4));

		wasmModuleFree(&module);
	}
}
void test_wasm_func()
{
	{
		Wasm module = wasmModuleCreate();
		u8 returns[] = {WasmType_I32};
		u8 opcodes[] = {WasmOp_I32Const, 42};
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes), -1);
		test("Single function produces a single function type", listLen(module.types) == 1);

		Buf bytecode = wasmModuleCompile(module);
		u8 expectedBytecode[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x00, 0x01,
								 0x7F, 0x03, 0x02, 0x01, 0x00, 0x0A, 0x06, 0x01, 0x04, 0x00, 0x41, 0x2A, 0x0B};

		test("Single function produces the correct bytecode", bufEqual(bytecode, BUF(expectedBytecode)));

		wasmModuleFree(&module);
	}

	{
		Wasm module = wasmModuleCreate();
		u8 returns[] = {WasmType_I32};
		u8 opcodes[] = {WasmOp_I32Const, 42};
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes), -1);
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes), -1);
		test("Two functions of the same type share a function type", listLen(module.types) == 1);

		wasmModuleFree(&module);
	}

	{
		Wasm module = wasmModuleCreate();
		u8 returns[] = {WasmType_I32};
		u8 returns2[] = {WasmType_I64};
		u8 opcodes[] = {WasmOp_I32Const, 42};
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns), BUFEMPTY, BUF(opcodes), -1);
		wasmModuleAddFunction(&module, STREMPTY, BUFEMPTY, BUF(returns2), BUFEMPTY, BUF(opcodes), -1);
		test("Two functions of different types get their own function type", listLen(module.types) == 2);

		wasmModuleFree(&module);
	}
}

void test_wasm_import()
{
	{
		Wasm module = wasmModuleCreate();

		test("Function count is 0 for an empty module", module.funcCount == 0);
		test("Import count is 0 for an empty module", listLen(module.imports) == 0);

		u8 argsValues[] = {WasmType_I32, WasmType_I32};
		Buf args = BUF(argsValues);
		Buf rets = BUFEMPTY;
		wasmModuleAddImport(&module, STR("print"), args, rets, -1);

		test("Function count increments when adding an import", module.funcCount == 1);
		test("Import count increments when adding an import", listLen(module.imports) == 1);

		Buf bytecode = wasmModuleCompile(module);
		u8 expectedBytecode[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01,
								 0x60, 0x02, 0x7F, 0x7F, 0x00, 0x02, 0x0D, 0x01, 0x03, 0x65, 0x6E,
								 0x76, 0x05, 0x70, 0x72, 0x69, 0x6E, 0x74, 0x00, 0x00};

		test("Single import produces the correct bytecode", bufEqual(bytecode, BUF(expectedBytecode)));

		wasmModuleFree(&module);
	}
}

void test_wasm_data()
{
	{
		Wasm module = wasmModuleCreate();
		wasmModuleAddMemory(&module, STREMPTY, 1, 1);

		test("Data count is empty for new module", listLen(module.data) == 0);
		test("Data offset is empty for new module", module.dataOffset == 0);

		Str msg1 = STR("hello");
		Str msg2 = STR("world");
		int msg1Offset = wasmModuleAddData(&module, STRTOBUF(msg1));

		test("the first data is added at offset 0", msg1Offset == 0);

		int msg2Offset = wasmModuleAddData(&module, STRTOBUF(msg2));
		test("the second data is appended after the first", msg2Offset >= msg1.len);

		test("data count matches the amount of data sections pushed", listLen(module.data) == 2);

		Buf bytecode = wasmModuleCompile(module);
		u8 expected[] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x05, 0x04, 0x01, 0x01, 0x01,
						 0x01, 0x0B, 0x15, 0x02, 0x00, 0x41, 0x00, 0x0B, 0x05, 0x68, 0x65, 0x6C, 0x6C,
						 0x6F, 0x00, 0x41, 0x05, 0x0B, 0x05, 0x77, 0x6F, 0x72, 0x6C, 0x64};
		test("data produces the correct bytecode", bufEqual(bytecode, BUF(expected)));

		wasmModuleFree(&module);
	}
}

void test_wasm()
{
	test_section("Wasm");
	test_wasm_empty_module();
	test_wasm_memory();
	test_wasm_import();
	test_wasm_func();
	test_wasm_data();
}
