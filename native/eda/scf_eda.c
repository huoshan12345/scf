#include"scf_eda.h"
#include"scf_basic_block.h"
#include"scf_3ac.h"

static char* component_types[SCF_EDA_Components_NB] =
{
    "None",
    "Battery",
    "Resistor",
    "Capacitor",
    "Inductor",
    "Diode",
    "Transistor",
};

int	scf_eda_open(scf_native_t* ctx, const char* arch)
{
	scf_eda_context_t* eda = calloc(1, sizeof(scf_eda_context_t));
	if (!eda)
		return -ENOMEM;

	ctx->priv = eda;
	return 0;
}

int scf_eda_close(scf_native_t* ctx)
{
	scf_eda_context_t* eda = ctx->priv;

	if (eda) {
		free(eda);
		eda = NULL;
	}
	return 0;
}

static int _eda_make_insts_for_list(scf_native_t* ctx, scf_list_t* h, int bb_offset)
{
	scf_list_t* l;
	int ret;

	for (l = scf_list_head(h); l != scf_list_sentinel(h); l = scf_list_next(l)) {

		scf_3ac_code_t* c = scf_list_data(l, scf_3ac_code_t, list);

		eda_inst_handler_pt h = scf_eda_find_inst_handler(c->op->type);
		if (!h) {
			scf_loge("3ac operator '%s' not supported\n", c->op->name);
			return -EINVAL;
		}

		ret = h(ctx, c);
		if (ret < 0) {
			scf_3ac_code_print(c, NULL);
			scf_loge("3ac op '%s' make inst failed\n", c->op->name);
			return ret;
		}

		if (!c->instructions)
			continue;

		scf_3ac_code_print(c, NULL);
	}

	return bb_offset;
}

static int __eda_dfs_mask(scf_function_t* f, ScfEpin* mask, scf_basic_block_t* root)
{
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* po;

	if (root->visit_flag)
		return 0;
	root->visit_flag = 1;

	if (root->mask_pin) {
		int ret = __eda_bit_and(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p0, root->mask_pin);
		EDA_PIN_ADD_PIN_EF(f->ef, p1, mask);
		root->mask_pin = po;
	} else
		root->mask_pin = mask;

	int i;
	for (i = 0; i < root->nexts->size; i++) {
		int ret = __eda_dfs_mask(f, mask, root->nexts->data[i]);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int __eda_bb_mask(scf_function_t* f, ScfEpin* mask, scf_basic_block_t* root)
{
	scf_basic_block_t* bb;
	scf_list_t*        l;
	int i;

	for (l = scf_list_head(&f->basic_block_list_head); l != scf_list_sentinel(&f->basic_block_list_head); l = scf_list_next(l)) {
		bb = scf_list_data(l, scf_basic_block_t, list);
		bb->visit_flag = 0;
	}

	for (i = 0; i < root->dominators->size; i++) {
		bb =        root->dominators->data[i];

		if (bb->dfo < root->dfo) {
			bb->visit_flag = 1;
			scf_logw("dom->index: %d, dfo: %d, root->index: %d, dfo: %d\n", bb->index, bb->dfo, root->index, root->dfo);
			break;
		}
	}

	return __eda_dfs_mask(f, mask, root);
}

static int __eda_jmp_mask(scf_function_t* f, scf_3ac_code_t* c, scf_basic_block_t* bb)
{
	scf_3ac_operand_t* dst = c->dsts->data[0];
	scf_basic_block_t* bb2;
	scf_list_t*        l;

	ScfEpin* __true  = NULL;
	ScfEpin* __false = NULL;
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* p2;
	ScfEpin* po;

	int ret;
	int i;
	switch (c->op->type) {

		case SCF_OP_3AC_JZ:
			ret = __eda_bit_not(f, &__false, &__true);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_ZERO], __false);
			break;
		case SCF_OP_3AC_JNZ:
			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_ZERO], __true);
			break;

		case SCF_OP_3AC_JLT:
			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_SIGN], __true);
			break;
		case SCF_OP_3AC_JGE:
			ret = __eda_bit_not(f, &__false, &__true);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_SIGN], __false);
			break;

		case SCF_OP_3AC_JGT:
			ret = __eda_bit_not(f, &p1, &p2);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_SIGN], p1);

			ret = __eda_bit_and(f, &p0, &p1, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p1, p2);
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_ZERO], p0);

			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, po, __true);
			break;

		case SCF_OP_3AC_JLE:
			ret = __eda_bit_not(f, &p0, &p2);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_ZERO], p0);

			ret = __eda_bit_or(f, &p0, &p1, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p0, p2);
			EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[SCF_EDA_FLAG_SIGN], p1);

			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, po, __true);
			break;

		case SCF_OP_GOTO:
			scf_logi("'%s'\n", c->op->name);
			return 0;
			break;
		default:
			scf_loge("'%s' not support\n", c->op->name);
			return -EINVAL;
			break;
	};

	if (__true)
		__true->flags |= SCF_EDA_PIN_CF;

	for (i = 0; i < bb->nexts->size; i++) {
		bb2       = bb->nexts->data[i];

		if (bb2 == dst->bb)
			ret = __eda_bb_mask(f, __true, bb2);
		else
			ret = __eda_bb_mask(f, __false, bb2);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int _eda_fix_jmps(scf_native_t* ctx, scf_function_t* f)
{
	scf_basic_block_t* cur_bb;
	scf_basic_block_t* bb;
	scf_3ac_code_t*    c;
	scf_list_t*        l;

	int i;
	for (i = 0; i < f->jmps->size; i++) {
		c  =        f->jmps->data[i];

		cur_bb = c->basic_block;

		for (l = scf_list_prev(&cur_bb->list); l != scf_list_sentinel(&f->basic_block_list_head); l = scf_list_prev(l)) {
			bb = scf_list_data(l, scf_basic_block_t, list);

			if (!bb->jmp_flag)
				break;
		}

		if (l == scf_list_sentinel(&f->basic_block_list_head))
			continue;

		int ret = __eda_jmp_mask(f, c, bb);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int	_scf_eda_select_inst(scf_native_t* ctx)
{
	scf_eda_context_t*	eda = ctx->priv;
	scf_function_t*     f   = eda->f;
	scf_basic_block_t*  bb;
	scf_bb_group_t*     bbg;

	int i;
	int j;
	int ret = 0;

	_eda_fix_jmps(ctx, f);

	for (i  = 0; i < f->bb_groups->size; i++) {
		bbg =        f->bb_groups->data[i];

		for (j = 0; j < bbg->body->size; j++) {
			bb =        bbg->body->data[j];

			assert(!bb->native_flag);

			scf_logd("************ bb: %d\n", bb->index);
			ret = _eda_make_insts_for_list(ctx, &bb->code_list_head, 0);
			if (ret < 0)
				return ret;
			bb->native_flag = 1;
			scf_logd("************ bb: %d\n", bb->index);
		}
	}

	for (i  = 0; i < f->bb_loops->size; i++) {
		bbg =        f->bb_loops->data[i];

		for (j = 0; j < bbg->body->size; j++) {
			bb =        bbg->body->data[j];

			assert(!bb->native_flag);

			ret = _eda_make_insts_for_list(ctx, &bb->code_list_head, 0);
			if (ret < 0)
				return ret;
			bb->native_flag = 1;
		}
	}

	return 0;
}

int scf_eda_select_inst(scf_native_t* ctx, scf_function_t* f)
{
	scf_eda_context_t* eda = ctx->priv;
	ScfEcomponent*     B   = NULL;

	scf_dag_node_t*    dn;
	scf_list_t*        l;

	eda->f = f;

	assert(!f->ef);

	f->ef = scf_efunction__alloc(f->node.w->text->data);
	if (!f->ef)
		return -ENOMEM;

	EDA_INST_ADD_COMPONENT(f->ef, B, SCF_EDA_Battery);

	B->pins[SCF_EDA_Battery_NEG]->flags = SCF_EDA_PIN_NEG;
	B->pins[SCF_EDA_Battery_POS]->flags = SCF_EDA_PIN_POS;

	int ret = _scf_eda_select_inst(ctx);
	if (ret < 0)
		return ret;

#if 0
	ScfEcomponent* c;
	ScfEpin*       p;
	size_t i;
	size_t j;
	size_t k;

	for (i = 0; i < f->ef->n_components; i++) {
		c  =        f->ef->components[i];

		printf("c: %ld, type: %s\n", c->id, component_types[c->type]);

		for (j = 0; j < c->n_pins; j++) {
			p  =        c->pins[j];

			printf("cid: %ld, pid: %ld, flags: %#lx\n", p->cid, p->id, p->flags);

			for (k = 0; k + 1 < p->n_tos; k += 2)
				printf("to cid: %ld, pid: %ld\n", p->tos[k], p->tos[k + 1]);
		}
		printf("\n");
	}
#endif
	return 0;
}

scf_native_ops_t	native_ops_eda = {
	.name            = "eda",

	.open            = scf_eda_open,
	.close           = scf_eda_close,

	.select_inst     = scf_eda_select_inst,
};
