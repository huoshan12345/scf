#include"scf_node.h"

scf_label_t* scf_label_alloc(scf_lex_word_t* w)
{
	scf_label_t* l = calloc(1, sizeof(scf_label_t));
	if (!l)
		return NULL;

	l->w = scf_lex_word_clone(w);
	if (!l->w) {
		free(l);
		return NULL;
	}

	l->refs = 1;
	l->type = SCF_LABEL;
	return l;
}

void scf_label_free(scf_label_t* l)
{
	if (l) {
		if (--l->refs > 0)
			return;

		if (l->w) {
			scf_lex_word_free(l->w);
			l->w = NULL;
		}

		l->node = NULL;
		free(l);
	}
}
