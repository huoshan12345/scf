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

static int __optimize_call_bb(scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_vector_t* aliases = scf_vector_alloc();
	if (!aliases)
		return -ENOMEM;

	int ret = _alias_call(aliases, c, bb, bb_list_head);
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
	}

	scf_vector_free(aliases);
	aliases = NULL;
	return 0;
}

static int _optimize_call_bb(scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_3ac_code_t*  c;
	scf_list_t*      l;

	l = scf_list_head(&bb->code_list_head);
	c = scf_list_data(l, scf_3ac_code_t, list);

	assert(SCF_OP_CALL == c->op->type);
	assert(scf_list_next(l) == scf_list_sentinel(&bb->code_list_head));

	return __optimize_call_bb(c, bb, bb_list_head);
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

//	scf_basic_block_print_list(bb_list_head);
	return 0;
}

scf_optimizer_t  scf_optimizer_call =
{
	.name     =  "call",

	.optimize =  _optimize_call,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
