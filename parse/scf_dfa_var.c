#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_var;

int _expr_multi_rets(scf_expr_t* e);

int _check_recursive(scf_type_t* parent, scf_type_t* child, scf_lex_word_t* w)
{
	if (child->type == parent->type) {
	
		scf_loge("recursive define '%s' type member var '%s' in type '%s', line: %d\n",
				child->name->data, w->text->data, parent->name->data, w->line);

		return SCF_DFA_ERROR;
	}

	if (child->scope) {
		assert(child->type >= SCF_STRUCT);

		scf_variable_t* v      = NULL;
		scf_type_t*     type_v = NULL;
		int i;

		for (i = 0; i < child->scope->vars->size; i++) {

			v = child->scope->vars->data[i];
			if (v->nb_pointers > 0 || v->type < SCF_STRUCT)
				continue;

			type_v = scf_block_find_type_type((scf_block_t*)child, v->type);
			assert(type_v);

			if (_check_recursive(parent, type_v, v->w) < 0)
				return SCF_DFA_ERROR;
		}
	}

	return SCF_DFA_OK;
}

static int _var_add_var(scf_dfa_t* dfa, dfa_data_t* d)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_identity_t*  id    = scf_stack_top(d->current_identities);

	if (id && id->identity) {

		scf_variable_t* v = scf_scope_find_variable(parse->ast->current_block->scope, id->identity->text->data);
		if (v) {
			scf_loge("repeated declare var '%s', line: %d\n", id->identity->text->data, id->identity->line);
			return SCF_DFA_ERROR;
		}

		assert(d->current_identities->size >= 2);

		dfa_identity_t* id0 = d->current_identities->data[0];
		assert(id0 && id0->type);

		scf_block_t* b = parse->ast->current_block;
		while (b) {
			if (b->node.type >= SCF_STRUCT || SCF_FUNCTION == b->node.type)
				break;
			b = (scf_block_t*)b->node.parent;
		}

		uint32_t global_flag;
		uint32_t local_flag;
		uint32_t member_flag;

		if (!b) {
			local_flag  = 0;
			global_flag = 1;
			member_flag = 0;

		} else if (SCF_FUNCTION == b->node.type) {
			local_flag  = 1;
			global_flag = 0;
			member_flag = 0;

		} else if (b->node.type >= SCF_STRUCT) {
			local_flag  = 0;
			global_flag = 0;
			member_flag = 1;

			if (0 == id0->nb_pointers && id0->type->type >= SCF_STRUCT) {
				// if not pointer var, check if define recursive struct/union/class var

				if (_check_recursive((scf_type_t*)b, id0->type, id->identity) < 0) {

					scf_loge("recursive define when define var '%s', line: %d\n",
							id->identity->text->data, id->identity->line);
					return SCF_DFA_ERROR;
				}
			}
		}

		if (SCF_FUNCTION_PTR == id0->type->type
				&& (!id0->func_ptr || 0 == id0->nb_pointers)) {
			scf_loge("invalid func ptr\n");
			return SCF_DFA_ERROR;
		}

		if (id0->extern_flag) {
			if (!global_flag) {
				scf_loge("extern var must be global.\n");
				return SCF_DFA_ERROR;
			}

			scf_variable_t* v = scf_block_find_variable(parse->ast->current_block, id->identity->text->data);
			if (v) {
				scf_loge("extern var already declared, line: %d\n", v->w->line);
				return SCF_DFA_ERROR;
			}
		}

		if (SCF_VAR_VOID == id0->type->type && 0 == id0->nb_pointers) {
			scf_loge("void var must be a pointer, like void*\n");
			return SCF_DFA_ERROR;
		}

		v = SCF_VAR_ALLOC_BY_TYPE(id->identity, id0->type, id0->const_flag, id0->nb_pointers, id0->func_ptr);
		if (!v) {
			scf_loge("alloc var failed\n");
			return SCF_DFA_ERROR;
		}
		v->local_flag  = local_flag;
		v->global_flag = global_flag;
		v->member_flag = member_flag;

		v->static_flag = id0->static_flag;
		v->extern_flag = id0->extern_flag;

		scf_logi("type: %d, nb_pointers: %d,nb_dimentions: %d, var: %s,line:%d,pos:%d, local: %d, global: %d, member: %d, extern: %d, static: %d\n\n",
				v->type, v->nb_pointers, v->nb_dimentions,
				v->w->text->data, v->w->line, v->w->pos,
				v->local_flag,  v->global_flag, v->member_flag,
				v->extern_flag, v->static_flag);

		scf_scope_push_var(parse->ast->current_block->scope, v);

		d->current_var   = v;
		d->current_var_w = id->identity;
		id0->nb_pointers = 0;
		id0->const_flag  = 0;
		id0->static_flag = 0;
		id0->extern_flag = 0;

		scf_stack_pop(d->current_identities);
		free(id);
		id = NULL;
	}

	return 0;
}

static int _var_init_expr(scf_dfa_t* dfa, dfa_data_t* d, scf_vector_t* words, int semi_flag)
{
	scf_parse_t*    parse = dfa->priv;
	scf_variable_t* r     = NULL;
	scf_lex_word_t* w     = words->data[words->size - 1];

	if (!d->expr) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	assert(d->current_var);

	d->expr_local_flag--;

	if (d->current_var->global_flag
			|| (d->current_var->const_flag && 0 == d->current_var->nb_pointers + d->current_var->nb_dimentions)) {

		if (scf_expr_calculate(parse->ast, d->expr, &r) < 0) {
			scf_loge("\n");

			scf_expr_free(d->expr);
			d->expr = NULL;
			return SCF_DFA_ERROR;
		}

		if (!scf_variable_const(r) && SCF_OP_ASSIGN != d->expr->nodes[0]->type) {
			scf_loge("number of array should be constant, file: %s, line: %d\n", w->file->data, w->line);

			scf_expr_free(d->expr);
			d->expr = NULL;
			return SCF_DFA_ERROR;
		}

		scf_expr_free(d->expr);

	} else {
		assert(d->expr->nb_nodes > 0);

		scf_node_add_child((scf_node_t*)parse->ast->current_block, (scf_node_t*)d->expr);

		scf_logd("d->expr->parent->type: %d\n", d->expr->parent->type);

		if (_expr_multi_rets(d->expr) < 0) {
			scf_loge("\n");
			return SCF_DFA_ERROR;
		}

		d->expr->semi_flag = semi_flag;
	}

	d->expr = NULL;
	return 0;
}

static int _var_add_vla(scf_ast_t* ast, scf_variable_t* vla)
{
	scf_function_t* f   = NULL;
	scf_expr_t*     e   = NULL;
	scf_expr_t*     e2  = NULL;
	scf_node_t*     mul = NULL;

	if (scf_ast_find_function(&f, ast, "printf") < 0 || !f) {
		scf_loge("printf() NOT found, which used to print error message when the variable length of array '%s' <= 0, file: %s, line: %d\n",
				vla->w->text->data, vla->w->file->data, vla->w->line);
		return SCF_DFA_ERROR;
	}

	int size = vla->data_size;
	int i;

	for (i = 0; i < vla->nb_dimentions; i++) {

		if (vla->dimentions[i].num > 0) {
			size *= vla->dimentions[i].num;
			continue;
		}

		if (0 == vla->dimentions[i].num) {
			scf_loge("\n");

			scf_expr_free(e);
			return SCF_DFA_ERROR;
		}

		if (!vla->dimentions[i].vla) {
			scf_loge("\n");

			scf_expr_free(e);
			return SCF_DFA_ERROR;
		}

		if (!e) {
			e = scf_expr_clone(vla->dimentions[i].vla);
			if (!e)
				return -ENOMEM;
			continue;
		}

		e2 = scf_expr_clone(vla->dimentions[i].vla);
		if (!e2) {
			scf_expr_free(e);
			return -ENOMEM;
		}

		mul = scf_node_alloc(vla->w, SCF_OP_MUL, NULL);
		if (!mul) {
			scf_expr_free(e2);
			scf_expr_free(e);
			return -ENOMEM;
		}

		int ret = scf_expr_add_node(e, mul);
		if (ret < 0) {
			scf_expr_free(mul);
			scf_expr_free(e2);
			scf_expr_free(e);
			return ret;
		}

		ret = scf_expr_add_node(e, e2);
		if (ret < 0) {
			scf_expr_free(e2);
			scf_expr_free(e);
			return ret;
		}
	}

	assert(e);

	scf_variable_t* v;
	scf_type_t*     t;
	scf_node_t*     node;

	if (size > 1) {
		mul = scf_node_alloc(vla->w, SCF_OP_MUL, NULL);
		if (!mul) {
			scf_expr_free(e);
			return -ENOMEM;
		}

		int ret = scf_expr_add_node(e, mul);
		if (ret < 0) {
			scf_expr_free(mul);
			scf_expr_free(e);
			return ret;
		}

		t = scf_block_find_type_type(ast->current_block, SCF_VAR_INT);
		v = SCF_VAR_ALLOC_BY_TYPE(vla->w, t, 1, 0, NULL);
		if (!v) {
			scf_expr_free(e);
			return SCF_DFA_ERROR;
		}
		v->data.i64    = size;
		v->global_flag = 1;
		v->const_literal_flag = 1;

		node = scf_node_alloc(NULL, v->type, v);
		scf_variable_free(v);
		v = NULL;
		if (!node) {
			scf_expr_free(e);
			return SCF_DFA_ERROR;
		}

		ret = scf_expr_add_node(e, node);
		if (ret < 0) {
			scf_node_free(node);
			scf_expr_free(e);
			return ret;
		}
	}

	scf_node_t*  assign;
	scf_node_t*  len;
	scf_node_t*  alloc;

	// len = e
	assign = scf_node_alloc(vla->w, SCF_OP_ASSIGN, NULL);
	if (!assign) {
		scf_expr_free(e);
		return SCF_DFA_ERROR;
	}

	scf_node_add_child(assign, e->nodes[0]);
	e->nodes[0]    = assign;
	assign->parent = e;

	scf_node_add_child((scf_node_t*)ast->current_block, e);
	e = NULL;

	t = scf_block_find_type_type(ast->current_block, SCF_VAR_INT);
	v = SCF_VAR_ALLOC_BY_TYPE(vla->w, t, 0, 0, NULL);
	if (!v)
		return SCF_DFA_ERROR;
	v->tmp_flag = 1;

	len = scf_node_alloc(NULL, v->type, v);
	if (!len) {
		scf_variable_free(v);
		return SCF_DFA_ERROR;
	}

	scf_node_add_child(assign, len);
	SCF_XCHG(assign->nodes[0], assign->nodes[1]);

	// vla_alloc(vla, len, printf, msg)
	len = scf_node_alloc(NULL, v->type, v);
	scf_variable_free(v);
	v = NULL;
	if (!len)
		return SCF_DFA_ERROR;

	alloc = scf_node_alloc(vla->w, SCF_OP_VLA_ALLOC, NULL);
	if (!alloc) {
		scf_node_free(len);
		return -ENOMEM;
	}

	// vla node
	node = scf_node_alloc(NULL, vla->type, vla);
	if (!node) {
		scf_node_free(len);
		scf_node_free(alloc);
		return SCF_DFA_ERROR;
	}

	scf_node_add_child(alloc, node);
	scf_node_add_child(alloc, len);
	node = NULL;
	len  = NULL;

	// printf() node
	t = scf_block_find_type_type(ast->current_block, SCF_FUNCTION_PTR);
	v = SCF_VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	if (!v) {
		scf_node_free(alloc);
		return SCF_DFA_ERROR;
	}
	v->const_literal_flag = 1;

	node = scf_node_alloc(NULL, v->type, v);
	scf_variable_free(v);
	v = NULL;
	if (!node) {
		scf_node_free(alloc);
		return SCF_DFA_ERROR;
	}

	scf_node_add_child(alloc, node);
	node = NULL;

	// msg
	char msg[1024];
	snprintf(msg, sizeof(msg) - 1, "\033[31merror:\033[0m variable length '%%d' of array '%s' not more than 0, file: %s, line: %d\n",
			vla->w->text->data, vla->w->file->data, vla->w->line);

	t = scf_block_find_type_type(ast->current_block, SCF_VAR_CHAR);
	v = SCF_VAR_ALLOC_BY_TYPE(vla->w, t, 1, 1, NULL);
	if (!v) {
		scf_node_free(alloc);
		return SCF_DFA_ERROR;
	}
	v->const_literal_flag = 1;
	v->global_flag = 1;

	v->data.s = scf_string_cstr(msg);
	if (!v->data.s) {
		scf_node_free(alloc);
		scf_variable_free(v);
		return -ENOMEM;
	}

	node = scf_node_alloc(NULL, v->type, v);
	scf_variable_free(v);
	v = NULL;
	if (!node) {
		scf_node_free(alloc);
		return SCF_DFA_ERROR;
	}
	scf_node_add_child(alloc, node);
	node = NULL;

	scf_node_add_child((scf_node_t*)ast->current_block, alloc);
	return 0;
}

static int _var_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		scf_loge("add var error\n");
		return SCF_DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	if (d->current_var) {
		scf_variable_size(d->current_var);

		if (d->current_var->vla_flag) {

			if (_var_add_vla(parse->ast, d->current_var) < 0)
				return SCF_DFA_ERROR;
		}
	}

	if (d->expr_local_flag > 0 && _var_init_expr(dfa, d, words, 0) < 0)
		return SCF_DFA_ERROR;

	return SCF_DFA_SWITCH_TO;
}

static int _var_action_semicolon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_identity_t*  id    = NULL;

	d->var_semicolon_flag = 0;

	if (_var_add_var(dfa, d) < 0) {
		scf_loge("add var error\n");
		return SCF_DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	id = scf_stack_pop(d->current_identities);
	assert(id && id->type);
	free(id);
	id = NULL;

	if (d->current_var) {
		scf_variable_size(d->current_var);

		if (d->current_var->vla_flag) {

			if (_var_add_vla(parse->ast, d->current_var) < 0)
				return SCF_DFA_ERROR;
		}
	}

	if (d->expr_local_flag > 0) {

		if (_var_init_expr(dfa, d, words, 1) < 0)
			return SCF_DFA_ERROR;

	} else if (d->expr) {
		scf_expr_free(d->expr);
		d->expr = NULL;
	}

	scf_node_t* b = (scf_node_t*)parse->ast->current_block;

	if (b->nb_nodes > 0)
		b->nodes[b->nb_nodes - 1]->semi_flag = 1;

	return SCF_DFA_OK;
}

static int _var_action_assign(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		scf_loge("add var error\n");
		return SCF_DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	scf_lex_word_t*  w = words->data[words->size - 1];

	if (!d->current_var) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	if (d->current_var->extern_flag) {
		scf_loge("extern var '%s' can't be inited here, line: %d\n",
				d->current_var->w->text->data, w->line);
		return SCF_DFA_ERROR;
	}

	if (d->current_var->vla_flag) {

		if (_var_add_vla(parse->ast, d->current_var) < 0)
			return SCF_DFA_ERROR;
	}

	if (d->current_var->nb_dimentions > 0) {
		scf_logi("var array '%s' init, nb_dimentions: %d\n",
				d->current_var->w->text->data, d->current_var->nb_dimentions);
		return SCF_DFA_NEXT_WORD;
	}

	scf_operator_t* op = scf_find_base_operator_by_type(SCF_OP_ASSIGN);
	scf_node_t*     n0 = scf_node_alloc(w, op->type, NULL);
	n0->op = op;

	scf_node_t*     n1 = scf_node_alloc(d->current_var_w, d->current_var->type, d->current_var);
	scf_expr_t*     e  = scf_expr_alloc();

	scf_node_add_child(n0, n1);
	scf_expr_add_node(e, n0);

	d->expr         = e;
	d->expr_local_flag++;

	if (!d->var_semicolon_flag) {
		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "var_semicolon"), SCF_DFA_HOOK_POST);
		d->var_semicolon_flag = 1;
	}

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "var_comma"), SCF_DFA_HOOK_POST);

	scf_logd("d->expr: %p\n", d->expr);

	return SCF_DFA_NEXT_WORD;
}

static int _var_action_ls(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		scf_loge("add var error\n");
		return SCF_DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	if (!d->current_var) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	assert(!d->expr);
	scf_variable_add_array_dimention(d->current_var, -1, NULL);
	d->current_var->const_literal_flag = 1;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "var_rs"), SCF_DFA_HOOK_POST);

	d->nb_lss++;

	return SCF_DFA_NEXT_WORD;
}

static int _var_action_rs(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	scf_variable_t* r     = NULL;
	scf_lex_word_t* w     = words->data[words->size - 1];

	d->nb_rss++;

	scf_logd("d->expr: %p\n", d->expr);

	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;

		if (scf_expr_calculate(parse->ast, d->expr, &r) < 0) {
			scf_loge("scf_expr_calculate\n");

			scf_expr_free(d->expr);
			d->expr = NULL;
			return SCF_DFA_ERROR;
		}

		assert(d->current_var->dim_index < d->current_var->nb_dimentions);

		if (!scf_variable_const(r) && SCF_OP_ASSIGN != d->expr->nodes[0]->type) {

			if (!d->current_var->local_flag) {
				scf_loge("variable length array '%s' must in local scope, file: %s, line: %d\n",
						d->current_var->w->text->data, w->file->data, w->line);

				scf_variable_free(r);
				r = NULL;

				scf_expr_free(d->expr);
				d->expr = NULL;
				return SCF_DFA_ERROR;
			}

			scf_logw("define variable length array, file: %s, line: %d\n", w->file->data, w->line);

			d->current_var->dimentions[d->current_var->dim_index].vla = d->expr;
			d->current_var->vla_flag = 1;
			d->expr = NULL;
		} else {
			d->current_var->dimentions[d->current_var->dim_index].num = r->data.i;

			scf_logi("dimentions: %d, size: %d\n",
					d->current_var->dim_index, d->current_var->dimentions[d->current_var->dim_index].num);

			scf_expr_free(d->expr);
			d->expr = NULL;
		}

		d->current_var->dim_index++;

		scf_variable_free(r);
		r = NULL;
	}

	return SCF_DFA_SWITCH_TO;
}

static int _dfa_init_module_var(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, var, comma,     scf_dfa_is_comma,     _var_action_comma);
	SCF_DFA_MODULE_NODE(dfa, var, semicolon, scf_dfa_is_semicolon, _var_action_semicolon);

	SCF_DFA_MODULE_NODE(dfa, var, ls,        scf_dfa_is_ls,        _var_action_ls);
	SCF_DFA_MODULE_NODE(dfa, var, rs,        scf_dfa_is_rs,        _var_action_rs);

	SCF_DFA_MODULE_NODE(dfa, var, assign,    scf_dfa_is_assign,    _var_action_assign);

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_var(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, var,    comma,     comma);
	SCF_DFA_GET_MODULE_NODE(dfa, var,    semicolon, semicolon);

	SCF_DFA_GET_MODULE_NODE(dfa, var,    ls,        ls);
	SCF_DFA_GET_MODULE_NODE(dfa, var,    rs,        rs);
	SCF_DFA_GET_MODULE_NODE(dfa, var,    assign,    assign);

	SCF_DFA_GET_MODULE_NODE(dfa, type,   star,      star);
	SCF_DFA_GET_MODULE_NODE(dfa, type,   identity,  identity);

	SCF_DFA_GET_MODULE_NODE(dfa, expr,   entry,     expr);

	SCF_DFA_GET_MODULE_NODE(dfa, init_data, entry,  init_data);
	SCF_DFA_GET_MODULE_NODE(dfa, init_data, rb,     init_data_rb);


	scf_dfa_node_add_child(identity,  comma);
	scf_dfa_node_add_child(comma,     star);
	scf_dfa_node_add_child(comma,     identity);

	// array var
	scf_dfa_node_add_child(identity,  ls);
	scf_dfa_node_add_child(ls,        rs);
	scf_dfa_node_add_child(ls,        expr);
	scf_dfa_node_add_child(expr,      rs);
	scf_dfa_node_add_child(rs,        ls);
	scf_dfa_node_add_child(rs,        comma);
	scf_dfa_node_add_child(rs,        semicolon);

	// var init
	scf_dfa_node_add_child(rs,        assign);
	scf_dfa_node_add_child(identity,  assign);

	// normal var init
	scf_dfa_node_add_child(assign,    expr);
	scf_dfa_node_add_child(expr,      comma);
	scf_dfa_node_add_child(expr,      semicolon);

	// struct or array init
	scf_dfa_node_add_child(assign,       init_data);
	scf_dfa_node_add_child(init_data_rb, comma);
	scf_dfa_node_add_child(init_data_rb, semicolon);

	scf_dfa_node_add_child(identity,  semicolon);
	return 0;
}

scf_dfa_module_t dfa_module_var = 
{
	.name        = "var",
	.init_module = _dfa_init_module_var,
	.init_syntax = _dfa_init_syntax_var,
};
