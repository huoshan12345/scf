#include"scf_optimizer.h"

void __use_function_dag(scf_list_t* h, scf_basic_block_t* bb);

static void __optimize_assign(scf_dag_node_t* assign)
{
	scf_dag_node_t* parent;
	scf_dag_node_t* dst;
	scf_dag_node_t* src;
	int i;

	assert(2 == assign->childs->size);

	dst = assign->childs->data[0];
	src = assign->childs->data[1];

	if (SCF_OP_ADD == src->type
			|| SCF_OP_SUB == src->type
			|| SCF_OP_MUL == src->type
			|| SCF_OP_DIV == src->type
			|| SCF_OP_MOD == src->type
			|| SCF_OP_ADDRESS_OF == src->type) {

		for (i     = src->parents->size - 1; i >= 0; i--) {
			parent = src->parents->data[i];

			if (parent == assign)
				break;
		}

		for (--i; i >= 0; i--) {
			parent = src->parents->data[i];

			if (SCF_OP_ASSIGN == parent->type)
				continue;

			if (SCF_OP_ADD == parent->type
					|| SCF_OP_SUB == parent->type
					|| SCF_OP_MUL == parent->type
					|| SCF_OP_DIV == parent->type
					|| SCF_OP_MOD == parent->type
					|| SCF_OP_ADDRESS_OF == parent->type) {

				if (dst != parent->childs->data[0] && dst != parent->childs->data[1])
					continue;
			}
			break;
		}

		if (i < 0)
			assign->direct = dst;
	}
}

static void __optimize_dn_free(scf_dag_node_t* dn)
{
	scf_dag_node_t* dn2;
	int i;

	for (i = 0; i < dn->childs->size; ) {
		dn2       = dn->childs->data[i];

		assert(0 == scf_vector_del(dn ->childs,  dn2));
		assert(0 == scf_vector_del(dn2->parents, dn));

		if (0 == dn2->parents->size) {
			scf_vector_free(dn2->parents);
			dn2->parents = NULL;
		}
	}

	scf_list_del(&dn->list);
	scf_dag_node_free(dn);
	dn = NULL;
}

static int _bb_dag_update(scf_basic_block_t* bb)
{
	scf_dag_node_t* dn;
	scf_dag_node_t* dn_bb;
	scf_dag_node_t* dn_func;
	scf_dag_node_t* base;
	scf_list_t*     l;

	while (1) {
		int updated = 0;

		for (l = scf_list_tail(&bb->dag_list_head); l != scf_list_sentinel(&bb->dag_list_head); ) {
			dn = scf_list_data(l, scf_dag_node_t, list);
			l  = scf_list_prev(l);

			if (dn->parents)
				continue;

			if (scf_type_is_var(dn->type))
				continue;

			if (scf_type_is_assign_array_index(dn->type))
				continue;
			if (scf_type_is_assign_dereference(dn->type))
				continue;
			if (scf_type_is_assign_pointer(dn->type))
				continue;

			if (scf_type_is_assign(dn->type)
					|| SCF_OP_INC         == dn->type || SCF_OP_DEC       == dn->type
					|| SCF_OP_3AC_INC     == dn->type || SCF_OP_3AC_DEC   == dn->type
					|| SCF_OP_3AC_SETZ    == dn->type || SCF_OP_3AC_SETNZ == dn->type
					|| SCF_OP_3AC_SETLT   == dn->type || SCF_OP_3AC_SETLE == dn->type
					|| SCF_OP_3AC_SETGT   == dn->type || SCF_OP_3AC_SETGE == dn->type
					|| SCF_OP_ADDRESS_OF  == dn->type
					|| SCF_OP_DEREFERENCE == dn->type) {

				if (!dn->childs) {
					scf_list_del(&dn->list);
					scf_dag_node_free(dn);
					dn = NULL;

					++updated;
					continue;
				}

				assert(1 <= dn->childs->size && dn->childs->size <= 3);
				dn_bb     = dn->childs->data[0];

				if (SCF_OP_ADDRESS_OF == dn->type || SCF_OP_DEREFERENCE == dn->type) {

					dn_func = dn->old;
				} else {
					assert(dn_bb->parents && dn_bb->parents->size > 0);

					if (dn != dn_bb->parents->data[dn_bb->parents->size - 1]) {

						if (SCF_OP_ASSIGN ==  dn->type)
							__optimize_assign(dn);
						continue;
					}

					dn_func = dn_bb->old;
				}

				if (!dn_func) {
					scf_loge("\n");
					return -1;
				}

				if (scf_vector_find(bb->dn_saves,   dn_func)
				 || scf_vector_find(bb->dn_resaves, dn_func)) {

					if (SCF_OP_ASSIGN ==  dn->type)
						__optimize_assign(dn);
					continue;
				}

				__optimize_dn_free(dn);
				++updated;

			} else if (SCF_OP_ADD == dn->type || SCF_OP_SUB == dn->type
					|| SCF_OP_MUL == dn->type || SCF_OP_DIV == dn->type
					|| SCF_OP_MOD == dn->type) {
				assert(dn->childs);
				assert(2 == dn->childs->size);

				dn_func = dn->old;

				if (!dn_func) {
					scf_loge("\n");
					return -1;
				}

				if (scf_vector_find(bb->dn_saves,   dn_func)
				 || scf_vector_find(bb->dn_resaves, dn_func))
					continue;

				__optimize_dn_free(dn);
				++updated;
			}
		}
		scf_logd("bb: %p, updated: %d\n\n", bb, updated);

		if (0 == updated)
			break;
	}
	return 0;
}

static int __optimize_basic_block(scf_basic_block_t* bb, scf_function_t* f)
{
	scf_dag_node_t*  dn;
	scf_vector_t*    roots;
	scf_list_t       h;

	int ret;
	int i;

	scf_list_init(&h);

	roots = scf_vector_alloc();
	if (!roots)
		return -ENOMEM;

	ret = scf_basic_block_dag(bb, &bb->dag_list_head);
	if (ret < 0)
		goto error;

	ret = _bb_dag_update(bb);
	if (ret < 0)
		goto error;

	ret = scf_dag_find_roots(&bb->dag_list_head, roots);
	if (ret < 0)
		goto error;

	scf_logd("bb: %p, roots->size: %d\n", bb, roots->size);

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

error:
	__use_function_dag(&bb->code_list_head, bb);

	scf_dag_node_free_list(&bb->dag_list_head);
	scf_vector_free(roots);

	if (ret >= 0)
		ret = scf_basic_block_active_vars(bb);
	return ret;
}

static int _optimize_basic_block(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!f)
		return -EINVAL;

	scf_list_t*        bb_list_head = &f->basic_block_list_head;
	scf_list_t*        l;
	scf_basic_block_t* bb;

	if (scf_list_empty(bb_list_head))
		return 0;

	scf_logd("------- %s() ------\n", f->node.w->text->data);

	for (l = scf_list_head(bb_list_head); l != scf_list_sentinel(bb_list_head); l = scf_list_next(l)) {
		bb = scf_list_data(l, scf_basic_block_t, list);

		if (bb->jmp_flag
				|| bb->end_flag
				|| bb->call_flag
				|| bb->dump_flag
				|| bb->varg_flag) {
			scf_logd("bb: %p, jmp:%d,ret:%d, end: %d, call:%d, varg:%d, dereference_flag: %d\n",
					bb, bb->jmp_flag, bb->ret_flag, bb->end_flag, bb->call_flag, bb->dereference_flag,
					bb->varg_flag);
			continue;
		}

		int ret = __optimize_basic_block(bb, f);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}

//	scf_basic_block_print_list(bb_list_head);
	return 0;
}

scf_optimizer_t  scf_optimizer_basic_block =
{
	.name     =  "basic_block",

	.optimize =  _optimize_basic_block,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
