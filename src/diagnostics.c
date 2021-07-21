#include <walc.h>

WlSpan spanFromRange(Str filename, Str source, size_t start, size_t end)
{
	return (WlSpan){.filename = filename, .source = source, .start = start, .len = end - start};
}

WlSpan spanFromTokens(Str filename, Str source, WlToken start, WlToken end)
{
	return (WlSpan){.filename = filename,
					.source = source,
					.start = start.span.start,
					.len = end.span.start + end.span.len};
}

void diagnosticPrint(WlDiagnostic d)
{
	Str errSlice = strSlice(d.span.source, d.span.start, d.span.len);

	int lineStart = d.span.start;
	int lineEnd = d.span.start + d.span.len;
	while (lineStart > 0 && !isNewline(d.span.source.buf[--lineStart])) {
	}
	while (lineEnd < d.span.source.len && !isNewline(d.span.source.buf[++lineEnd])) {
	}

	Str lineSlice = strSlice(d.span.source, lineStart, lineEnd);

	printf("%s%.*s%s: %sfatal error:%s ", TERMBOLD, STRPRINT(d.span.filename), TERMCLEAR, TERMRED, TERMCLEAR);

	switch (d.kind) {
	case UnterminatedCommentDiagnostic: {
		printf("Unexpected End of file, expected '*/'\n", STRPRINT(errSlice));
	} break;
	case UnterminatedStringDiagnostic: {
		printf("Unexpected End of file, expected '\"'\n", STRPRINT(errSlice));
	} break;
	case UnexpectedTokenDiagnostic: {
		if (d.kind2) {
			printf("Unexpected token %s%.*s%s, expected %s\n", TERMBOLDCYAN, STRPRINT(errSlice), TERMCLEAR,
				   WlKindText[d.kind2]);
		} else {
			printf("Unexpected token %s%.*s%s\n", TERMBOLDCYAN, STRPRINT(errSlice), TERMCLEAR, WlKindText[d.kind1]);
		}
	} break;
	case UnexpectedTokenInPrimaryExpressionDiagnostic: {
		printf("Unexpected token %s%.*s%s while parsing primary expression\n", TERMBOLDCYAN, STRPRINT(errSlice),
			   TERMCLEAR);
	} break;
	default: PANIC("unhandled diagnostic kind %d\n", d.kind);
	}

	printf("%.*s\n", STRPRINT(lineSlice));
	printf("%*s%s", d.span.start - lineStart, "", TERMRED);
	for (int i = 0; i < d.span.len; i++) {
		fputchar(i == 0 ? '^' : '~');
	}
	printf("%s\n", TERMCLEAR);
}
