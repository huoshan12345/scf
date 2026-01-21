#include"scf_asm.h"
#include"scf_lex_word.h"

extern scf_dfa_module_t  dfa_module_x64;
extern scf_dfa_module_t  dfa_module_naja;

static scf_dfa_module_t* asm_dfa_modules[] =
{
	&dfa_module_x64,
	&dfa_module_naja,
};

static void* dfa_pop_word(scf_dfa_t* dfa)
{
	scf_asm_t* _asm = dfa->priv;

	scf_lex_word_t* w = NULL;
	scf_lex_pop_word(_asm->lex, &w);
	return w;
}

static int dfa_push_word(scf_dfa_t* dfa, void* word)
{
	scf_asm_t* _asm = dfa->priv;

	scf_lex_word_t* w = word;
	scf_lex_push_word(_asm->lex, w);
	return 0;
}

static void dfa_free_word(void* word)
{
	scf_lex_word_t* w = word;
	scf_lex_word_free(w);
}

scf_dfa_ops_t dfa_ops_asm = 
{
	.name      = "asm",

	.pop_word  = dfa_pop_word,
	.push_word = dfa_push_word,
	.free_word = dfa_free_word,
};

int scf_asm_dfa_init(scf_asm_t* _asm, const char* arch)
{
	if (scf_dfa_open(&_asm->dfa, &dfa_ops_asm, _asm) < 0) {
		scf_loge("\n");
		return -1;
	}

	int nb_modules = sizeof(asm_dfa_modules) / sizeof(asm_dfa_modules[0]);

	_asm->dfa_data = calloc(1, sizeof(dfa_asm_t));
	if (!_asm->dfa_data) {
		scf_loge("\n");
		return -1;
	}

	int i;
	for (i = 0; i < nb_modules; i++) {

		scf_dfa_module_t* m = asm_dfa_modules[i];
		if (!m)
			continue;
		m->index = i;

		if (!strcmp(m->name, arch))
		{
			if (m->init_module && m->init_module(_asm->dfa) < 0) {
				scf_loge("init module: %s\n", m->name);
				return -1;
			}

			if (m->init_syntax && m->init_syntax(_asm->dfa) < 0) {
				scf_loge("init syntax: %s\n", m->name);
				return -1;
			}

			return 0;
		}
	}

	return -1;
}
