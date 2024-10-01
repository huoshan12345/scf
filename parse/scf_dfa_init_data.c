#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t  dfa_module_init_data;

int scf_array_init (scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* var, scf_vector_t* init_exprs);
int scf_struct_init(scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* var, scf_vector_t* init_exprs);

int _expr_add_var(scf_parse_t* parse, dfa_data_t* d);

typedef struct {
	scf_lex_word_t*  assign;
	scf_vector_t*    init_exprs;

	dfa_index_t*     current_index;
	int              current_n;
	int              current_dim;

	int              nb_lbs;
	int              nb_rbs;
} init_module_data_t;

static int _do_data_init(scf_dfa_t* dfa, scf_vector_t* words, dfa_data_t* d)
{
	scf_parse_t*        parse = dfa->priv;
	scf_variable_t*     var   = d->current_var;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];
	dfa_init_expr_t*    ie;

	int ret = -1;
	int i   = 0;

	if (d->current_var->nb_dimentions > 0)
		ret = scf_array_init(parse->ast, md->assign, d->current_var, md->init_exprs);

	else if (d->current_var->type >=  SCF_STRUCT)
		ret = scf_struct_init(parse->ast, md->assign, d->current_var, md->init_exprs);

	if (ret < 0)
		goto error;

	for (i = 0; i < md->init_exprs->size; i++) {
		ie =        md->init_exprs->data[i];

		if (d->current_var->global_flag) {

			ret = scf_expr_calculate(parse->ast, ie->expr, NULL);
			if (ret < 0)
				goto error;

			scf_expr_free(ie->expr);
			ie->expr = NULL;

		} else {
			ret = scf_node_add_child((scf_node_t*)parse->ast->current_block, ie->expr);
			if (ret < 0)
				goto error;
			ie->expr = NULL;
		}

		free(ie);
		ie = NULL;
	}

error:
	for (; i < md->init_exprs->size; i++) {
		ie =   md->init_exprs->data[i];

		scf_expr_free(ie->expr);
		free(ie);
		ie = NULL;
	}

	md->assign = NULL;

	scf_vector_free(md->init_exprs);
	md->init_exprs = NULL;

	free(md->current_index);
	md->current_index = NULL;

	md->current_dim = -1;
	md->nb_lbs      = 0;
	md->nb_rbs      = 0;

	return ret;
}

static int _add_data_init_expr(scf_dfa_t* dfa, scf_vector_t* words, dfa_data_t* d)
{
	init_module_data_t* md = d->module_datas[dfa_module_init_data.index];
	dfa_init_expr_t*    ie;

	assert(!d->expr->parent);
	assert(md->current_dim >= 0);

	size_t N =  sizeof(dfa_index_t) * md->current_n;

	ie = malloc(sizeof(dfa_init_expr_t) + N);
	if (!ie)
		return -ENOMEM;

	ie->expr = d->expr;
	d ->expr = NULL;
	ie->n    = md->current_n;

	memcpy(ie->index, md->current_index, N);

	int ret = scf_vector_add(md->init_exprs, ie);
	if (ret < 0)
		return ret;

	return SCF_DFA_OK;
}

static int _data_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (md->current_dim < 0) {
		scf_logi("md->current_dim: %d\n", md->current_dim);
		return SCF_DFA_NEXT_SYNTAX;
	}

	if (d->expr) {
		if (_add_data_init_expr(dfa, words, d) < 0)
			return SCF_DFA_ERROR;
	}

	md->current_index[md->current_dim].w = NULL;
	md->current_index[md->current_dim].i++;

	intptr_t i = md->current_index[md->current_dim].i;

	scf_logi("\033[31m md->current_dim[%d]: %ld\033[0m\n", md->current_dim, i);

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "init_data_comma"), SCF_DFA_HOOK_POST);

	return SCF_DFA_SWITCH_TO;
}

static int _data_action_entry(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	assert(words->size >= 2);

	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 2];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	assert(SCF_LEX_WORD_ASSIGN == w->type);

	md->assign = w;
	md->nb_lbs = 0;
	md->nb_rbs = 0;

	assert(!md->init_exprs);
	assert(!md->current_index);

	md->init_exprs = scf_vector_alloc();
	if (!md->init_exprs)
		return -ENOMEM;

	md->current_dim = -1;
	md->current_n   = 0;

	d->expr_local_flag = 1;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "init_data_comma"), SCF_DFA_HOOK_POST);

	return SCF_DFA_CONTINUE;
}

static int _data_action_lb(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	d->expr = NULL;
	md->current_dim++;
	md->nb_lbs++;

	if (md->current_dim >= md->current_n) {

		void* p = realloc(md->current_index, sizeof(dfa_index_t) * (md->current_dim + 1));
		if (!p)
			return -ENOMEM;
		md->current_index = p;
		md->current_n     = md->current_dim + 1;
	}

	int i;
	for (i = md->current_dim; i < md->current_n; i++) {

		md->current_index[i].w = NULL;
		md->current_index[i].i = 0;
	}

	return SCF_DFA_NEXT_WORD;
}

static int _data_action_member(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (md->current_dim >= md->current_n) {
		scf_loge("init data not right, file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	md->current_index[md->current_dim].w = w;

	return SCF_DFA_NEXT_WORD;
}

static int _data_action_index(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	scf_lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (md->current_dim >= md->current_n) {
		scf_loge("init data not right, file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	md->current_index[md->current_dim].i = w->data.u64;

	return SCF_DFA_NEXT_WORD;
}

static int _data_action_rb(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (d->expr) {
		dfa_identity_t* id = scf_stack_top(d->current_identities);

		if (id && id->identity) {
			if (_expr_add_var(parse, d) < 0)
				return SCF_DFA_ERROR;
		}

		if (_add_data_init_expr(dfa, words, d) < 0)
			return SCF_DFA_ERROR;
	}

	md->nb_rbs++;
	md->current_dim--;

	int i;
	for (i = md->current_dim + 1; i < md->current_n; i++) {

		md->current_index[i].w = NULL;
		md->current_index[i].i = 0;
	}

	if (md->nb_rbs == md->nb_lbs) {
		d->expr_local_flag = 0;

		if (_do_data_init(dfa, words, d) < 0)
			return SCF_DFA_ERROR;

		scf_dfa_del_hook_by_name(&(dfa->hooks[SCF_DFA_HOOK_POST]), "init_data_comma");
	}

	return SCF_DFA_NEXT_WORD;
}

static int _dfa_init_module_init_data(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, init_data, entry,  scf_dfa_is_lb,             _data_action_entry);
	SCF_DFA_MODULE_NODE(dfa, init_data, comma,  scf_dfa_is_comma,          _data_action_comma);
	SCF_DFA_MODULE_NODE(dfa, init_data, lb,     scf_dfa_is_lb,             _data_action_lb);
	SCF_DFA_MODULE_NODE(dfa, init_data, rb,     scf_dfa_is_rb,             _data_action_rb);

	SCF_DFA_MODULE_NODE(dfa, init_data, dot,    scf_dfa_is_dot,            scf_dfa_action_next);
	SCF_DFA_MODULE_NODE(dfa, init_data, member, scf_dfa_is_identity,       _data_action_member);
	SCF_DFA_MODULE_NODE(dfa, init_data, index,  scf_dfa_is_const_integer,  _data_action_index);
	SCF_DFA_MODULE_NODE(dfa, init_data, assign, scf_dfa_is_assign,         scf_dfa_action_next);

	scf_parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = parse->dfa_data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	assert(!md);

	md = calloc(1, sizeof(init_module_data_t));
	if (!md)
		return -ENOMEM;

	d->module_datas[dfa_module_init_data.index] = md;

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_init_data(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, entry,  entry);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, comma,  comma);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, lb,     lb);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, rb,     rb);

	SCF_DFA_GET_MODULE_NODE(dfa, init_data, dot,    dot);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, member, member);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, index,  index);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, assign, assign);

	SCF_DFA_GET_MODULE_NODE(dfa, expr,      entry,  expr);

	// empty init, use 0 to fill the data
	scf_dfa_node_add_child(entry,     lb);
	scf_dfa_node_add_child(lb,        rb);

	// multi-dimention data init
	scf_dfa_node_add_child(lb,        lb);
	scf_dfa_node_add_child(rb,        rb);
	scf_dfa_node_add_child(rb,        comma);
	scf_dfa_node_add_child(comma,     lb);

	// init expr for member of data
	scf_dfa_node_add_child(lb,        dot);
	scf_dfa_node_add_child(comma,     dot);

	scf_dfa_node_add_child(lb,        expr);
	scf_dfa_node_add_child(expr,      comma);
	scf_dfa_node_add_child(comma,     expr);
	scf_dfa_node_add_child(expr,      rb);

	scf_dfa_node_add_child(dot,       member);
	scf_dfa_node_add_child(member,    assign);
	scf_dfa_node_add_child(assign,    expr);

	scf_dfa_node_add_child(dot,       index);
	scf_dfa_node_add_child(index,     assign);

	return 0;
}

scf_dfa_module_t dfa_module_init_data =
{
	.name        = "init_data",
	.init_module = _dfa_init_module_init_data,
	.init_syntax = _dfa_init_syntax_init_data,
};
