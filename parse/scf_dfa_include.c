#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_include;

static int _include_action_include(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	scf_lex_word_t*  w      = words->data[words->size - 1];

	return SCF_DFA_NEXT_WORD;
}

static int _include_action_path(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	scf_lex_word_t*  w      = words->data[words->size - 1];
	scf_lex_t*       lex    = parse->lex;
	scf_block_t*     cur    = parse->ast->current_block;

	assert(w->data.s);
	scf_logd("include '%s', line %d\n", w->data.s->data, w->line);

	parse->lex = NULL;
	parse->ast->current_block = parse->ast->root_block;

	int ret = scf_parse_file(parse, w->data.s->data);
	if (ret < 0) {
		scf_loge("parse file '%s' failed, 'include' line: %d\n", w->data.s->data, w->line);
		goto error;
	}

	if (parse->lex != lex && parse->lex->macros) { // copy macros

		if (!lex->macros) {
			lex->macros = scf_vector_clone(parse->lex->macros);

			if (!lex->macros) {
				ret = -ENOMEM;
				goto error;
			}
		} else {
			ret = scf_vector_cat(lex->macros, parse->lex->macros);
			if (ret < 0)
				goto error;
		}

		scf_macro_t* m;
		int i;
		for (i = 0; i < parse->lex->macros->size; i++) {
			m  =        parse->lex->macros->data[i];
			m->refs++;
		}
	}

	ret = SCF_DFA_NEXT_WORD;
error:
	parse->lex = lex;
	parse->ast->current_block = cur;
	return ret;
}

static int _include_action_LF(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	return SCF_DFA_OK;
}

static int _dfa_init_module_include(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, include, include,   scf_dfa_is_include,      _include_action_include);
	SCF_DFA_MODULE_NODE(dfa, include, path,      scf_dfa_is_const_string, _include_action_path);
	SCF_DFA_MODULE_NODE(dfa, include, LF,        scf_dfa_is_LF,           _include_action_LF);

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_include(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, include, include, include);
	SCF_DFA_GET_MODULE_NODE(dfa, include, path,    path);
	SCF_DFA_GET_MODULE_NODE(dfa, include, LF,      LF);

	SCF_DFA_GET_MODULE_NODE(dfa, macro,   hash,    hash);

	scf_dfa_node_add_child(hash,     include);
	scf_dfa_node_add_child(include,  path);
	scf_dfa_node_add_child(path,     LF);

	return 0;
}

scf_dfa_module_t dfa_module_include =
{
	.name        = "include",
	.init_module = _dfa_init_module_include,
	.init_syntax = _dfa_init_syntax_include,
};
