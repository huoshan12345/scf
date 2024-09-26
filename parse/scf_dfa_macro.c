#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_macro;

static int _dfa_init_module_macro(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, macro, hash, scf_dfa_is_hash, scf_dfa_action_next);

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_macro(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa,  macro, hash, hash);

	scf_vector_add(dfa->syntaxes, hash);
	return 0;
}

scf_dfa_module_t dfa_module_macro =
{
	.name        = "macro",
	.init_module = _dfa_init_module_macro,
	.init_syntax = _dfa_init_syntax_macro,
};
