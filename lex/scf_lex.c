#include"scf_lex.h"

static scf_key_word_t  key_words[] =
{
	{"if",        SCF_LEX_WORD_KEY_IF},
	{"else",      SCF_LEX_WORD_KEY_ELSE},

	{"for",       SCF_LEX_WORD_KEY_FOR},
	{"while",     SCF_LEX_WORD_KEY_WHILE},
	{"do",        SCF_LEX_WORD_KEY_DO},

	{"break",     SCF_LEX_WORD_KEY_BREAK},
	{"continue",  SCF_LEX_WORD_KEY_CONTINUE},

	{"switch",    SCF_LEX_WORD_KEY_SWITCH},
	{"case",      SCF_LEX_WORD_KEY_CASE},
	{"default",   SCF_LEX_WORD_KEY_DEFAULT},

	{"return",    SCF_LEX_WORD_KEY_RETURN},

	{"goto",      SCF_LEX_WORD_KEY_GOTO},

	{"sizeof",    SCF_LEX_WORD_KEY_SIZEOF},

	{"create",    SCF_LEX_WORD_KEY_CREATE},

	{"operator",  SCF_LEX_WORD_KEY_OPERATOR},

	{"_",         SCF_LEX_WORD_KEY_UNDERLINE},

	{"char",      SCF_LEX_WORD_KEY_CHAR},

	{"int",       SCF_LEX_WORD_KEY_INT},
	{"float",     SCF_LEX_WORD_KEY_FLOAT},
	{"double",    SCF_LEX_WORD_KEY_DOUBLE},
	{"bit",       SCF_LEX_WORD_KEY_BIT},

	{"int8_t",    SCF_LEX_WORD_KEY_INT8},
	{"int16_t",   SCF_LEX_WORD_KEY_INT16},
	{"int32_t",   SCF_LEX_WORD_KEY_INT32},
	{"int64_t",   SCF_LEX_WORD_KEY_INT64},

	{"uint8_t",   SCF_LEX_WORD_KEY_UINT8},
	{"uint16_t",  SCF_LEX_WORD_KEY_UINT16},
	{"uint32_t",  SCF_LEX_WORD_KEY_UINT32},
	{"uint64_t",  SCF_LEX_WORD_KEY_UINT64},

	{"intptr_t",  SCF_LEX_WORD_KEY_INTPTR},
	{"uintptr_t", SCF_LEX_WORD_KEY_UINTPTR},

	{"void",      SCF_LEX_WORD_KEY_VOID},

	{"va_start",  SCF_LEX_WORD_KEY_VA_START},
	{"va_arg",    SCF_LEX_WORD_KEY_VA_ARG},
	{"va_end",    SCF_LEX_WORD_KEY_VA_END},

	{"container", SCF_LEX_WORD_KEY_CONTAINER},

	{"class",     SCF_LEX_WORD_KEY_CLASS},

	{"const",     SCF_LEX_WORD_KEY_CONST},
	{"static",    SCF_LEX_WORD_KEY_STATIC},
	{"extern",    SCF_LEX_WORD_KEY_EXTERN},
	{"inline",    SCF_LEX_WORD_KEY_INLINE},

	{"async",     SCF_LEX_WORD_KEY_ASYNC},

	{"include",   SCF_LEX_WORD_KEY_INCLUDE},

	{"union",     SCF_LEX_WORD_KEY_UNION},
	{"struct",    SCF_LEX_WORD_KEY_STRUCT},
};

static scf_escape_char_t  escape_chars[] =
{
	{'n', '\n'},
	{'r', '\r'},
	{'t', '\t'},
	{'0', '\0'},
};

static int _find_key_word(const char* text)
{
	int i;
	for (i = 0; i < sizeof(key_words) / sizeof(key_words[0]); i++) {

		if (!strcmp(key_words[i].text, text))
			return key_words[i].type;
	}

	return -1;
}

static int _find_escape_char(const int c)
{
	int i;
	for (i = 0; i < sizeof(escape_chars) / sizeof(escape_chars[0]); i++) {

		if (escape_chars[i].origin == c)
			return escape_chars[i].escape; // return the escape char
	}

	// if it isn't in the escape array, return the original char
	return c;
}

int	scf_lex_open(scf_lex_t** plex, const char* path)
{
	if (!plex || !path)
		return -EINVAL;

	scf_lex_t* lex = calloc(1, sizeof(scf_lex_t));
	if (!lex)
		return -ENOMEM;

	scf_list_init(&lex->word_list_head);

	lex->char_list_head = NULL;

	lex->fp = fopen(path, "r");
	if (!lex->fp) {
		char cwd[4096];
		getcwd(cwd, 4095);
		scf_loge("path: %s, errno: %d, pwd: %s\n", path, errno, cwd);

		free(lex);
		return -1;
	}

	lex->file = scf_string_cstr(path);
	if (!lex->file) {
		fclose(lex->fp);
		free(lex);
		return -ENOMEM;
	}

	lex->nb_lines = 1;

	*plex = lex;
	return 0;
}

int scf_lex_close(scf_lex_t* lex)
{
	if (lex) {
		scf_list_clear(&lex->word_list_head, scf_lex_word_t, list, scf_lex_word_free);

		free(lex);
	}
	return 0;
}

int scf_lex_push_word(scf_lex_t* lex, scf_lex_word_t* word)
{
	if (lex && word)
		scf_list_add_front(&lex->word_list_head, &word->list);
	return 0;
}

static int _lex_plus(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	scf_char_t* c1 = _lex_pop_char(lex);

	if ('+' == c1->c) {
		scf_char_t* c2 = _lex_pop_char(lex);

		if ('+' == c2->c)
			scf_logw("+++ may cause a BUG in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

		_lex_push_char(lex, c2);
		c2 = NULL;

		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_INC);
		w->text = scf_string_cstr("++");
		lex->pos += 2;

		*pword = w;

		free(c1);
		c1 = NULL;

	} else if ('=' == c1->c) {
		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_ADD_ASSIGN);
		w->text = scf_string_cstr("+=");
		lex->pos += 2;

		*pword = w;
		free(c1);
		c1 = NULL;
	} else {
		_lex_push_char(lex, c1);
		c1 = NULL;

		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_PLUS);
		w->text = scf_string_cstr("+");
		lex->pos++;

		*pword = w;
	}

	free(c0);
	c0 = NULL;
	return 0;
}

static int _lex_minus(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	scf_char_t* c1 = _lex_pop_char(lex);

	if ('-' == c1->c) {
		scf_char_t* c2 = _lex_pop_char(lex);

		if ('-' == c2->c)
			scf_logw("--- may cause a BUG in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

		_lex_push_char(lex, c2);
		c2 = NULL;

		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_DEC);
		w->text = scf_string_cstr("--");
		lex->pos += 2;

		*pword = w;
		free(c1);
		c1 = NULL;

	} else if ('>' == c1->c) {
		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_ARROW);
		w->text = scf_string_cstr("->");
		lex->pos += 2;

		*pword = w;
		free(c1);
		c1 = NULL;

	} else if ('=' == c1->c) {
		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_SUB_ASSIGN);
		w->text = scf_string_cstr("-=");
		lex->pos += 2;

		*pword = w;
		free(c1);
		c1 = NULL;
	} else {
		_lex_push_char(lex, c1);
		c1 = NULL;

		scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_MINUS);
		w->text = scf_string_cstr("-");
		lex->pos++;

		*pword = w;
	}

	free(c0);
	c0 = NULL;
	return 0;
}

static int _lex_number(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	lex->pos++;

	if ('0' == c0->c) {
		scf_char_t* c1 = _lex_pop_char(lex);

		if ('x' == c1->c || 'X' == c1->c) {
			// base 16
			scf_string_t* s = scf_string_cstr_len(c0->utf8, 1);

			scf_string_cat_cstr_len(s, c1->utf8, 1);
			lex->pos += 2;

			free(c1);
			free(c0);
			c1 = NULL;
			c0 = NULL;

			return _lex_number_base_16(lex, pword, s);

		} else if ('.' == c1->c) {
			// double
			scf_string_t* s = scf_string_cstr_len(c0->utf8, 1);

			scf_string_cat_cstr_len(s, c1->utf8, 1);
			lex->pos += 2;

			free(c1);
			free(c0);
			c1 = NULL;
			c0 = NULL;

			while (1) {
				scf_char_t* c2 = _lex_pop_char(lex);

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

					scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_DOUBLE);
					w->data.d = atof(s->data);

					w->text = s;
					s = NULL;

					*pword = w;
					return 0;
				}
			}
		} else {
			scf_string_t* s = scf_string_cstr_len(c0->utf8, 1);

			if (c1->c < '0' || c1->c > '9') {
				// is 0
				_lex_push_char(lex, c1);
				c1 = NULL;

				scf_lex_word_t* w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_INT);
				w->data.u64 = atoi(s->data);

				w->text = s;
				s = NULL;
				lex->pos++;

				*pword = w;
				return 0;
			}

			// base 8
			_lex_push_char(lex, c1);
			c1 = NULL;

			lex->pos++;

			return _lex_number_base_8(lex, pword, s);
		}
	} else {
		// base 10
		scf_string_t* s  = scf_string_cstr_len(c0->utf8, 1);

		uint64_t value   = c0->c - '0';
		int      nb_dots = 0;

		free(c0);
		c0 = NULL;

		while (1) {
			scf_char_t* c1 = _lex_pop_char(lex);

			if (c1->c >= '0' && c1->c <= '9') {

				value = value * 10 + c1->c - '0';

				scf_string_cat_cstr_len(s, c1->utf8, 1);
				lex->pos++;

				free(c1);
				c1 = NULL;

			} else if ('.' == c1->c) {
				scf_string_cat_cstr_len(s, c1->utf8, 1);
				lex->pos++;

				free(c1);
				c1 = NULL;

				if (++nb_dots > 1) {
					scf_loge("too many '.' for number in file: %s, line: %d\n", lex->file->data, lex->nb_lines);
					return -1;
				}
			} else {
				_lex_push_char(lex, c1);
				c1 = NULL;

				scf_lex_word_t* w = NULL;

				if (nb_dots > 0) {
					w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_DOUBLE);
					w->data.d = atof(s->data);
				} else {
					if (value & ~0xffffffffULL)
						w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_I64);
					else
						w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_INT);
					w->data.i64 = value;
				}

				w->text = s;
				s = NULL;

				*pword = w;
				return 0;
			}
		}
	}
}

static int _lex_identity(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	scf_string_t* s = scf_string_cstr_len(c0->utf8, c0->len);

	lex->pos += c0->len;
	free(c0);
	c0 = NULL;

	while (1) {
		scf_char_t* c1 = _lex_pop_char(lex);

		if ('_' == c1->c
				|| ('a'    <= c1->c && 'z'    >= c1->c)
				|| ('A'    <= c1->c && 'Z'    >= c1->c)
				|| ('0'    <= c1->c && '9'    >= c1->c)
				|| (0x4e00 <= c1->c && 0x9fa5 >= c1->c)) {

			scf_string_cat_cstr_len(s, c1->utf8, c1->len);
			lex->pos += c1->len;

			free(c1);
			c1 = NULL;
		} else {
			_lex_push_char(lex, c1);
			c1 = NULL;

			scf_lex_word_t* w = NULL;

			if (!strcmp(s->data, "NULL")) {

				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U64);
				if (w)
					w->data.u64 = 0;

			} else if (!strcmp(s->data, "__LINE__")) {

				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_U64);
				if (w)
					w->data.u64 = lex->nb_lines;

			} else if (!strcmp(s->data, "__func__")) {

				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_STRING);

			} else {
				int type = _find_key_word(s->data);

				if (-1 == type)
					w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_ID);
				else
					w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type);
			}

			if (w)
				w->text = s;
			else
				scf_string_free(s);
			s = NULL;

			*pword = w;
			return 0;
		}
	}

	return -1;
}

static int _lex_char(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	scf_lex_word_t* w = NULL;
	scf_string_t*   s = scf_string_cstr_len(c0->utf8, 1);

	scf_char_t* c1 = _lex_pop_char(lex);

	if ('\\' == c1->c) {
		scf_char_t* c2 = _lex_pop_char(lex);
		scf_char_t* c3 = _lex_pop_char(lex);

		if ('\'' == c3->c) {
			scf_string_cat_cstr_len(s, c1->utf8, 1);
			scf_string_cat_cstr_len(s, c2->utf8, c2->len);
			scf_string_cat_cstr_len(s, c3->utf8, 1);

			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_CHAR);
			w->data.i64 = _find_escape_char(c2->c);
			lex->pos += 3 + c2->len;

			free(c3);
			free(c2);
			c3 = NULL;
			c2 = NULL;
		} else {
			scf_loge("const char lost 2nd ' in file: %s, line: %d\n", lex->file->data, lex->nb_lines);
			return -1;
		}

	} else {
		scf_char_t* c2 = _lex_pop_char(lex);

		if ('\'' == c2->c) {
			scf_string_cat_cstr_len(s, c1->utf8, c1->len);
			scf_string_cat_cstr_len(s, c2->utf8, 1);

			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_CHAR);
			w->data.i64 = c1->c;
			lex->pos += 2 + c1->len;

			free(c2);
			c2 = NULL;
		} else {
			scf_loge("const char lost 2nd ' in file: %s, line: %d\n", lex->file->data, lex->nb_lines);
			return -1;
		}
	}

	w->text = s;
	s = NULL;
	*pword = w;

	free(c1);
	free(c0);
	c1 = NULL;
	c0 = NULL;
	return 0;
}

static int _lex_string(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0)
{
	scf_lex_word_t* w = NULL;
	scf_string_t*   s = scf_string_cstr_len(c0->utf8, 1);
	scf_string_t*   d = scf_string_alloc();

	while (1) {
		scf_char_t* c1 = _lex_pop_char(lex);

		if ('\"' == c1->c) {
			scf_string_cat_cstr_len(s, c1->utf8, 1);

			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_STRING);
			w->data.s = d;
			w->text   = s;
			d = NULL;
			s = NULL;

			lex->pos++;
			*pword = w;

			free(c1);
			c1 = NULL;
			return 0;

		} else if ('\\' == c1->c) {
			scf_char_t* c2 = _lex_pop_char(lex);

			int ch2 = _find_escape_char(c2->c);

			scf_string_cat_cstr_len(s, c1->utf8, 1);
			scf_string_cat_cstr_len(s, c2->utf8, c2->len);
			lex->pos += 1 + c2->len;

			free(c2);
			free(c1);
			c2 = NULL;
			c1 = NULL;

			if (0 == ch2) {
				while (1) {
					c1 = _lex_pop_char(lex);

					if ('0' <= c1->c && c1->c <= '7') {
						ch2 <<= 3;
						ch2  += c1->c - '0';

						scf_string_cat_cstr_len(s, c1->utf8, 1);
						lex->pos++;

						free(c1);
						c1 = NULL;
					} else {
						_lex_push_char(lex, c1);
						break;
					}
				}

				scf_string_cat_cstr_len(d, (char*)&ch2, 1);
			} else
				scf_string_cat_cstr_len(d, (char*)&ch2, 1);

		} else if (EOF == c1->c) {
			scf_loge("const string lost 2nd \" in file: %s, line: %d\n", lex->file->data, lex->nb_lines);
			return -1;

		} else {
			scf_string_cat_cstr_len(s, c1->utf8, c1->len);
			scf_string_cat_cstr_len(d, c1->utf8, c1->len);
			lex->pos += c1->len;

			free(c1);
			c1 = NULL;
		}
	}
}

int scf_lex_pop_word(scf_lex_t* lex, scf_lex_word_t** pword)
{
	if (!lex || !lex->fp || !pword)
		return -EINVAL;

	scf_list_t*		l = NULL;
	scf_char_t*     c = NULL;
	scf_lex_word_t*	w = NULL;

	if (!scf_list_empty(&lex->word_list_head)) {
		l = scf_list_head(&lex->word_list_head);
		w = scf_list_data(l, scf_lex_word_t, list);

		scf_list_del(&w->list);
		*pword = w;
		return 0;
	}

	c = _lex_pop_char(lex);

	while  ('\n' == c->c || '\r' == c->c || '\t' == c->c || ' ' == c->c) {
		if ('\n' == c->c) {
			lex->nb_lines++;
			lex->pos = 0;
		} else
			lex->pos++;

		free(c);
		c = _lex_pop_char(lex);
	}

	if (EOF == c->c) {
		w       = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_EOF);
		w->text = scf_string_cstr("eof");
		*pword  = w;

		free(c);
		c = NULL;
		return 0;
	}

	if ('+' == c->c)
		return _lex_plus(lex, pword, c);

	if ('-' == c->c)
		return _lex_minus(lex, pword, c);

	if ('*' == c->c) {
		char c1 = '=';
		int  t1 = SCF_LEX_WORD_MUL_ASSIGN;

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_STAR, &c1, &t1, 1);
	}

	if ('/' == c->c) {

		scf_char_t* c2 = _lex_pop_char(lex);

		if ('/' == c2->c) {
			free(c);
			free(c2);
			c  = NULL;
			c2 = NULL;

			while (1) {
				c2 = _lex_pop_char(lex);

				if (EOF == c2->c) {
					_lex_push_char(lex, c2);
					break;
				}

				int tmp = c2->c;
				free(c2);
				c2 = NULL;

				if ('\n' == tmp) {
					lex->nb_lines++;
					lex->pos = 0;
					break;
				}
			}

			return scf_lex_pop_word(lex, pword);

		} else if ('*' == c2->c) {
			free(c);
			free(c2);
			c  = NULL;
			c2 = NULL;

			while (1) {
				c2 = _lex_pop_char(lex);

				if (EOF == c2->c) {
					_lex_push_char(lex, c2);
					break;
				}

				int tmp = c2->c;
				free(c2);
				c2 = NULL;

				if ('\n' == tmp) {
					lex->nb_lines++;
					lex->pos = 0;
					continue;
				}

				if ('*' == tmp) {
					c2  = _lex_pop_char(lex);

					if ('/' == c2->c) {
						free(c2);
						c2 = NULL;
						break;
					}

					_lex_push_char(lex, c2);
					c2 = NULL;
				}
			}

			return scf_lex_pop_word(lex, pword);

		} else {
			_lex_push_char(lex, c2);
			c2 = NULL;
		}

		char c1 = '=';
		int  t1 = SCF_LEX_WORD_DIV_ASSIGN;

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_DIV, &c1, &t1, 1);
	}

	if ('%' == c->c) {
		char c1 = '=';
		int  t1 = SCF_LEX_WORD_MOD_ASSIGN;

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_MOD, &c1, &t1, 1);
	}

	switch (c->c) {
		case '~':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_BIT_NOT);
			break;

		case '(':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_LP);
			break;
		case ')':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_RP);
			break;
		case '[':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_LS);
			break;
		case ']':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_RS);
			break;
		case '{':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_LB);
			break;
		case '}':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_RB);
			break;

		case ',':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_COMMA);
			break;
		case ';':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_SEMICOLON);
			break;
		case ':':
			return _lex_op1_ll1(lex, pword, c, SCF_LEX_WORD_COLON);
			break;

		default:
			break;
	};

	if ('&' == c->c) {
		char chs[2]   = {'&', '='};
		int  types[2] = {SCF_LEX_WORD_LOGIC_AND, SCF_LEX_WORD_BIT_AND_ASSIGN};

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_BIT_AND, chs, types, 2);
	}

	if ('|' == c->c) {
		char chs[2]   = {'|', '='};
		int  types[2] = {SCF_LEX_WORD_LOGIC_OR, SCF_LEX_WORD_BIT_OR_ASSIGN};

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_BIT_OR, chs, types, 2);
	}

	if ('!' == c->c) {
		char c1 = '=';
		int  t1 = SCF_LEX_WORD_NE;

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_LOGIC_NOT, &c1, &t1, 1);
	}

	if ('=' == c->c) {
		char c1 = '=';
		int  t1 = SCF_LEX_WORD_EQ;

		return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_ASSIGN, &c1, &t1, 1);
	}

	if ('>' == c->c)
		return _lex_op3_ll1(lex, pword, c, '>', '=', '=',
				SCF_LEX_WORD_SHR_ASSIGN, SCF_LEX_WORD_SHR, SCF_LEX_WORD_GE, SCF_LEX_WORD_GT);

	if ('<' == c->c)
		return _lex_op3_ll1(lex, pword, c, '<', '=', '=',
				SCF_LEX_WORD_SHL_ASSIGN, SCF_LEX_WORD_SHL, SCF_LEX_WORD_LE, SCF_LEX_WORD_LT);

	if ('.' == c->c)
		return _lex_dot(lex, pword, c);

	if ('\'' == c->c)
		return _lex_char(lex, pword, c);

	if ('\"' == c->c) {
		scf_lex_word_t* w0 = NULL;

		int ret = _lex_string(lex, &w0, c);
		if (ret < 0) {
			*pword = NULL;
			return ret;
		}

		while (1) {
			scf_lex_word_t* w1 = NULL;

			ret = scf_lex_pop_word(lex, &w1);
			if (ret < 0) {
				scf_lex_word_free(w0);
				*pword = NULL;
				return ret;
			}

			if (SCF_LEX_WORD_CONST_STRING != w1->type) {
				scf_lex_push_word(lex, w1);
				break;
			}

			if (scf_string_cat(w0->text, w1->text) < 0
			 || scf_string_cat(w0->data.s, w1->data.s) < 0) {

				scf_lex_word_free(w1);
				scf_lex_word_free(w0);
				*pword = NULL;
				return -1;
			}

			scf_lex_word_free(w1);
		}

		scf_logd("w0: %s\n", w0->data.s->data);
		*pword = w0;
		return 0;
	}

	if ('0' <= c->c && '9' >= c->c)
		return _lex_number(lex, pword, c);

	if ('_' == c->c
			|| ('a'    <= c->c && 'z'    >= c->c)
			|| ('A'    <= c->c && 'Z'    >= c->c)
			|| (0x4e00 <= c->c && 0x9fa5 >= c->c)) { // support China chars

		return _lex_identity(lex, pword, c);
	}

	scf_loge("unknown char: %c, utf: %#x, in file: %s, line: %d\n", c->c, c->c, lex->file->data, lex->nb_lines);
	return -1;
}
