#include"scf_optimizer.h"

void __use_function_dag(scf_list_t* h, scf_basic_block_t* bb)
{
	scf_3ac_operand_t*  src;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_dag_node_t*     dn;
	scf_list_t*         l;

	int i;

	for (l = scf_list_head(h); l != scf_list_sentinel(h); l = scf_list_next(l)) {
		c  = scf_list_data(l, scf_3ac_code_t, list);

		if (c->dsts) {
			dst = c->dsts->data[0];
			dn  = dst->dag_node->old;

			dst->node     = dn->node;
			dst->dag_node = dn;
		}

		if (c->srcs) {
			for (i  = 0; i < c->srcs->size; i++) {
				src =        c->srcs->data[i];

				dn  = src->dag_node->old;

				src->node     = dn->node;
				src->dag_node = dn;
			}
		}

		c->basic_block = bb;
	}
}

int __optimize_common_expr(scf_basic_block_t* bb, scf_function_t* f)
{
	scf_dag_node_t*    dn;
	scf_vector_t*      roots;
	scf_list_t*        l;
	scf_list_t         h;

	int ret;
	int i;

	scf_list_init(&h);

	roots = scf_vector_alloc();
	if (!roots)
		return -ENOMEM;

	ret = scf_basic_block_dag(bb, &bb->dag_list_head);
	if (ret < 0)
		goto error;

	ret = scf_dag_find_roots(&bb->dag_list_head, roots);
	if (ret < 0)
		goto error;

	for (i = 0; i < roots->size; i++) {
		dn =        roots->data[i];

		ret = scf_dag_expr_calculate(&h, dn);
		if (ret < 0) {
			scf_loge("\n");
			scf_list_clear(&h, scf_3ac_code_t, list, scf_3ac_code_free);
			goto error;
		}
	}

	scf_list_clear(&bb->code_list_head, scf_3ac_code_t, list, scf_3ac_code_free);

	scf_list_mov2(&bb->code_list_head, &h);

	ret = 0;
error:
	__use_function_dag(&bb->code_list_head, bb);

	scf_dag_node_free_list(&bb->dag_list_head);
	scf_vector_free(roots);
	return ret;
}

static int _optimize_common_expr(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!f)
		return -EINVAL;

	scf_basic_block_t* bb;
	scf_list_t*        l;
	scf_list_t*        bb_list_head = &f->basic_block_list_head;

	if (scf_list_empty(bb_list_head))
		return 0;

	scf_logd("------- %s() ------\n", f->node.w->text->data);

	for (l = scf_list_head(bb_list_head); l != scf_list_sentinel(bb_list_head); l = scf_list_next(l)) {

		bb  = scf_list_data(l, scf_basic_block_t, list);

		if (bb->jmp_flag
				|| bb->end_flag
				|| bb->call_flag
				|| bb->varg_flag) {
			scf_logd("bb: %p, jmp:%d,ret:%d, end: %d, call:%d, varg:%d, dereference_flag: %d\n",
					bb, bb->jmp_flag, bb->ret_flag, bb->end_flag, bb->call_flag, bb->dereference_flag,
					bb->varg_flag);
			continue;
		}

		int ret = __optimize_common_expr(bb, f);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

		ret = scf_basic_block_vars(bb, bb_list_head);
		if (ret < 0)
			return ret;
	}

//	scf_basic_block_print_list(bb_list_head);
	return 0;
}

scf_optimizer_t  scf_optimizer_common_expr =
{
	.name     =  "common_expr",

	.optimize =  _optimize_common_expr,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
