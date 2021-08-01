#include <walc.h>

void lowerNode(WlBinder *b, WlbNode *n)
{
	switch (n->kind) {
	case WlBKind_None: break;
	case WlBKind_Unresolved: break;
	case WlBKind_Function: break;
	case WlBKind_DoExpression: {
		n->kind = WlBKind_Block;
		lowerNode(b, n);
	} break;
	case WlBKind_Block: {
		WlBoundBlock blk = *(WlBoundBlock *)n->data;

		WlScope *targetScope = listPeek(&b->scopes);
		if (blk.scope != targetScope && blk.scope) {
			for (int i = 0; i < listLen(blk.scope->symbols); i++) {
				listPush(&targetScope->symbols, blk.scope->symbols[i]);
			}
		}

		int len = listLen(blk.nodes);
		for (int i = 0; i < len; i++) {
			lowerNode(b, &blk.nodes[i]);
		}

		if (len > 0 && blk.nodes[len - 1].kind == WlBKind_Return) {
			WlBoundReturn *st = blk.nodes[len - 1].data;
			blk.nodes[len - 1] = st->expression;
		}

	} break;
	case WlBKind_If: {
		WlBoundIf *st = n->data;
		lowerNode(b, &st->condition);
		lowerNode(b, &st->thenBlock);
		lowerNode(b, &st->elseBlock);
	} break;
	case WlBKind_DoWhileLoop: {
		WlBoundDoWhile *st = n->data;
		lowerNode(b, &st->condition);
		lowerNode(b, &st->block);
	} break;
	case WlBKind_WhileLoop: {
		WlBoundWhile *st = n->data;
		lowerNode(b, &st->condition);
		lowerNode(b, &st->block);
	} break;
	case WlBKind_ForLoop: {
		// <precondition>
		// while(<condition>) {
		// 	<block>
		// 	<postcondition>
		// }

		WlBoundFor *st = n->data;

		WlBoundBlock *blk = arenaMalloc(sizeof(WlBoundBlock), &b->arena);
		blk->nodes = listNew();
		blk->scope = st->scope;

		listPush(&blk->nodes, st->preCondition);

		WlBoundWhile *whl = arenaMalloc(sizeof(WlBoundWhile), &b->arena);
		whl->condition = st->condition;
		whl->block = st->block;

		WlBoundBlock *innerBlk = st->block.data;
		listPush(&innerBlk->nodes, st->postCondition);

		WlbNode whileNode = {.kind = WlBKind_WhileLoop, .data = whl, .type = WlBType_u0};

		listPush(&blk->nodes, whileNode);

		n->kind = WlBKind_Block;
		n->data = blk;
		lowerNode(b, n);
	} break;
	case WlBKind_VariableDeclaration: {
		WlBoundVariable *st = n->data;

		// constants are inlined, so the declaration can disappear
		if (st->symbol->flags & WlSFlag_Constant) {
			n->kind = WlBKind_None;
			return;
		}

		if (st->initializer.kind != WlBKind_None) {
			n->kind = WlBKind_VariableAssignment;
			WlBoundAssignment *assigment = arenaMalloc(sizeof(WlBoundAssignment), &b->arena);
			assigment->symbol = st->symbol;
			assigment->expression = st->initializer;
			n->data = assigment;
			lowerNode(b, n);
		} else {
			n->kind = WlBKind_None;
		}
	} break;
	case WlBKind_VariableAssignment: {
		WlBoundAssignment *st = n->data;
		// oops! abstract type slipped through...
		// TODO: maybe do this elsewhere
		if (st->symbol->type != st->expression.type) {
			propagateType(&st->expression, st->symbol->type);
		}
		lowerNode(b, &st->expression);
	} break;
	case WlBKind_Call: {
		WlBoundCallExpression *st = n->data;
		for (int i = 0; i < listLen(st->args); i++) {
			lowerNode(b, &st->args[i]);
		}
	} break;
	case WlBKind_Ref: {
		WlSymbol *s = n->data;
		if (s->flags & WlSFlag_Constant) {
			if (s->initializer->type != n->type) {
				propagateType(s->initializer, n->type);
			}
			*n = *s->initializer;
		}
	} break;
	case WlBKind_Return: {
		WlBoundReturn *st = n->data;
		lowerNode(b, &st->expression);
	} break;
	case WlBKind_BinaryExpression: {
		WlBoundBinaryExpression *st = n->data;
		lowerNode(b, &st->left);
		lowerNode(b, &st->right);
	} break;
	case WlBKind_TernaryExpression: {
		WlBoundTernaryExpression *st = n->data;
		WlBoundIf *df = arenaMalloc(sizeof(WlBoundIf), &b->arena);
		df->condition = st->condition;
		df->thenBlock = st->thenExpr;
		df->elseBlock = st->elseExpr;

		n->kind = WlBKind_If;
		n->data = df;

		lowerNode(b, n);
	} break;
	case WlBKind_PreUnaryExpression: {
		WlBoundPreUnaryExpression *st = n->data;
		if (st->operator== WlBOperator_Increment || st->operator== WlBOperator_Decrement) {
			WlBoundBlock *blk = arenaMalloc(sizeof(WlBoundBlock), &b->arena);

			// x = x - 1 | x = x + 1
			// ref x

			WlBoundBinaryExpression *bin = arenaMalloc(sizeof(WlBoundBinaryExpression), &b->arena);
			bin->left = st->expression;
			bin->operator= st->operator== WlBOperator_Increment ? WlBOperator_Add : WlBOperator_Subtract;
			bin->right = (WlbNode){.kind = WlBKind_NumberLiteral, .type = WlBType_integerNumber, .dataNum = 1};
			if (bin->right.type != bin->left.type) {
				propagateType(&bin->right, bin->left.type);
			}

			WlBoundAssignment *asg = arenaMalloc(sizeof(WlBoundAssignment), &b->arena);
			asg->symbol = st->expression.data;
			asg->expression = (WlbNode){.kind = WlBKind_BinaryExpression, .type = bin->left.type, .data = bin};

			WlbNode asgnode = (WlbNode){.kind = WlBKind_VariableAssignment, .type = bin->left.type, .data = asg};
			WlbNode ref = st->expression;

			blk->scope = NULL;
			blk->nodes = listNew();
			listPush(&blk->nodes, asgnode);
			listPush(&blk->nodes, ref);
			n->kind = WlBKind_DoExpression;
			n->data = blk;
			n->type = bin->left.type;
			lowerNode(b, n);
		}
	} break;
	case WlBKind_PostUnaryExpression: {
		WlBoundPostUnaryExpression *st = n->data;
		WlBoundBlock *blk = arenaMalloc(sizeof(WlBoundBlock), &b->arena);

		// ref x
		// x = x - 1 | x = x + 1

		WlBoundBinaryExpression *bin = arenaMalloc(sizeof(WlBoundBinaryExpression), &b->arena);
		bin->left = st->expression;
		bin->operator= st->operator== WlBOperator_Increment ? WlBOperator_Add : WlBOperator_Subtract;
		bin->right = (WlbNode){.kind = WlBKind_NumberLiteral, .type = WlBType_integerNumber, .dataNum = 1};
		if (bin->right.type != bin->left.type) {
			propagateType(&bin->right, bin->left.type);
		}

		WlBoundAssignment *asg = arenaMalloc(sizeof(WlBoundAssignment), &b->arena);
		asg->symbol = st->expression.data;
		asg->expression = (WlbNode){.kind = WlBKind_BinaryExpression, .type = bin->left.type, .data = bin};

		WlbNode asgnode = (WlbNode){.kind = WlBKind_VariableAssignment, .type = bin->left.type, .data = asg};
		WlbNode ref = st->expression;

		blk->scope = NULL;
		blk->nodes = listNew();
		listPush(&blk->nodes, ref);
		listPush(&blk->nodes, asgnode);
		n->kind = WlBKind_DoExpression;
		n->data = blk;
		n->type = bin->left.type;
		lowerNode(b, n);
	} break;
	case WlBKind_StringLiteral: break;
	case WlBKind_NumberLiteral: break;
	case WlBKind_BoolLiteral: break;
	default: PANIC("Unhandled kind %d in lowerer", n->kind);
	}
}

void lower(WlBinder *b)
{
	for (int i = 0; i < listLen(b->functions); i++) {
		WlBoundFunction fn = *b->functions[i];
		listPush(&b->scopes, fn.scope);
		lowerNode(b, &fn.body);
		listPop(&b->scopes);
	}
}
