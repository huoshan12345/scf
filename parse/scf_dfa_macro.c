#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_parse.h"

extern scf_dfa_module_t dfa_module_macro;

static inline int _macro_action_if(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_lex_word_t* w  = words->data[words->size - 1];
	scf_lex_word_t* w1 = dfa->ops->pop_word(dfa);
	scf_lex_word_t* w2;
	scf_lex_word_t* w3;

	if (!w1)
		return SCF_DFA_ERROR;

	if (!scf_lex_is_const_integer(w1)) {
		scf_loge("the condition after '#if' must be a const integer, file: %s, line: %d\n", w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	int flag = w1->data.u32;

	scf_lex_word_free(w1);
	w1 = NULL;

	while (1) {
		w1 = dfa->ops->pop_word(dfa);
		if (!w1)
			return SCF_DFA_ERROR;

		int type = w1->type;

		scf_lex_word_free(w1);
		w1 = NULL;

		if (SCF_LEX_WORD_EOF == type) {
			scf_loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
			return SCF_DFA_ERROR;
		}

		if (SCF_LEX_WORD_LF == type)
			break;
	}

	scf_lex_word_t* h = NULL;

	int n_if = 1;

	while (1) {
		w1 = dfa->ops->pop_word(dfa);
		if (!w1)
			goto error;
		w1->next = NULL;

		if (SCF_LEX_WORD_EOF == w1->type) {
			scf_loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
			scf_lex_word_free(w1);
			goto error;
		}

		if (SCF_LEX_WORD_HASH == w1->type) {
			w2 = dfa->ops->pop_word(dfa);
			if (!w2) {
				scf_lex_word_free(w1);
				goto error;
			}
			w2->next = NULL;

			scf_logd("'#%s' file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);

			if (SCF_LEX_WORD_EOF == w2->type) {
				scf_loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
				scf_lex_word_free(w2);
				scf_lex_word_free(w1);
				goto error;
			}

			if (n_if < 1) {
				scf_loge("extra '#%s' without an '#if' in file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);
				scf_lex_word_free(w2);
				scf_lex_word_free(w1);
				goto error;
			}

			if (SCF_LEX_WORD_KEY_ELSE == w2->type || SCF_LEX_WORD_KEY_ENDIF == w2->type) {
				w3 = dfa->ops->pop_word(dfa);
				if (!w3) {
					scf_lex_word_free(w2);
					scf_lex_word_free(w1);
					goto error;
				}
				w3->next = NULL;

				if (SCF_LEX_WORD_LF != w3->type) {
					scf_loge("'\\n' NOT found after '#%s' in file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);
					scf_lex_word_free(w3);
					scf_lex_word_free(w2);
					scf_lex_word_free(w1);
					goto error;
				}

				if (SCF_LEX_WORD_KEY_ELSE == w2->type) {
					if (1 == n_if) {
						flag = !flag;
						scf_lex_word_free(w3);
						scf_lex_word_free(w2);
						scf_lex_word_free(w1);
						continue;
					}
				} else {
					if (0 == --n_if) {
						scf_lex_word_free(w3);
						scf_lex_word_free(w2);
						scf_lex_word_free(w1);
						break;
					}
				}

				if (flag)
					w2->next = w3;
				else
					scf_lex_word_free(w3);
				w3 = NULL;

			} else if (SCF_LEX_WORD_KEY_IF == w2->type) {
				n_if++;
			}

			if (flag)
				w1->next = w2;
			else
				scf_lex_word_free(w2);
			w2 = NULL;
		}

		if (flag) {
			while (w1) {
				w2 = w1->next;
				w1->next = h;
				h  = w1;
				w1 = w2;
			}
		} else
			scf_lex_word_free(w1);
		w1 = NULL;
	}

	while (h) {
		w = h;
		scf_logd("'%s' file: %s, line: %d\n", w->text->data, w->file->data, w->line);
		h = w->next;
		dfa->ops->push_word(dfa, w);
	}

	return SCF_DFA_OK;

error:
	while (h) {
		w = h;
		h = w->next;
		scf_lex_word_free(w);
	}
	return SCF_DFA_ERROR;
}

static int _dfa_init_module_macro(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, macro, hash, scf_dfa_is_hash, scf_dfa_action_next);
	SCF_DFA_MODULE_NODE(dfa, macro, _if,  scf_dfa_is_if,   _macro_action_if);

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_macro(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa,  macro, hash, hash);
	SCF_DFA_GET_MODULE_NODE(dfa,  macro, _if,  _if);

	scf_vector_add(dfa->syntaxes, hash);

	scf_dfa_node_add_child(hash,  _if);
	return 0;
}

scf_dfa_module_t dfa_module_macro =
{
	.name        = "macro",
	.init_module = _dfa_init_module_macro,
	.init_syntax = _dfa_init_syntax_macro,
};
