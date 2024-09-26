#include"scf_lex_word.h"

scf_lex_word_t*	scf_lex_word_alloc(scf_string_t* file, int line, int pos, int type)
{
	if (!file)
		return NULL;

	scf_lex_word_t* w = calloc(1, sizeof(scf_lex_word_t));
	if (!w)
		return NULL;

	w->file = scf_string_clone(file);
	if (!w->file) {
		free(w);
		return NULL;
	}

	w->type = type;
	w->line = line;
	w->pos  = pos;
	return w;
}

scf_lex_word_t*	scf_lex_word_clone(scf_lex_word_t* w)
{
	if (!w)
		return NULL;

	scf_lex_word_t* w1 = calloc(1, sizeof(scf_lex_word_t));
	if (!w1)
		return NULL;

	w1->type = w->type;

	switch (w->type) {
		case SCF_LEX_WORD_CONST_CHAR:
			w1->data.u32 = w->data.u32;
			break;

		case SCF_LEX_WORD_CONST_STRING:

			w1->data.s = scf_string_clone(w->data.s);
			if (!w1->data.s) {
				free(w1);
				return NULL;
			}
			break;

		case SCF_LEX_WORD_CONST_INT:
			w1->data.i = w->data.i;
			break;
		case SCF_LEX_WORD_CONST_FLOAT:
			w1->data.f = w->data.f;
			break;
		case SCF_LEX_WORD_CONST_DOUBLE:
			w1->data.d = w->data.d;
			break;
		case SCF_LEX_WORD_CONST_COMPLEX:
			w1->data.z = w->data.z;
			break;

		case SCF_LEX_WORD_CONST_I64:
			w1->data.i64 = w->data.i64;
			break;
		case SCF_LEX_WORD_CONST_U64:
			w1->data.u64 = w->data.u64;
			break;
		default:
			break;
	};

	if (w->text) {
		w1->text = scf_string_clone(w->text);

		if (!w1->text) {
			scf_lex_word_free(w1);
			return NULL;
		}
	}

	if (w->file) {
		w1->file = scf_string_clone(w->file);

		if (!w1->file) {
			scf_lex_word_free(w1);
			return NULL;
		}
	}

	w1->line = w->line;
	w1->pos  = w->pos;
	return w1;
}

void scf_lex_word_free(scf_lex_word_t* w)
{
	if (w) {
		if (SCF_LEX_WORD_CONST_STRING == w->type)
			scf_string_free(w->data.s);

		if (w->text)
			scf_string_free(w->text);

		if (w->file)
			scf_string_free(w->file);

		free(w);
	}
}

scf_macro_t* scf_macro_alloc(scf_lex_word_t* w)
{
	if (!w)
		return NULL;

	scf_macro_t* m = calloc(1, sizeof(scf_macro_t));
	if (!m)
		return NULL;

	m->w    = w;
	m->refs = 1;
	return m;
}

void scf_macro_free(scf_macro_t* m)
{
	if (m) {
		if (--m->refs > 0)
			return;

		assert(0 == m->refs);

		scf_lex_word_free(m->w);

		if (m->argv) {
			scf_vector_clear(m->argv, ( void (*)(void*) ) scf_lex_word_free);
			scf_vector_free (m->argv);
		}

		scf_slist_clear(m->text_list, scf_lex_word_t, next, scf_lex_word_free);

		free(m);
	}
}
