#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_enum;

typedef struct {
	scf_lex_word_t*  current_enum;
	scf_variable_t*  current_v;

	scf_dfa_hook_t*  hook;

	scf_vector_t*    vars;

	int              nb_lbs;
	int              nb_rbs;

} enum_module_data_t;

static int _enum_action_type(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];

	if (md->current_enum) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	scf_type_t* t = scf_block_find_type(parse->ast->root_block, w->text->data);
	if (!t) {
		t = scf_type_alloc(w, w->text->data, SCF_STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			scf_loge("type alloc failed\n");
			return SCF_DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.enum_flag = 1;
		scf_scope_push_type(parse->ast->root_block->scope, t);
	}

	md->current_enum = w;

	return SCF_DFA_NEXT_WORD;
}

static int _enum_action_lb(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];
	scf_type_t*          t     = NULL;

	if (md->nb_lbs > 0) {
		scf_loge("too many '{' in enum, file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	if (md->current_enum) {

		t = scf_block_find_type(parse->ast->root_block, md->current_enum->text->data);
		if (!t) {
			scf_loge("type '%s' not found\n", md->current_enum->text->data);
			return SCF_DFA_ERROR;
		}
	} else {
		t = scf_type_alloc(w, "anonymous", SCF_STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			scf_loge("type alloc failed\n");
			return SCF_DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.enum_flag = 1;
		scf_scope_push_type(parse->ast->root_block->scope, t);

		md->current_enum = w;
	}

	md->nb_lbs++;

	return SCF_DFA_NEXT_WORD;
}

static int _enum_action_var(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];

	scf_variable_t*      v0    = NULL;
	scf_variable_t*      v     = NULL;
	scf_type_t*          t     = NULL;

	if (!md->current_enum) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	if (scf_ast_find_type_type(&t, parse->ast, SCF_VAR_INT) < 0) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	v = scf_block_find_variable(parse->ast->root_block, w->text->data);
	if (v) {
		scf_loge("repeated declared enum var '%s', 1st in file: %s, line: %d\n", w->text->data, v->w->file->data, v->w->line);
		return SCF_DFA_ERROR;
	}

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 1, 0, NULL);
	if (!v) {
		scf_loge("var alloc failed\n");
		return SCF_DFA_ERROR;
	}

	v->const_literal_flag = 1;

	if (md->vars->size > 0) {
		v0          = md->vars->data[md->vars->size - 1];
		v->data.i64 = v0->data.i64 + 1;
	} else
		v->data.i64 = 0;

	if (scf_vector_add(md->vars, v) < 0) {
		scf_loge("var add failed\n");
		return SCF_DFA_ERROR;
	}

	scf_scope_push_var(parse->ast->root_block->scope, v);

	md->current_v = v;

	scf_logi("enum var: '%s', type: %d, size: %d\n", w->text->data, v->type, v->size);

	return SCF_DFA_NEXT_WORD;
}

static int _enum_action_assign(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];

	if (!md->current_v) {
		scf_loge("no enum var before '=' in file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	assert(!d->expr);
	d->expr_local_flag++;

	md->hook = SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "enum_comma"), SCF_DFA_HOOK_POST);

	return SCF_DFA_NEXT_WORD;
}

static int _enum_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];
	scf_variable_t*      r     = NULL;

	if (!md->current_v) {
		scf_loge("enum var before ',' not found, in file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;

		if (scf_expr_calculate(parse->ast, d->expr, &r) < 0) {
			scf_loge("scf_expr_calculate\n");
			return SCF_DFA_ERROR;
		}

		if (!scf_variable_const(r) && SCF_OP_ASSIGN != d->expr->nodes[0]->type) {
			scf_loge("enum var must be inited by constant, file: %s, line: %d\n", w->file->data, w->line);
			return -1;
		}

		md->current_v->data.i64 = r->data.i64;

		scf_variable_free(r);
		r = NULL;

		scf_expr_free(d->expr);
		d->expr = NULL;
		d->expr_local_flag--;
	}

	md->current_v = NULL;

	return SCF_DFA_SWITCH_TO;
}

static int _enum_action_rb(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];
	scf_variable_t*      r     = NULL;

	if (md->nb_lbs > 1) {
		scf_loge("too many '{' in enum, file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	md->nb_rbs++;

	assert(md->nb_rbs == md->nb_lbs);

	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;

		if (scf_expr_calculate(parse->ast, d->expr, &r) < 0) {
			scf_loge("scf_expr_calculate\n");
			return SCF_DFA_ERROR;
		}

		if (!scf_variable_const(r) && SCF_OP_ASSIGN != d->expr->nodes[0]->type) {
			scf_loge("enum var must be inited by constant, file: %s, line: %d\n", w->file->data, w->line);
			return -1;
		}

		md->current_v->data.i64 = r->data.i64;

		scf_variable_free(r);
		r = NULL;

		scf_expr_free(d->expr);
		d->expr = NULL;
		d->expr_local_flag--;
	}

	if (md->hook) {
		scf_dfa_del_hook(&(dfa->hooks[SCF_DFA_HOOK_POST]), md->hook);
		md->hook = NULL;
	}

	md->nb_lbs = 0;
	md->nb_rbs = 0;

	md->current_enum = NULL;
	md->current_v    = NULL;

	scf_vector_clear(md->vars, NULL);

	return SCF_DFA_NEXT_WORD;
}

static int _enum_action_semicolon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	scf_lex_word_t*      w     = words->data[words->size - 1];

	if (0 != md->nb_rbs || 0 != md->nb_lbs) {
		scf_loge("'{' and '}' not same in enum, file: %s, line: %d\n", w->file->data, w->line);

		md->nb_rbs = 0;
		md->nb_lbs = 0;
		return SCF_DFA_ERROR;
	}

	if (md->current_v) {
		scf_loge("enum var '%s' should be followed by ',' or '}', file: %s, line: %d\n",
				md->current_v->w->text->data,
				md->current_v->w->file->data,
				md->current_v->w->line);

		md->current_v = NULL;
		return SCF_DFA_ERROR;
	}

	return SCF_DFA_OK;
}

static int _dfa_init_module_enum(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, enum, _enum,     scf_dfa_is_enum,      NULL);

	SCF_DFA_MODULE_NODE(dfa, enum, type,      scf_dfa_is_identity,  _enum_action_type);

	SCF_DFA_MODULE_NODE(dfa, enum, lb,        scf_dfa_is_lb,        _enum_action_lb);
	SCF_DFA_MODULE_NODE(dfa, enum, rb,        scf_dfa_is_rb,        _enum_action_rb);
	SCF_DFA_MODULE_NODE(dfa, enum, semicolon, scf_dfa_is_semicolon, _enum_action_semicolon);

	SCF_DFA_MODULE_NODE(dfa, enum, var,       scf_dfa_is_identity,  _enum_action_var);
	SCF_DFA_MODULE_NODE(dfa, enum, assign,    scf_dfa_is_assign,    _enum_action_assign);
	SCF_DFA_MODULE_NODE(dfa, enum, comma,     scf_dfa_is_comma,     _enum_action_comma);

	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];

	assert(!md);

	md = calloc(1, sizeof(enum_module_data_t));
	if (!md) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	md->vars = scf_vector_alloc();
	if (!md->vars) {
		scf_loge("\n");
		free(md);
		return SCF_DFA_ERROR;
	}

	d->module_datas[dfa_module_enum.index] = md;

	return SCF_DFA_OK;
}

static int _dfa_fini_module_enum(scf_dfa_t* dfa)
{
	scf_parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];

	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_enum.index] = NULL;
	}

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_enum(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  _enum,    _enum);
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  type,      type);
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  lb,        lb);
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  rb,        rb);
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  semicolon, semicolon);

	SCF_DFA_GET_MODULE_NODE(dfa, enum,  assign,    assign);
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  comma,     comma);
	SCF_DFA_GET_MODULE_NODE(dfa, enum,  var,       var);
	SCF_DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);

	scf_vector_add(dfa->syntaxes,  _enum);

	// enum start
	scf_dfa_node_add_child(_enum,   type);
	scf_dfa_node_add_child(type,    semicolon);
	scf_dfa_node_add_child(type,    lb);

	// anonymous enum
	scf_dfa_node_add_child(_enum,   lb);

	// empty enum
	scf_dfa_node_add_child(lb,      rb);

	// const member var
	scf_dfa_node_add_child(lb,      var);
	scf_dfa_node_add_child(var,     comma);
	scf_dfa_node_add_child(var,     assign);
	scf_dfa_node_add_child(assign,  expr);
	scf_dfa_node_add_child(expr,    comma);
	scf_dfa_node_add_child(comma,   var);

	scf_dfa_node_add_child(var,     rb);
	scf_dfa_node_add_child(expr,    rb);

	// end
	scf_dfa_node_add_child(rb,      semicolon);

	return 0;
}

scf_dfa_module_t dfa_module_enum = 
{
	.name        = "enum",
	.init_module = _dfa_init_module_enum,
	.init_syntax = _dfa_init_syntax_enum,

	.fini_module = _dfa_fini_module_enum,
};
