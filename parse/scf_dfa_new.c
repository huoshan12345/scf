#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"
#include"scf_stack.h"

extern scf_dfa_module_t dfa_module_new;

typedef struct {

	int              nb_lps;
	int              nb_rps;

	scf_node_t*      new;

	scf_expr_t*      parent_expr;

} new_module_data_t;

static int _new_is_new(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_KEY_NEW == w->type;
}

static int _new_action_lp_stat(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_data_t*         d  = data;
	scf_stack_t*        s  = d->module_datas[dfa_module_new.index];
	new_module_data_t*  md = d->module_datas[dfa_module_new.index];

	md->nb_lps++;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_lp_stat"), SCF_DFA_HOOK_POST);

	return SCF_DFA_NEXT_WORD;
}

static int _new_action_new(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	SCF_CHECK_ERROR(md->new, SCF_DFA_ERROR, "\n");

	md->new = scf_node_alloc(w, SCF_OP_NEW, NULL);
	if (!md->new)
		return SCF_DFA_ERROR;

	md->nb_lps      = 0;
	md->nb_rps      = 0;
	md->parent_expr = d->expr;

	return SCF_DFA_NEXT_WORD;
}

static int _new_action_identity(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	scf_type_t* t  = NULL;
	scf_type_t* pt = NULL;

	if (scf_ast_find_type(&t, parse->ast, w->text->data) < 0)
		return SCF_DFA_ERROR;

	if (!t) {
		scf_loge("type '%s' not found\n", w->text->data);
		return SCF_DFA_ERROR;
	}

	if (scf_ast_find_type_type(&pt, parse->ast, SCF_FUNCTION_PTR) < 0)
		return SCF_DFA_ERROR;
	assert(pt);

	scf_variable_t* var = SCF_VAR_ALLOC_BY_TYPE(w, pt, 1, 1, NULL);
	SCF_CHECK_ERROR(!var, SCF_DFA_ERROR, "var '%s' alloc failed\n", w->text->data);
	var->const_literal_flag = 1;

	scf_node_t* node = scf_node_alloc(NULL, var->type, var);
	SCF_CHECK_ERROR(!node, SCF_DFA_ERROR, "node alloc failed\n");

	int ret = scf_node_add_child(md->new, node);
	SCF_CHECK_ERROR(ret < 0, SCF_DFA_ERROR, "node add child failed\n");

	w = dfa->ops->pop_word(dfa);
	if (SCF_LEX_WORD_LP != w->type) {

		if (d->expr) {
			ret = scf_expr_add_node(d->expr, md->new);
			SCF_CHECK_ERROR(ret < 0, SCF_DFA_ERROR, "expr add child failed\n");
		} else
			d->expr = md->new;

		md->new = NULL;
	}
	dfa->ops->push_word(dfa, w);

	return SCF_DFA_NEXT_WORD;
}

static int _new_action_lp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	scf_logd("d->expr: %p\n", d->expr);

	d->expr = NULL;
	d->expr_local_flag++;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_rp"),      SCF_DFA_HOOK_POST);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_comma"),   SCF_DFA_HOOK_POST);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_lp_stat"), SCF_DFA_HOOK_POST);

	return SCF_DFA_NEXT_WORD;
}

static int _new_action_rp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	md->nb_rps++;

	scf_logd("md->nb_lps: %d, md->nb_rps: %d\n", md->nb_lps, md->nb_rps);

	if (md->nb_rps < md->nb_lps) {

		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_rp"),      SCF_DFA_HOOK_POST);
		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_comma"),   SCF_DFA_HOOK_POST);
		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_lp_stat"), SCF_DFA_HOOK_POST);

		return SCF_DFA_NEXT_WORD;
	}
	assert(md->nb_rps == md->nb_lps);

	if (d->expr) {
		int ret = scf_node_add_child(md->new, d->expr);
		d->expr = NULL;
		SCF_CHECK_ERROR(ret < 0, SCF_DFA_ERROR, "node add child failed\n");
	}

	d->expr = md->parent_expr;
	d->expr_local_flag--;

	if (d->expr) {
		int ret = scf_expr_add_node(d->expr, md->new);
		SCF_CHECK_ERROR(ret < 0, SCF_DFA_ERROR, "expr add child failed\n");
	} else
		d->expr = md->new;

	md->new = NULL;

	scf_logi("d->expr: %p\n", d->expr);

	return SCF_DFA_NEXT_WORD;
}

static int _new_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	SCF_CHECK_ERROR(!d->expr, SCF_DFA_ERROR, "\n");

	int ret = scf_node_add_child(md->new, d->expr);
	d->expr = NULL;
	SCF_CHECK_ERROR(ret < 0, SCF_DFA_ERROR, "node add child failed\n");

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_comma"),   SCF_DFA_HOOK_POST);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "new_lp_stat"), SCF_DFA_HOOK_POST);

	return SCF_DFA_SWITCH_TO;
}

static int _dfa_init_module_new(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, new, new,       _new_is_new,          _new_action_new);
	SCF_DFA_MODULE_NODE(dfa, new, identity,  scf_dfa_is_identity,  _new_action_identity);

	SCF_DFA_MODULE_NODE(dfa, new, lp,        scf_dfa_is_lp,        _new_action_lp);
	SCF_DFA_MODULE_NODE(dfa, new, rp,        scf_dfa_is_rp,        _new_action_rp);
	SCF_DFA_MODULE_NODE(dfa, new, lp_stat,   scf_dfa_is_lp,        _new_action_lp_stat);
	SCF_DFA_MODULE_NODE(dfa, new, comma,     scf_dfa_is_comma,     _new_action_comma);

	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = parse->dfa_data;
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	assert(!md);

	md = calloc(1, sizeof(new_module_data_t));
	if (!md) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	d->module_datas[dfa_module_new.index] = md;

	return SCF_DFA_OK;
}

static int _dfa_fini_module_new(scf_dfa_t* dfa)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = parse->dfa_data;
	new_module_data_t*  md    = d->module_datas[dfa_module_new.index];

	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_new.index] = NULL;
	}

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_new(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, new,  new,       new);
	SCF_DFA_GET_MODULE_NODE(dfa, new,  identity,  identity);
	SCF_DFA_GET_MODULE_NODE(dfa, new,  lp,        lp);
	SCF_DFA_GET_MODULE_NODE(dfa, new,  rp,        rp);
	SCF_DFA_GET_MODULE_NODE(dfa, new,  comma,     comma);

	SCF_DFA_GET_MODULE_NODE(dfa, expr, entry,     expr);

	scf_dfa_node_add_child(new,      identity);
	scf_dfa_node_add_child(identity, lp);

	scf_dfa_node_add_child(lp,       rp);

	scf_dfa_node_add_child(lp,       expr);
	scf_dfa_node_add_child(expr,     comma);
	scf_dfa_node_add_child(comma,    expr);
	scf_dfa_node_add_child(expr,     rp);

	return 0;
}

scf_dfa_module_t dfa_module_new =
{
	.name        = "new",
	.init_module = _dfa_init_module_new,
	.init_syntax = _dfa_init_syntax_new,

	.fini_module = _dfa_fini_module_new,
};
