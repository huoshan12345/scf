#include"scf_optimizer.h"
#include"scf_pointer_alias.h"

#define SCF_BB_SPLIT(bb0, bb1) \
	do { \
		bb1 = NULL; \
		int ret = scf_basic_block_split(bb0, &bb1); \
		if (ret < 0) \
		    return ret; \
		bb1->dereference_flag = bb0->dereference_flag; \
		bb1->ret_flag         = bb0->ret_flag; \
		bb0->ret_flag         = 0; \
		scf_list_add_front(&bb0->list, &bb1->list); \
	} while (0)

static int _optimize_split_call_bb(scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_basic_block_t* cur_bb = bb;
	scf_basic_block_t* bb2;
	scf_3ac_code_t*    c;
	scf_list_t*        l;

	int split_flag = 0;

	for (l = scf_list_head(&bb->code_list_head); l != scf_list_sentinel(&bb->code_list_head); ) {

		c  = scf_list_data(l, scf_3ac_code_t, list);
		l  = scf_list_next(l);

		if (split_flag) {
			split_flag = 0;

			SCF_BB_SPLIT(cur_bb, bb2);
			cur_bb = bb2;
		}

		if (cur_bb != bb) {
			scf_list_del(&c->list);
			scf_list_add_tail(&cur_bb->code_list_head, &c->list);

			c->basic_block = cur_bb;
		}

		if (SCF_OP_CALL == c->op->type) {
			split_flag = 1;

			if (scf_list_prev(&c->list) != scf_list_sentinel(&cur_bb->code_list_head)) {
				SCF_BB_SPLIT(cur_bb, bb2);

				cur_bb->call_flag = 0;

				scf_list_del(&c->list);
				scf_list_add_tail(&bb2->code_list_head, &c->list);

				c->basic_block = bb2;

				cur_bb = bb2;
			}

			cur_bb->call_flag = 1;
		}
	}

	return 0;
}

static int _optimize_split_call(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!f)
		return -EINVAL;

	scf_list_t*        bb_list_head = &f->basic_block_list_head;
	scf_list_t*        l;
	scf_basic_block_t* bb;

	if (scf_list_empty(bb_list_head))
		return 0;

	for (l = scf_list_head(bb_list_head); l != scf_list_sentinel(bb_list_head); ) {

		bb = scf_list_data(l, scf_basic_block_t, list);
		l  = scf_list_next(l);

		if (bb->jmp_flag || bb->end_flag || bb->cmp_flag)
			continue;

		if (!bb->call_flag)
			continue;

		int ret = _optimize_split_call_bb(bb, bb_list_head);
		if (ret < 0)
			return ret;
	}

	return 0;
}

scf_optimizer_t  scf_optimizer_split_call =
{
	.name     =  "split_call",

	.optimize =  _optimize_split_call,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
