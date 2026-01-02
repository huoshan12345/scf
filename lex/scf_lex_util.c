#include"scf_lex.h"

int _find_key_word(const char* text);

static int __lex_getc(scf_lex_t* lex)
{
	if (lex->fp)
		return fgetc(lex->fp);

	if (lex->text_i < lex->text->len)
		return lex->text->data[lex->text_i++];
	return EOF;
}

scf_char_t* _lex_pop_char(scf_lex_t* lex)
{
	assert(lex);

	scf_char_t* c;

	if (lex->char_list) {
		c              = lex->char_list;
		lex->char_list = c->next;
		return c;
	}

	c = calloc(1, sizeof(scf_char_t));
	if (!c)
		return NULL;

	int ret = __lex_getc(lex);
	if (EOF == ret) {
		c->c = ret;
		return c;
	}

	if (ret < 0x80) {
		c->c   = ret;
		c->len = 1;
		c->utf8[0] = ret;
		return c;
	}

	if (0x6 == (ret >> 5)) {
		c->c   = ret & 0x1f;
		c->len = 2;

	} else if (0xe == (ret >> 4)) {
		c->c   = ret & 0xf;
		c->len = 3;

	} else if (0x1e == (ret >> 3)) {
		c->c   = ret & 0x7;
		c->len = 4;

	} else if (0x3e == (ret >> 2)) {
		c->c   = ret & 0x3;
		c->len = 5;

	} else if (0x7e == (ret >> 1)) {
		c->c   = ret & 0x1;
		c->len = 6;
	} else {
		scf_loge("utf8 first byte wrong %#x, file: %s, line: %d\n", ret, lex->file->data, lex->nb_lines);
		free(c);
		return NULL;
	}

	c->utf8[0] = ret;

	int i;
	for (i = 1; i < c->len; i++) {

		ret = __lex_getc(lex);

		if (0x2  == (ret >> 6)) {
			c->c <<= 6;
			c->c  |= ret & 0x3f;

			c->utf8[i] = ret;
		} else {
			scf_loge("utf8 byte[%d] wrong %#x, file: %s, line: %d\n", i + 1, ret, lex->file->data, lex->nb_lines);
			free(c);
			return NULL;
		}
	}

	return c;
}

void _lex_push_char(scf_lex_t* lex, scf_char_t* c)
{
	assert(lex);
	assert(c);

	c->next        = lex->char_list;
	lex->char_list = c;
}

int _lex_op1_ll1(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0, int type0)
{
	scf_string_t*	s = scf_string_cstr_len(c0->utf8, c0->len);
	scf_lex_word_t*	w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type0);

	lex->pos += c0->len;
	w->text = s;
	s = NULL;

	free(c0);
	c0 = NULL;

	*pword = w;
	return 0;
}

int _lex_op2_ll1(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0,
		int type0, char* chs, int* types, int n)
{
	scf_lex_word_t* w  = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type0);
	scf_string_t*   s  = scf_string_cstr_len(c0->utf8, c0->len);
	scf_char_t*     c1 = _lex_pop_char(lex);

	lex->pos += c0->len;

	int i;
	for (i = 0; i < n; i++) {
		if (chs[i] == c1->c)
			break;
	}

	if (i < n) {
		scf_string_cat_cstr_len(s, c1->utf8, c1->len);

		w->type   = types[i];
		lex->pos += c1->len;

		free(c1);
	} else
		_lex_push_char(lex, c1);
	c1 = NULL;

	w->text = s;
	s = NULL;

	free(c0);
	c0 = NULL;

	*pword = w;
	return 0;
}

int _lex_op3_ll1(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0,
		char ch1_0, char ch1_1, char ch2, int type0, int type1, int type2, int type3)
{
	scf_char_t*     c1 = _lex_pop_char(lex);
	scf_char_t*     c2 = NULL;
	scf_lex_word_t* w  = NULL;
	scf_string_t*   s  = scf_string_cstr_len(c0->utf8, c0->len);

	if (ch1_0 == c1->c) {
		scf_string_cat_cstr_len(s, c1->utf8, c1->len);

		c2 = _lex_pop_char(lex);

		if (ch2 == c2->c) {
			scf_string_cat_cstr_len(s, c2->utf8, c2->len);

			w         = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type0);
			w->text   = s;
			s         = NULL;
			lex->pos += c0->len + c1->len + c2->len;

			free(c2);
			c2 = NULL;
		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;

			w         = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type1);
			w->text   = s;
			s         = NULL;
			lex->pos += c0->len + c1->len;
		}

		free(c1);
		c1 = NULL;

	} else if (ch1_1 == c1->c) {
		scf_string_cat_cstr_len(s, c1->utf8, c1->len);

		w         = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type2);
		w->text   = s;
		s         = NULL;
		lex->pos += c0->len + c1->len;

		free(c1);
		c1 = NULL;
	} else {
		_lex_push_char(lex, c1);
		c1 = NULL;

		w       = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type3);
		w->text = s;
		s       = NULL;
		lex->pos += c0->len;
	}

	free(c0);
	c0 = NULL;

	*pword = w;
	return 0;
}

int _lex_number_base_10(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s)
{
	scf_char_t* c2;
	scf_char_t* c3;
	scf_lex_word_t* w;

	int      dot   = 0;
	int      exp   = 0;
	int      neg   = 0;
	uint64_t value = 0;
	uint64_t num;

	while (1) {
		c2 = _lex_pop_char(lex);

		if (c2->c >= '0' && c2->c <= '9') {
			num    =        c2->c -  '0';
			value *= 10;
			value += num;

		} else if ('.' == c2->c) {

			c3 = _lex_pop_char(lex);

			_lex_push_char(lex, c3);

			if ('.' == c3->c) {
				c3 = NULL;

				_lex_push_char(lex, c2);
				c2 = NULL;
				break;
			}

			c3 = NULL;

			if (++dot > 1) {
				scf_loge("\n");
				return -EINVAL;
			}

		} else if ('e' == c2->c || 'E' == c2->c) {
			exp++;

			if (exp > 1) {
				scf_loge("\n");
				return -EINVAL;
			}

		} else if ('-' == c2->c) {
			neg++;

			if (0 == exp || neg > 1) {
				scf_loge("\n");
				return -EINVAL;
			}

		} else if ('_' == c2->c) {

		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;
			break;
		}

		assert(1 == c2->len);
		scf_string_cat_cstr_len(s, c2->utf8, 1);
		lex->pos++;

		free(c2);
		c2 = NULL;
	}

	if (exp > 0 || dot > 0) {
		w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_DOUBLE);
		w->data.d = atof(s->data);
	} else {
		if (value & ~0xffffffffULL)
			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U64);
		else
			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U32);

		w->data.u64 = value;
	}

	w->text = s;
	s = NULL;

	*pword = w;
	return 0;
}

int _lex_number_base_16(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s)
{
	scf_char_t* c2;
	scf_lex_word_t* w;

	uint64_t value = 0;
	uint64_t value2;

	while (1) {
		c2 = _lex_pop_char(lex);

		if (c2->c >= '0' && c2->c <= '9')
			value2 =        c2->c -  '0';

		else if ('a' <= c2->c && 'f' >= c2->c)
			value2    = c2->c  - 'a' + 10;

		else if ('A' <= c2->c && 'F' >= c2->c)
			value2    = c2->c  - 'A' + 10;

		else if ('_' == c2->c) {
			assert(1 == c2->len);
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			free(c2);
			c2 = NULL;

		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;

			if (value & ~0xffffffffULL)
				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U64);
			else
				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U32);

			w->data.u64 = value;

			w->text = s;
			s = NULL;

			*pword = w;
			return 0;
		}

		value <<= 4;
		value  += value2;

		assert(1 == c2->len);
		scf_string_cat_cstr_len(s, c2->utf8, 1);
		lex->pos++;

		free(c2);
		c2 = NULL;
	}
}

int _lex_number_base_8(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s)
{
	scf_char_t*     c2;
	scf_lex_word_t* w;

	uint64_t value = 0;

	while (1) {
		c2 = _lex_pop_char(lex);

		if (c2->c >= '0' && c2->c <= '7') {
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			value  = (value << 3) + c2->c - '0';

			free(c2);
			c2 = NULL;

		} else if ('8' == c2->c || '9' == c2->c) {
			scf_loge("number must be 0-7 when base 8");

			free(c2);
			c2 = NULL;
			return -1;

		} else if ('_' == c2->c) {
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			free(c2);
			c2 = NULL;

		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;

			if (value & ~0xffffffffULL)
				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U64);
			else
				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U32);
			w->data.u64 = value;

			w->text = s;
			s = NULL;

			*pword = w;
			return 0;
		}
	}
}

int _lex_number_base_2(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s)
{
	scf_char_t* c2;
	scf_lex_word_t* w;

	uint64_t value = 0;

	while (1) {
		c2 = _lex_pop_char(lex);

		if (c2->c >= '0' && c2->c <= '1') {
			assert(1 == c2->len);
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			value  = (value << 1) + c2->c - '0';

			free(c2);
			c2 = NULL;

		} else if (c2->c >= '2' && c2->c <= '9') {
			scf_loge("number must be 0-1 when base 2");

			free(c2);
			c2 = NULL;
			return -1;

		} else if ('_' == c2->c) {
			assert(1   == c2->len);
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			free(c2);
			c2 = NULL;

		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;

			if (value & ~0xffffffffULL)
				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U64);
			else
				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U32);
			w->data.u64 = value;

			w->text = s;
			s = NULL;

			*pword = w;
			return 0;
		}
	}
}

int _lex_double(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s)
{
	scf_lex_word_t* w;
	scf_char_t*     c2;

	while (1) {
		c2 = _lex_pop_char(lex);

		if (c2->c >= '0' && c2->c <= '9') {
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			free(c2);
			c2 = NULL;

		} else if ('.' == c2->c) {
			scf_loge("too many '.' for number in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

			free(c2);
			c2 = NULL;
			return -1;

		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;
			break;
		}
	}

	w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_DOUBLE);
	w->data.d = atof(s->data);

	w->text = s;
	*pword  = w;
	return 0;
}

int _lex_dot(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	scf_char_t*     c1 = _lex_pop_char(lex);
	scf_char_t*     c2 = NULL;
	scf_lex_word_t* w  = NULL;
	scf_lex_word_t* w1 = NULL;
	scf_lex_word_t* w2 = NULL;
	scf_string_t*   s  = scf_string_cstr_len(c0->utf8, c0->len);

	lex->pos += c0->len;

	free(c0);
	c0 = NULL;

	if ('.' == c1->c) {
		scf_string_cat_cstr_len(s, c1->utf8, 1);
		lex->pos++;

		free(c1);
		c1 = NULL;

		c2 = _lex_pop_char(lex);

		if ('.' == c2->c) {
			scf_string_cat_cstr_len(s, c2->utf8, 1);
			lex->pos++;

			free(c2);
			c2 = NULL;

			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_VAR_ARGS);
			w->text = s;
			s = NULL;
		} else {
			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_RANGE);
			w->text = s;
			s = NULL;

			_lex_push_char(lex, c2);
			c2 = NULL;
		}

	} else {
		if (lex->asm_flag
				&& 'a' <= (c1->c | 0x20)
				&& 'z' >= (c1->c | 0x20)) {

			do {
				scf_string_cat_cstr_len(s, c1->utf8, 1);
				lex->pos++;

				free(c1);
				c1 = _lex_pop_char(lex);

				if (!c1) {
					scf_string_free(s);
					return -ENOMEM;
				}
			} while ('a' <= (c1->c | 0x20) && 'z' >= (c1->c | 0x20));

			_lex_push_char(lex, c1);
			c1 = NULL;

			int type = _find_key_word(s->data);
			if (type < 0) {
				scf_loge("unknown asm key word '%s', file: %s, line: %d\n", s->data, lex->file->data, lex->nb_lines);

				scf_string_free(s);
				return -EINVAL;
			}

			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type);
			if (!w) {
				scf_string_free(s);
				return -ENOMEM;
			}

			w->text = s;
			s = NULL;

			*pword = w;
			return 0;
		}

		w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_DOT);
		w->text = s;
		s = NULL;

		_lex_push_char(lex, c1);
		c1 = NULL;

		int ret = __lex_pop_word(lex, &w1);
		if (ret < 0) {
			scf_lex_word_free(w);
			return ret;
		}

		if (SCF_LEX_WORD_CONST_CHAR <= w1->type && w1->type <= SCF_LEX_WORD_CONST_U64) {

			ret = __lex_pop_word(lex, &w2);
			if (ret < 0) {
				scf_lex_word_free(w);
				scf_lex_word_free(w1);
				return ret;
			}

			scf_lex_push_word(lex, w2);

			if (w2->type != SCF_LEX_WORD_ASSIGN && w2->type != SCF_LEX_WORD_DOT) {
				w->type   = SCF_LEX_WORD_CONST_DOUBLE;

				ret = scf_string_cat(w->text, w1->text);
				scf_lex_word_free(w1);
				w1 = NULL;

				if (ret < 0) {
					scf_lex_word_free(w);
					return ret;
				}

				w->data.d = atof(w->text->data);
			}
		}

		if (w1)
			scf_lex_push_word(lex, w1);
	}

	*pword = w;
	return 0;
}
