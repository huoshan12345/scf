#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_async;

static int _async_is_async(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_KEY_ASYNC == w->type;
}

static int _async_action_async(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	scf_lex_word_t*   w     = words->data[words->size - 1];

	if (d->expr) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	d->current_async_w = w;

	d->expr_local_flag = 1;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "async_semicolon"), SCF_DFA_HOOK_POST);

	return SCF_DFA_NEXT_WORD;
}

static int _async_action_semicolon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;
	scf_expr_t*   e     = d->expr;

	if (!d->expr) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	while (SCF_OP_EXPR == e->type) {

		assert(e->nodes && 1 == e->nb_nodes);

		e = e->nodes[0];
	}

	if (SCF_OP_CALL != e->type) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	e->parent->nodes[0] = NULL;

	scf_expr_free(d->expr);
	d->expr = NULL;
	d->expr_local_flag = 0;

	scf_type_t*     t   = NULL;
	scf_function_t* f   = NULL;
	scf_variable_t* v   = NULL;
	scf_node_t*     pf  = NULL;
	scf_node_t*     fmt = NULL;

	if (scf_ast_find_type_type(&t, parse->ast, SCF_FUNCTION_PTR) < 0)
		return SCF_DFA_ERROR;

	if (scf_ast_find_function(&f, parse->ast, "scf_async") < 0)
		return SCF_DFA_ERROR;
	if (!f) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	v = SCF_VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	if (!v) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}
	v->const_literal_flag = 1;

	pf = scf_node_alloc(d->current_async_w, v->type, v);
	if (!pf) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	if (scf_ast_find_type_type(&t, parse->ast, SCF_VAR_CHAR) < 0)
		return SCF_DFA_ERROR;

	v = SCF_VAR_ALLOC_BY_TYPE(d->current_async_w, t, 1, 1, NULL);
	if (!v) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}
	v->const_literal_flag = 1;
	v->data.s = scf_string_cstr("");

	fmt = scf_node_alloc(d->current_async_w, v->type, v);
	if (!fmt) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	scf_node_add_child(e, pf);
	scf_node_add_child(e, fmt);

	int i;
	for (i = e->nb_nodes - 3; i >= 0; i--)
		e->nodes[i + 2] = e->nodes[i];

	e->nodes[0] = pf;
	e->nodes[1] = e->nodes[2];
	e->nodes[2] = fmt;

	if (d->current_node)
		scf_node_add_child(d->current_node, e);
	else
		scf_node_add_child((scf_node_t*)parse->ast->current_block, e);

	d->current_async_w = NULL;

	return SCF_DFA_OK;
}

static int _dfa_init_module_async(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, async, semicolon, scf_dfa_is_semicolon, _async_action_semicolon);
	SCF_DFA_MODULE_NODE(dfa, async, async,     _async_is_async,      _async_action_async);

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_async(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa,  async,   semicolon, semicolon);
	SCF_DFA_GET_MODULE_NODE(dfa,  async,   async,     async);
	SCF_DFA_GET_MODULE_NODE(dfa,  expr,    entry,     expr);

	scf_dfa_node_add_child(async, expr);
	scf_dfa_node_add_child(expr,  semicolon);

	return 0;
}

scf_dfa_module_t dfa_module_async =
{
	.name        = "async",
	.init_module = _dfa_init_module_async,
	.init_syntax = _dfa_init_syntax_async,
};
