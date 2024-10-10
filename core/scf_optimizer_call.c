#include"scf_optimizer.h"
#include"scf_pointer_alias.h"

static int _alias_call(scf_vector_t* aliases, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_3ac_operand_t* src;
	scf_dn_status_t*   ds;
	scf_dag_node_t*    dn;
	scf_variable_t*    v;
	scf_dag_node_t*    dn_pointer;
	scf_vector_t*      dn_pointers;

	int i;
	int j;
	int k = aliases->size;

	assert(c->srcs && c->srcs->size >= 1);

	for (i = 1; i < c->srcs->size; i++) {

		src = c->srcs->data[i];
		dn  = src->dag_node;
		v   = dn->var;

		if (0 == v->nb_pointers)
			continue;

		if (scf_variable_const_string(v))
			continue;

		if (SCF_OP_VA_ARG == dn->type)
			continue;

		scf_logd("pointer: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

		dn_pointers = scf_vector_alloc();
		if (!dn_pointers)
			return -ENOMEM;

		int ret = scf_vector_add_unique(dn_pointers, dn);
		if (ret < 0) {
			scf_vector_free(dn_pointers);
			return ret;
		}

		for (j = 0; j  < dn_pointers->size; j++) {
			dn_pointer = dn_pointers->data[j];
			v          = dn_pointer->var;

			scf_logd("i: %d, dn_pointers->size: %d, pointer: v_%d_%d/%s\n", i, dn_pointers->size, v->w->line, v->w->pos, v->w->text->data);

			ret  = __alias_dereference(aliases, dn_pointer, c, bb, bb_list_head);
			if (ret < 0) {
				if (dn == dn_pointer || 1 == dn->var->nb_pointers) {
					scf_loge("\n");
					scf_vector_free(dn_pointers);
					return ret;
				}
			}

			if (dn != dn_pointer && dn->var->nb_pointers > 1) {
				scf_logd("pointer: v_%d_%d/%s, SCF_DN_ALIAS_ALLOC\n", v->w->line, v->w->pos, v->w->text->data);

				ds = calloc(1, sizeof(scf_dn_status_t));
				if (!ds) {
					scf_vector_free(dn_pointers);
					return -ENOMEM;
				}

				ret = scf_vector_add(aliases, ds);
				if (ret < 0) {
					scf_dn_status_free(ds);
					scf_vector_free(dn_pointers);
					return ret;
				}

				ds->dag_node   = dn_pointer;
				ds->alias_type = SCF_DN_ALIAS_ALLOC;
			}

			for ( ; k < aliases->size; k++) {
				ds    = aliases->data[k];

				if (SCF_DN_ALIAS_VAR != ds->alias_type)
					continue;

				if (0 == ds->alias->var->nb_pointers)
					continue;

				ret = scf_vector_add_unique(dn_pointers, ds->alias);
				if (ret < 0) {
					scf_vector_free(dn_pointers);
					return ret;
				}
			}
		}

		scf_vector_free(dn_pointers);
		dn_pointers = NULL;
	}

	return 0;
}

static void _bb_update_dn_status(scf_vector_t* aliases, scf_list_t* start, scf_basic_block_t* bb)
{
	scf_list_t*      l;
	scf_3ac_code_t*  c;
	scf_dn_status_t* status;

	for (l = start; l != scf_list_sentinel(&bb->code_list_head); l = scf_list_next(l)) {

		c  = scf_list_data(l, scf_3ac_code_t, list);

		if (!c->active_vars)
			continue;

		int i;
		for (i = 0; i < c->active_vars->size; ) {
			status    = c->active_vars->data[i];

			if (scf_vector_find_cmp(aliases, status->dag_node, scf_dn_status_cmp))
				assert(0 == scf_vector_del(c->active_vars, status));
			else
				++i;
		}
	}
}

static int __optimize_call_bb(scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_list_t*   l;
	scf_vector_t* aliases;
	int ret;

	aliases = scf_vector_alloc();
	if (!aliases)
		return -ENOMEM;

	ret = _alias_call(aliases, c, bb, bb_list_head);
	if (ret < 0) {
		scf_loge("\n");
		scf_vector_free(aliases);
		return ret;
	}

	if (aliases->size > 0) {
		ret = _bb_copy_aliases2(bb, aliases);
		if (ret < 0) {
			scf_vector_free(aliases);
			return ret;
		}

		l = scf_list_head(&bb->code_list_head);
		l = scf_list_next(l);
		_bb_update_dn_status(aliases, l, bb);
	}

	scf_vector_free(aliases);
	aliases = NULL;
	return 0;
}

#define SCF_BB_SPLIT_MOV_CODE(start, bb0, bb1) \
			do { \
				bb1 = NULL; \
				ret = scf_basic_block_split(bb0, &bb1); \
				if (ret < 0) \
					return ret; \
				bb1->dereference_flag = bb0->dereference_flag; \
				bb1->ret_flag         = bb0->ret_flag; \
				bb0->ret_flag         = 0; \
				scf_list_add_front(&bb0->list, &bb1->list); \
				scf_basic_block_mov_code(bb1, start, bb0); \
			} while (0)

static int _optimize_call_bb(scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_list_t*   l;
	scf_list_t*   start;
	scf_list_t*   sentinel;

	int ret;

	while (1) {
		scf_3ac_code_t*    c;
		scf_basic_block_t* bb_child;

		start    = scf_list_head(&bb->code_list_head);
		sentinel = scf_list_sentinel(&bb->code_list_head);

		for (l = start; l != sentinel; l = scf_list_next(l)) {
			c  = scf_list_data(l, scf_3ac_code_t, list);

			if (SCF_OP_CALL == c->op->type)
				break;
		}
		if (l == sentinel)
			break;

		if (l != start) {
			bb->call_flag = 0;
			SCF_BB_SPLIT_MOV_CODE(l, bb, bb_child);
			bb = bb_child;
		}
		bb->call_flag = 1;

		ret = __optimize_call_bb(c, bb, bb_list_head);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

		l = scf_list_head(&bb->code_list_head);
		l = scf_list_next(l);
		if (l == scf_list_sentinel(&bb->code_list_head))
			break;

		SCF_BB_SPLIT_MOV_CODE(l, bb, bb_child);
		bb = bb_child;

		ret = scf_basic_block_inited_vars(bb, bb_list_head);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int _optimize_call(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
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

		int ret = _optimize_call_bb(bb, bb_list_head);
		if (ret < 0)
			return ret;
	}

	return 0;
}

scf_optimizer_t  scf_optimizer_call =
{
	.name     =  "call",

	.optimize =  _optimize_call,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
