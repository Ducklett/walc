#include <walc.h>

int main(int argc, char **argv)
{
	Str filename = STR("examples/07_notes.wl");
	Str source;

	fileReadAllText(filename.buf, &source) || PANIC("Failed to open file");

	WlParser p = wlParserCreate(filename, source);
	wlParse(&p);

	WlBinder b = wlBind(p.topLevelDeclarations);

	// for (int i = 0; i < topLevelCount; i++) {
	// 	wlPrint(p.topLevelDeclarations[i]);
	// }

	// int functionCount = listLen(b.functions);
	// for (int i = 0; i < functionCount; i++) {
	// 	WlBoundFunction *fn = b.functions[i];
	// 	printf("%s%s %.*s(){}\n", (fn->symbol->flags & WlSFlag_Export) ? "export " : "", WlBTypeText[fn->symbol->type],
	// 		   STRPRINT(fn->symbol->name));
	// }

	bool hasDiagnostics = listLen(p.diagnostics) || listLen(p.lexer.diagnostics) || listLen(b.diagnostics);

	if (hasDiagnostics) {
		for (int i = 0; i < listLen(p.lexer.diagnostics); i++)
			diagnosticPrint(p.lexer.diagnostics[i]);
		for (int i = 0; i < listLen(p.diagnostics); i++)
			diagnosticPrint(p.diagnostics[i]);
		for (int i = 0; i < listLen(b.diagnostics); i++)
			diagnosticPrint(b.diagnostics[i]);
	} else {
		lower(&b);
		Buf wasm = emitWasm(&b);
		fileWriteAllBytes("out.wasm", wasm) || PANIC("Failed to write wasm");
	}

	wlParserFree(&p);
}
