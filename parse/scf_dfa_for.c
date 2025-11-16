#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"
#include"scf_stack.h"

extern scf_dfa_module_t dfa_module_for;

typedef struct {
	int              nb_lps;
	int              nb_rps;

	scf_block_t*     parent_block;
	scf_node_t*      parent_node;

	scf_block_t*     _for;

	scf_block_t*     init_exprs;
	scf_block_t*     cond_exprs;
	scf_block_t*     update_exprs;

	int              nb_semicolons;
} dfa_for_data_t;

int _expr_fini_expr(scf_parse_t* parse, dfa_data_t* d, int semi_flag);

static int _for_is_end(scf_dfa_t* dfa, void* word)
{
	return 1;
}

static int _for_action_for(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	scf_ast_t*       ast   = parse->ast;
	dfa_data_t*      d     = data;
	scf_lex_word_t*  w     = words->data[words->size - 1];
	dfa_for_data_t*  fd    = NULL;
	scf_stack_t*     s     = d->module_datas[dfa_module_for.index];
	scf_block_t*    _for   = scf_block_alloc(w);

	if (!_for)
		return -ENOMEM;
	_for->node.type = SCF_OP_FOR;

	int ret;
	if (d->current_node)
		ret = scf_node_add_child(d->current_node, (scf_node_t*) _for);
	else
		ret = scf_node_add_child((scf_node_t*)ast->current_block, (scf_node_t*) _for);
	if (ret < 0) {
		scf_block_free(_for);
		return ret;
	}

	fd = calloc(1, sizeof(dfa_for_data_t));
	if (!fd)
		return -ENOMEM;

	fd->parent_block   = ast->current_block;
	fd->parent_node    = d->current_node;
	fd->_for           = _for;
	d->current_node    = (scf_node_t*) _for;

	ast->current_block = _for;

	scf_stack_push(s, fd);

	return SCF_DFA_NEXT_WORD;
}

static int _for_action_lp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	scf_lex_word_t*   w     = words->data[words->size - 1];
	scf_stack_t*      s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*   fd    = scf_stack_top(s);

	assert(!d->expr);
//	d->expr_local_flag = 1;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_rp"),        SCF_DFA_HOOK_POST);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_semicolon"), SCF_DFA_HOOK_END);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_lp_stat"),   SCF_DFA_HOOK_POST);

	return SCF_DFA_NEXT_WORD;
}

static int _for_action_lp_stat(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_data_t*      d  = data;
	scf_stack_t*     s  = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd = scf_stack_top(s);

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_lp_stat"), SCF_DFA_HOOK_POST);

	fd->nb_lps++;

	return SCF_DFA_NEXT_WORD;
}

static int _for_action_semicolon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	if (!data)
		return SCF_DFA_ERROR;

	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	scf_lex_word_t*  w     = words->data[words->size - 1];
	scf_stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = scf_stack_top(s);
	scf_block_t*     b;
	scf_node_t*      node;
	int i;

	if (fd->nb_semicolons > 1) {
		scf_loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	b = scf_block_alloc(w);
	if (!b)
		return -ENOMEM;

	SCF_XCHG(fd->_for->node.nb_nodes, b->node.nb_nodes);
	SCF_XCHG(fd->_for->node.nodes,    b->node.nodes);

	for (i = 0; i < b->node.nb_nodes; i++) {
		node      = b->node.nodes[i];
		node->parent = (scf_node_t*)b;
	}

	if (0 == fd->nb_semicolons)
		fd->init_exprs = b;
	else
		fd->cond_exprs = b;

	fd->nb_semicolons++;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_semicolon"), SCF_DFA_HOOK_END);
	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_lp_stat"),   SCF_DFA_HOOK_POST);

	return SCF_DFA_SWITCH_TO;
}

static int _for_add_exprs(dfa_for_data_t* fd)
{
	assert(0 == fd->_for->node.nb_nodes);

	int ret = scf_node_add_child((scf_node_t*)fd->_for, (scf_node_t*)fd->init_exprs);
	if (ret < 0)
		goto _init_error;

	ret = scf_node_add_child((scf_node_t*)fd->_for, (scf_node_t*)fd->cond_exprs);
	if (ret < 0)
		goto _cond_error;

	ret = scf_node_add_child((scf_node_t*)fd->_for, (scf_node_t*)fd->update_exprs);
	if (ret < 0)
		goto _update_error;

	fd->init_exprs = NULL;
	fd->cond_exprs = NULL;
	fd->update_exprs = NULL;
	return 0;

_init_error:
	scf_block_free(fd->init_exprs);
_cond_error:
	scf_block_free(fd->cond_exprs);
_update_error:
	scf_block_free(fd->update_exprs);

	fd->init_exprs = NULL;
	fd->cond_exprs = NULL;
	fd->update_exprs = NULL;
	return ret;
}

static int _for_action_rp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	scf_lex_word_t*  w     = words->data[words->size - 1];
	scf_stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = scf_stack_top(s);
	scf_block_t*     b;
	scf_node_t*      node;
	int i;

	fd->nb_rps++;

	if (fd->nb_rps < fd->nb_lps) {

		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_rp"),        SCF_DFA_HOOK_POST);
		SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_lp_stat"),   SCF_DFA_HOOK_POST);

		return SCF_DFA_NEXT_WORD;
	}

	assert (2 == fd->nb_semicolons);

	scf_dfa_del_hook_by_name(&dfa->hooks[SCF_DFA_HOOK_END], "for_semicolon");

	if (d->expr) {
		if (_expr_fini_expr(parse, d, 0) < 0)
			return SCF_DFA_ERROR;
	}

	b = scf_block_alloc(w);
	if (!b)
		return -ENOMEM;

	SCF_XCHG(fd->_for->node.nb_nodes, b->node.nb_nodes);
	SCF_XCHG(fd->_for->node.nodes,    b->node.nodes);

	for (i = 0; i < b->node.nb_nodes; i++) {
		node      = b->node.nodes[i];
		node->parent = (scf_node_t*)b;
	}

	fd->update_exprs = b;

	int ret = _for_add_exprs(fd);
	if (ret < 0)
		return ret;

	SCF_DFA_PUSH_HOOK(scf_dfa_find_node(dfa, "for_end"), SCF_DFA_HOOK_END);

	return SCF_DFA_SWITCH_TO;
}

static int _for_action_end(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse = dfa->priv;
	scf_ast_t*       ast   = parse->ast;
	dfa_data_t*      d     = data;
	scf_stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = scf_stack_pop(s);

	if (3 == fd->_for->node.nb_nodes)
		scf_node_add_child((scf_node_t*)fd->_for, NULL);

	ast->current_block = fd->parent_block;
	d->current_node    = fd->parent_node;

	scf_logi("for: %d, fd: %p, s->size: %d\n", fd->_for->node.w->line, fd, s->size);

	free(fd);
	fd = NULL;

	assert(s->size >= 0);

	return SCF_DFA_OK;
}

static int _dfa_init_module_for(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, for, semicolon, scf_dfa_is_semicolon,  _for_action_semicolon);
	SCF_DFA_MODULE_NODE(dfa, for, end,       _for_is_end,           _for_action_end);

	SCF_DFA_MODULE_NODE(dfa, for, lp,        scf_dfa_is_lp,         _for_action_lp);
	SCF_DFA_MODULE_NODE(dfa, for, lp_stat,   scf_dfa_is_lp,         _for_action_lp_stat);
	SCF_DFA_MODULE_NODE(dfa, for, rp,        scf_dfa_is_rp,         _for_action_rp);

	SCF_DFA_MODULE_NODE(dfa, for, _for,      scf_dfa_is_for,        _for_action_for);

	scf_parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	scf_stack_t*  s     = d->module_datas[dfa_module_for.index];

	assert(!s);

	s = scf_stack_alloc();
	if (!s) {
		scf_loge("\n");
		return SCF_DFA_ERROR;
	}

	d->module_datas[dfa_module_for.index] = s;

	return SCF_DFA_OK;
}

static int _dfa_fini_module_for(scf_dfa_t* dfa)
{
	scf_parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	scf_stack_t*  s     = d->module_datas[dfa_module_for.index];

	if (s) {
		scf_stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_for.index] = NULL;
	}

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_for(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, for,   semicolon, semicolon);
	SCF_DFA_GET_MODULE_NODE(dfa, for,   lp,        lp);
	SCF_DFA_GET_MODULE_NODE(dfa, for,   lp_stat,   lp_stat);
	SCF_DFA_GET_MODULE_NODE(dfa, for,   rp,        rp);
	SCF_DFA_GET_MODULE_NODE(dfa, for,   _for,    _for);
	SCF_DFA_GET_MODULE_NODE(dfa, for,   end,       end);

	SCF_DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	SCF_DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	SCF_DFA_GET_MODULE_NODE(dfa, type,     _const,    _const);
	SCF_DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	SCF_DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);

	// for start
	scf_vector_add(dfa->syntaxes,  _for);

	// dead loop
	scf_dfa_node_add_child(_for,      lp);
	scf_dfa_node_add_child(lp,        semicolon);
	scf_dfa_node_add_child(semicolon, semicolon);
	scf_dfa_node_add_child(semicolon, rp);

	// declare loop var in for
	scf_dfa_node_add_child(lp,        _const);
	scf_dfa_node_add_child(lp,        base_type);
	scf_dfa_node_add_child(lp,        type_name);

	// init, cond, update expr
	scf_dfa_node_add_child(lp,        expr);
	scf_dfa_node_add_child(expr,      semicolon);
	scf_dfa_node_add_child(semicolon, expr);
	scf_dfa_node_add_child(expr,      rp);

	// for body block
	scf_dfa_node_add_child(rp,     block);

	return 0;
}

scf_dfa_module_t dfa_module_for =
{
	.name        = "for",

	.init_module = _dfa_init_module_for,
	.init_syntax = _dfa_init_syntax_for,

	.fini_module = _dfa_fini_module_for,
};
