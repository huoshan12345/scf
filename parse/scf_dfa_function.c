#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_function;

typedef struct {

	scf_block_t*     parent_block;

} dfa_fun_data_t;

int _function_add_function(scf_dfa_t* dfa, dfa_data_t* d)
{
	if (d->current_identities->size < 2) {
		scf_loge("d->current_identities->size: %d\n", d->current_identities->size);
		return SCF_DFA_ERROR;
	}

	scf_parse_t*    parse = dfa->priv;
	scf_ast_t*      ast   = parse->ast;
	dfa_identity_t* id    = scf_stack_pop(d->current_identities);
	dfa_fun_data_t* fd    = d->module_datas[dfa_module_function.index];

	scf_function_t* f;
	scf_variable_t* v;
	scf_block_t*    b;

	if (!id || !id->identity) {
		scf_loge("function identity not found\n");
		return SCF_DFA_ERROR;
	}

	b = ast->current_block;
	while (b) {
		if (b->node.type >= SCF_STRUCT)
			break;
		b = (scf_block_t*)b->node.parent;
	}

	f = scf_function_alloc(id->identity);
	if (!f)
		return SCF_DFA_ERROR;
	f->member_flag = !!b;

	free(id);
	id = NULL;

	scf_logi("function: %s,line:%d, member_flag: %d\n", f->node.w->text->data, f->node.w->line, f->member_flag);

	int void_flag = 0;

	while (d->current_identities->size > 0) {

		id = scf_stack_pop(d->current_identities);

		if (!id || !id->type || !id->type_w) {
			scf_loge("function return value type NOT found\n");
			return SCF_DFA_ERROR;
		}

		if (SCF_VAR_VOID == id->type->type && 0 == id->nb_pointers)
			void_flag = 1;

		f->extern_flag |= id->extern_flag;
		f->static_flag |= id->static_flag;
		f->inline_flag |= id->inline_flag;

		if (f->extern_flag && (f->static_flag || f->inline_flag)) {
			scf_loge("'extern' function can't be 'static' or 'inline'\n");
			return SCF_DFA_ERROR;
		}

		v  = SCF_VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, NULL);
		free(id);
		id = NULL;

		if (!v) {
			scf_function_free(f);
			return SCF_DFA_ERROR;
		}

		if (scf_vector_add(f->rets, v) < 0) {
			scf_variable_free(v);
			scf_function_free(f);
			return SCF_DFA_ERROR;
		}
	}

	assert(f->rets->size > 0);

	if (void_flag && 1 != f->rets->size) {
		scf_loge("void function must have no other return value\n");
		return SCF_DFA_ERROR;
	}

	f->void_flag = void_flag;

	if (f->rets->size > 4) {
		scf_loge("function return values must NOT more than 4!\n");
		return SCF_DFA_ERROR;
	}

	int i;
	int j;
	for (i = 0; i < f->rets->size / 2;  i++) {
		j  =        f->rets->size - 1 - i;

		SCF_XCHG(f->rets->data[i], f->rets->data[j]);
	}

	scf_scope_push_function(ast->current_block->scope, f);

	scf_node_add_child((scf_node_t*)ast->current_block, (scf_node_t*)f);

	fd ->parent_block  = ast->current_block;
	ast->current_block = (scf_block_t*)f;

	d->current_function = f;

	return SCF_DFA_NEXT_WORD;
}

int _function_add_arg(scf_dfa_t* dfa, dfa_data_t* d)
{
	dfa_identity_t* t = NULL;
	dfa_identity_t* v = NULL;

	switch (d->current_identities->size) {
		case 0:
			break;
		case 1:
			t = scf_stack_pop(d->current_identities);
			assert(t && t->type);
			break;
		case 2:
			v = scf_stack_pop(d->current_identities);
			t = scf_stack_pop(d->current_identities);
			assert(t && t->type);
			assert(v && v->identity);
			break;
		default:
			scf_loge("\n");
			return SCF_DFA_ERROR;
			break;
	};

	if (t && t->type) {
		scf_variable_t* arg = NULL;
		scf_lex_word_t* w   = NULL;

		if (v && v->identity)
			w = v->identity;

		if (SCF_VAR_VOID == t->type->type && 0 == t->nb_pointers) {
			scf_loge("\n");
			return SCF_DFA_ERROR;
		}

		if (!d->current_var) {
			arg = SCF_VAR_ALLOC_BY_TYPE(w, t->type, t->const_flag, t->nb_pointers, t->func_ptr);
			if (!arg)
				return SCF_DFA_ERROR;

			scf_scope_push_var(d->current_function->scope, arg);
		} else {
			arg = d->current_var;

			if (arg->nb_dimentions > 0) {
				arg->nb_pointers += arg->nb_dimentions;
				arg->nb_dimentions = 0;
			}

			if (arg->dimentions) {
				free(arg->dimentions);
				arg->dimentions = NULL;
			}

			arg->const_literal_flag = 0;

			d->current_var = NULL;
		}

		scf_logi("d->argc: %d, arg->nb_pointers: %d, arg->nb_dimentions: %d\n",
				d->argc, arg->nb_pointers, arg->nb_dimentions);

		scf_vector_add(d->current_function->argv, arg);

		arg->refs++;
		arg->arg_flag   = 1;
		arg->local_flag = 1;

		if (v)
			free(v);
		free(t);

		d->argc++;
	}

	return SCF_DFA_NEXT_WORD;
}

static int _function_action_vargs(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->current_function->vargs_flag = 1;

	return SCF_DFA_NEXT_WORD;
}

static int _function_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_data_t* d = data;

	if (_function_add_arg(dfa, d) < 0) {
		scf_loge("function add arg failed\n");
		return SCF_DFA_ERROR;
	}

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "function_comma"), SCF_DFA_HOOK_PRE);

	return SCF_DFA_NEXT_WORD;
}

static int _function_action_lp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	assert(!d->current_node);

	d->current_var = NULL;

	if (_function_add_function(dfa, d) < 0)
		return SCF_DFA_ERROR;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "function_rp"),    SCF_DFA_HOOK_PRE);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "function_comma"), SCF_DFA_HOOK_PRE);

	d->argc = 0;
	d->nb_lps++;

	return SCF_DFA_NEXT_WORD;
}

static int _function_action_rp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];
	scf_function_t*  f     = d->current_function;
	scf_function_t*  fprev = NULL;

	d->nb_rps++;

	if (d->nb_rps < d->nb_lps) {
		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "function_rp"), SCF_DFA_HOOK_PRE);
		return SCF_DFA_NEXT_WORD;
	}

	if (_function_add_arg(dfa, d) < 0) {
		scf_loge("function add arg failed\n");
		return SCF_DFA_ERROR;
	}

	scf_list_del(&f->list);
	scf_node_del_child((scf_node_t*)fd->parent_block, (scf_node_t*)f);

	if (fd->parent_block->node.type >= SCF_STRUCT) {

		scf_type_t* t = (scf_type_t*)fd->parent_block;

		if (!t->node.class_flag) {
			scf_loge("only class has member function\n");
			return SCF_DFA_ERROR;
		}

		assert(t->scope);

		if (!strcmp(f->node.w->text->data, "__init")) {

			fprev = scf_scope_find_same_function(t->scope, f);

		} else if (!strcmp(f->node.w->text->data, "__release")) {

			fprev = scf_scope_find_function(t->scope, f->node.w->text->data);

			if (fprev && !scf_function_same(fprev, f)) {
				scf_loge("function '%s' can't be overloaded, repeated declare first in line: %d, second in line: %d\n",
						f->node.w->text->data, fprev->node.w->line, f->node.w->line);
				return SCF_DFA_ERROR;
			}
		} else {
			scf_loge("class member function must be '__init()' or '__release()', file: %s, line: %d\n", f->node.w->file->data, f->node.w->line);
			return SCF_DFA_ERROR;
		}
	} else {
		scf_block_t* b = fd->parent_block;

		if (!b->node.root_flag && !b->node.file_flag) {
			scf_loge("function should be defined in file, global, or class\n");
			return SCF_DFA_ERROR;
		}

		assert(b->scope);

		if (f->static_flag)
			fprev = scf_scope_find_function(b->scope, f->node.w->text->data);
		else {
			int ret = scf_ast_find_global_function(&fprev, parse->ast, f->node.w->text->data);
			if (ret < 0)
				return ret;
		}

		if (fprev && !scf_function_same(fprev, f)) {

			scf_loge("repeated declare function '%s', first in line: %d, second in line: %d, function overloading only can do in class\n",
					f->node.w->text->data, fprev->node.w->line, f->node.w->line);
			return SCF_DFA_ERROR;
		}
	}

	if (fprev) {
		if (!fprev->node.define_flag) {
			int i;
			scf_variable_t* v0;
			scf_variable_t* v1;

			for (i = 0; i < fprev->argv->size; i++) {
				v0 =        fprev->argv->data[i];
				v1 =        f    ->argv->data[i];

				if (v1->w)
					SCF_XCHG(v0->w, v1->w);
			}

			scf_function_free(f);
			d->current_function = fprev;

		} else {
			scf_lex_word_t* w = dfa->ops->pop_word(dfa);

			if (SCF_LEX_WORD_SEMICOLON != w->type) {

				scf_loge("repeated define function '%s', first in line: %d, second in line: %d\n",
						f->node.w->text->data, fprev->node.w->line, f->node.w->line); 

				dfa->ops->push_word(dfa, w);
				return SCF_DFA_ERROR;
			}

			dfa->ops->push_word(dfa, w);
		}
	} else {
		scf_scope_push_function(fd->parent_block->scope, f);

		scf_node_add_child((scf_node_t*)fd->parent_block, (scf_node_t*)f);
	}

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "function_end"), SCF_DFA_HOOK_END);

	parse->ast->current_block = (scf_block_t*)d->current_function;

	return SCF_DFA_NEXT_WORD;
}

static int _function_action_end(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	scf_lex_word_t*  w     = words->data[words->size - 1];
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	parse->ast->current_block = (scf_block_t*)(fd->parent_block);

	if (d->current_function->node.nb_nodes > 0)
		d->current_function->node.define_flag = 1;

	fd->parent_block = NULL;

	d->current_function = NULL;
	d->argc   = 0;
	d->nb_lps = 0;
	d->nb_rps = 0;

	return SCF_DFA_OK;
}

static int _dfa_init_module_function(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, function, comma,  scf_dfa_is_comma, _function_action_comma);
	SCF_DFA_MODULE_NODE(dfa, function, vargs,  scf_dfa_is_vargs, _function_action_vargs);
	SCF_DFA_MODULE_NODE(dfa, function, end,    scf_dfa_is_entry, _function_action_end);

	SCF_DFA_MODULE_NODE(dfa, function, lp,     scf_dfa_is_lp,    _function_action_lp);
	SCF_DFA_MODULE_NODE(dfa, function, rp,     scf_dfa_is_rp,    _function_action_rp);

	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = parse->dfa_data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	assert(!fd);

	fd = calloc(1, sizeof(dfa_fun_data_t));
	if (!fd) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	d->module_datas[dfa_module_function.index] = fd;

	return SCF_DFA_OK;
}

static int _dfa_fini_module_function(scf_dfa_t* dfa)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = parse->dfa_data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	if (fd) {
		free(fd);
		fd = NULL;
		d->module_datas[dfa_module_function.index] = NULL;
	}

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_function(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, function, comma,     comma);
	SCF_DFA_GET_MODULE_NODE(dfa, function, vargs,     vargs);

	SCF_DFA_GET_MODULE_NODE(dfa, function, lp,        lp);
	SCF_DFA_GET_MODULE_NODE(dfa, function, rp,        rp);

	SCF_DFA_GET_MODULE_NODE(dfa, type,     _const,    _const);
	SCF_DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	SCF_DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);

	SCF_DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	SCF_DFA_GET_MODULE_NODE(dfa, type,     identity,  identity);
	SCF_DFA_GET_MODULE_NODE(dfa, block,    entry,     block);

	// function start
	scf_dfa_node_add_child(identity,  lp);

	// function args
	scf_dfa_node_add_child(lp,        _const);
	scf_dfa_node_add_child(lp,        base_type);
	scf_dfa_node_add_child(lp,        type_name);
	scf_dfa_node_add_child(lp,        rp);

	scf_dfa_node_add_child(identity,  comma);
	scf_dfa_node_add_child(identity,  rp);

	scf_dfa_node_add_child(base_type, comma);
	scf_dfa_node_add_child(type_name, comma);
	scf_dfa_node_add_child(base_type, rp);
	scf_dfa_node_add_child(type_name, rp);
	scf_dfa_node_add_child(star,      comma);
	scf_dfa_node_add_child(star,      rp);

	scf_dfa_node_add_child(comma,     _const);
	scf_dfa_node_add_child(comma,     base_type);
	scf_dfa_node_add_child(comma,     type_name);
	scf_dfa_node_add_child(comma,     vargs);
	scf_dfa_node_add_child(vargs,     rp);

	// function body
	scf_dfa_node_add_child(rp,        block);

	return 0;
}

scf_dfa_module_t dfa_module_function =
{
	.name        = "function",

	.init_module = _dfa_init_module_function,
	.init_syntax = _dfa_init_syntax_function,

	.fini_module = _dfa_fini_module_function,
};
