#include"scf_optimizer.h"

static int __bb_add_vla_free(scf_basic_block_t* back, scf_function_t* f)
{
	scf_basic_block_t* jcc  = scf_list_data(scf_list_next(&back->list), scf_basic_block_t, list);
	scf_basic_block_t* next = scf_list_data(scf_list_next(&jcc ->list), scf_basic_block_t, list);
	scf_basic_block_t* free;
	scf_basic_block_t* jmp;
	scf_3ac_operand_t* dst;
	scf_3ac_code_t*    c;
	scf_3ac_code_t*    c2;
	scf_list_t*        l;

	assert(jcc->jmp_flag);

	free = scf_basic_block_alloc();
	if (!free)
		return -ENOMEM;
	scf_list_add_front(&jcc->list, &free->list);

	jmp = scf_basic_block_alloc();
	if (!jmp)
		return -ENOMEM;
	scf_list_add_front(&free->list, &jmp->list);

	l  = scf_list_head(&jcc->code_list_head);
	c  = scf_list_data(l, scf_3ac_code_t, list);

	c2 = scf_3ac_code_clone(c);
	if (!c2)
		return -ENOMEM;
	scf_list_add_tail(&jmp->code_list_head, &c2->list);

	c2->op = scf_3ac_find_operator(SCF_OP_GOTO);
	c2->basic_block = jmp;

	int ret = scf_vector_add(f->jmps, c2);
	if (ret < 0)
		return ret;

	switch (c->op->type) {
		case SCF_OP_3AC_JZ:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JNZ);
			break;
		case SCF_OP_3AC_JNZ:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JZ);
			break;

		case SCF_OP_3AC_JGE:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JLT);
			break;
		case SCF_OP_3AC_JLT:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JGE);
			break;

		case SCF_OP_3AC_JGT:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JLE);
			break;
		case SCF_OP_3AC_JLE:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JGT);
			break;

		case SCF_OP_3AC_JA:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JBE);
			break;
		case SCF_OP_3AC_JBE:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JA);
			break;

		case SCF_OP_3AC_JB:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JAE);
			break;
		case SCF_OP_3AC_JAE:
			c->op = scf_3ac_find_operator(SCF_OP_3AC_JB);
			break;
		default:
			break;
	};
	dst = c->dsts->data[0];
	dst->bb = next;

	back->vla_free = free;
	return 0;
}

static int __loop_add_vla_free(scf_bb_group_t* loop, scf_function_t* f)
{
	scf_basic_block_t* bb;
	scf_basic_block_t* back;
	scf_basic_block_t* jcc;
	scf_3ac_operand_t* dst;
	scf_3ac_code_t*    c;
	scf_3ac_code_t*    c2;
	scf_list_t*        l;

	int i;
	int j;
	int k;

	for (i = 0; i < loop->body->size; i++) {
		bb        = loop->body->data[i];

		if (!bb->vla_flag)
			continue;

		for (j = i; j < loop->body->size; j++) {
			back      = loop->body->data[j];

			if (!back->back_flag)
				continue;

			jcc = scf_list_data(scf_list_next(&back->list), scf_basic_block_t, list);

			l   = scf_list_head(&jcc->code_list_head);
			c   = scf_list_data(l, scf_3ac_code_t, list);
			dst = c->dsts->data[0];

			for (k = j; k > i; k--) {
				if (dst->bb == loop->body->data[k])
					break;
			}
			if (k > i)
				continue;

			if (!back->vla_free) {
				int ret = __bb_add_vla_free(back, f);
				if (ret < 0)
					return ret;

				ret = scf_vector_add(loop->body, back->vla_free);
				if (ret < 0)
					return ret;

				for (k = loop->body->size - 2; k > j; k--)
					loop->body->data[k + 1] = loop->body->data[k];
				loop->body->data[k + 1] = back->vla_free;

				back->vla_free->loop_flag = 1;
			}

			for (l = scf_list_head(&bb->code_list_head); l != scf_list_sentinel(&bb->code_list_head); l = scf_list_next(l)) {
				c  = scf_list_data(l, scf_3ac_code_t, list);

				if (SCF_OP_VLA_ALLOC != c->op->type)
					continue;

				c2 = scf_3ac_code_clone(c);
				if (!c2)
					return -ENOMEM;
				scf_list_add_front(&back->vla_free->code_list_head, &c2->list);

				c2->op = scf_3ac_find_operator(SCF_OP_VLA_FREE);
				c2->basic_block = back->vla_free;
			}
		}
	}

	return 0;
}

static int _optimize_vla(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!f)
		return -EINVAL;

	if (!f->vla_flag || f->bb_loops->size <= 0)
		return 0;

	scf_bb_group_t* loop;
	int i;

	for (i = 0; i < f->bb_loops->size; i++) {
		loop      = f->bb_loops->data[i];

		int ret = __loop_add_vla_free(loop, f);
		if (ret < 0)
			return ret;
	}

	return 0;
}

scf_optimizer_t  scf_optimizer_vla =
{
	.name     =  "vla",

	.optimize =  _optimize_vla,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
