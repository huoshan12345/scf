#include"scf_lex.h"

static scf_key_word_t  key_words[] =
{
	{"if",        SCF_LEX_WORD_KEY_IF},
	{"else",      SCF_LEX_WORD_KEY_ELSE},
	{"endif",     SCF_LEX_WORD_KEY_ENDIF},

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

	{"new",       SCF_LEX_WORD_KEY_NEW},

	{"operator",  SCF_LEX_WORD_KEY_OPERATOR},

	{"_",         SCF_LEX_WORD_KEY_UNDERLINE},

	{"char",      SCF_LEX_WORD_KEY_CHAR},

	{"int",       SCF_LEX_WORD_KEY_INT},
	{"float",     SCF_LEX_WORD_KEY_FLOAT},
	{"double",    SCF_LEX_WORD_KEY_DOUBLE},
	{"bit",       SCF_LEX_WORD_KEY_BIT},
	{"bit2_t",    SCF_LEX_WORD_KEY_BIT2},
	{"bit3_t",    SCF_LEX_WORD_KEY_BIT3},
	{"bit4_t",    SCF_LEX_WORD_KEY_BIT4},

	{"int8_t",    SCF_LEX_WORD_KEY_INT8},
	{"int1_t",    SCF_LEX_WORD_KEY_INT1},
	{"int2_t",    SCF_LEX_WORD_KEY_INT2},
	{"int3_t",    SCF_LEX_WORD_KEY_INT3},
	{"int4_t",    SCF_LEX_WORD_KEY_INT4},
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
	{"define",    SCF_LEX_WORD_KEY_DEFINE},

	{"enum",      SCF_LEX_WORD_KEY_ENUM},
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

	lex->fp = fopen(path, "r");
	if (!lex->fp) {

		char cwd[4096];
		getcwd(cwd, 4095);
		scf_loge("open file '%s' failed, errno: %d, default path dir: %s\n", path, errno, cwd);

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
		scf_slist_clear(lex->char_list, scf_char_t,     next, free);
		scf_slist_clear(lex->word_list, scf_lex_word_t, next, scf_lex_word_free);

		if (lex->macros) {
			scf_vector_clear(lex->macros, ( void (*)(void*) )scf_macro_free);
			scf_vector_free (lex->macros);
		}

		scf_string_free(lex->file);

		fclose(lex->fp);
		free(lex);
	}
	return 0;
}

void scf_lex_push_word(scf_lex_t* lex, scf_lex_word_t* w)
{
	if (lex && w) {
		w->next        = lex->word_list;
		lex->word_list = w;
	}
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
	scf_lex_word_t* w  = NULL;
	scf_string_t*   s  = NULL;
	scf_char_t*     c1 = NULL;
	scf_char_t*     c2 = NULL;

	int ret = 0;

	if ('0' == c0->c) {
		s = scf_string_cstr_len(c0->utf8, 1);
		lex->pos++;

		free(c0);
		c0 = NULL;

		c1 = _lex_pop_char(lex);

		scf_string_cat_cstr_len(s, c1->utf8, 1);
		lex->pos++;

		switch (c1->c) {
			case 'x':
			case 'X': // base 16
				ret = _lex_number_base_16(lex, pword, s);
				break;

			case 'b': // base 2
				ret = _lex_number_base_2(lex, pword, s);
				break;

			case '.': // double
				ret = _lex_double(lex, pword, s);
				break;

			default:
				lex->pos--;
				s->data[--s->len] = '\0';

				_lex_push_char(lex, c1);

				if (c1->c < '0' || c1->c > '9') { // is 0
					w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_INT);
					w->data.u64 = atoi(s->data);

					w->text = s;
					*pword  = w;
					return 0;
				}

				// base 8
				c1 = NULL;
				return _lex_number_base_8(lex, pword, s);
				break;
		};

		free(c1);
		c1 = NULL;
		return ret;
	}

	// base 10
	s = scf_string_alloc();

	_lex_push_char(lex, c0);
	c0 = NULL;

	return _lex_number_base_10(lex, pword, s);
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

			} else if (!strcmp(s->data, "__FILE__")) {

				w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_STRING);
				if (w)
					w->data.s = scf_string_clone(lex->file);

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
	scf_string_t*   s  = NULL;
	scf_lex_word_t* w  = NULL;
	scf_char_t*     c1 = _lex_pop_char(lex);
	scf_char_t*     c2 = _lex_pop_char(lex);
	scf_char_t*     c3;

	if ('\\' == c1->c) {
		c3 = _lex_pop_char(lex);

		if ('\'' == c3->c) {
			s = scf_string_cstr_len(c0->utf8, 1);

			scf_string_cat_cstr_len(s, c1->utf8, 1);
			scf_string_cat_cstr_len(s, c2->utf8, c2->len);
			scf_string_cat_cstr_len(s, c3->utf8, 1);

			w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_CHAR);

			w->data.i64 = _find_escape_char(c2->c);
			lex->pos   += c2->len + 3;
		} else
			scf_loge("const char lost 2nd ' in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

		free(c3);
		c3 = NULL;

	} else if ('\'' == c2->c) {
		s = scf_string_cstr_len(c0->utf8, 1);

		scf_string_cat_cstr_len(s, c1->utf8, c1->len);
		scf_string_cat_cstr_len(s, c2->utf8, 1);

		w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_CONST_CHAR);

		w->data.i64 = c1->c;
		lex->pos   += c1->len + 2;
	} else
		scf_loge("const char lost 2nd ' in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

	free(c0);
	free(c1);
	free(c2);

	if (!w)
		return -1;

	w->text = s;
	*pword  = w;
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

static int _lex_macro(scf_lex_t* lex)
{
	scf_char_t*  h  =  NULL;
	scf_char_t** pp = &h;
	scf_char_t*  c;
	scf_char_t*  c2;

	while (1) {
		c = _lex_pop_char(lex);
		if (!c) {
			scf_slist_clear(h, scf_char_t, next, free);
			return -1;
		}

		*pp =  c;
		pp  = &c->next;

		if (EOF == c->c)
			break;

		if ('\n' == c->c) {
			c->flag = SCF_UTF8_LF;
			break;
		}

		if ('\\' == c->c) {

			c2 = _lex_pop_char(lex);
			if (!c2) {
				scf_slist_clear(h, scf_char_t, next, free);
				return -1;
			}

			*pp =  c2;
			pp  = &c2->next;

			if (EOF == c2->c)
				break;

			if ('\n' == c2->c)
				c2->flag = 0;
		}
	}

	*pp = lex->char_list;
	lex->char_list = h;
	return 0;
}

static void _lex_drop_to(scf_lex_t* lex, int c0, int c1)
{
	scf_char_t* c = NULL;

	while (1) {
		if (!c)
			c = _lex_pop_char(lex);

		if (EOF == c->c) {
			_lex_push_char(lex, c);
			break;
		}

		int tmp = c->c;
		free(c);
		c = NULL;

		if ('\n' == tmp) {
			lex->nb_lines++;
			lex->pos = 0;
		}

		if (c0 == tmp) {
			if (c1 < 0)
				break;

			c = _lex_pop_char(lex);

			if (c1 == c->c) {
				free(c);
				c = NULL;
				break;
			}
		}
	}
}

int __lex_pop_word(scf_lex_t* lex, scf_lex_word_t** pword)
{
	scf_list_t*		l = NULL;
	scf_char_t*     c = NULL;
	scf_lex_word_t*	w = NULL;

	if (lex->word_list) {
		w              = lex->word_list;
		lex->word_list = w->next;

		*pword = w;
		return 0;
	}

	c = _lex_pop_char(lex);

	while ('\n' == c->c
			|| '\r' == c->c || '\t' == c->c
			|| ' '  == c->c || '\\' == c->c) {

		if ('\n' == c->c) {
			lex->nb_lines++;
			lex->pos = 0;

			if (SCF_UTF8_LF == c->flag) {
				w       = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_LF);
				w->text = scf_string_cstr("LF");
				*pword  = w;

				free(c);
				c = NULL;
				return 0;
			}
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
		char c1 = '=';
		int  t1 = SCF_LEX_WORD_DIV_ASSIGN;

		scf_char_t* c2 = _lex_pop_char(lex);

		switch (c2->c) {
			case '/':
				_lex_drop_to(lex, '\n', -1);
				break;

			case '*':
				_lex_drop_to(lex, '*', '/');
				break;

			default:
				_lex_push_char(lex, c2);

				return _lex_op2_ll1(lex, pword, c, SCF_LEX_WORD_DIV, &c1, &t1, 1);
				break;
		};

		free(c);
		free(c2);
		c  = NULL;
		c2 = NULL;
		return __lex_pop_word(lex, pword);
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
		scf_lex_word_t* w1 = NULL;

		int ret = _lex_string(lex, &w0, c);
		if (ret < 0) {
			*pword = NULL;
			return ret;
		}

		while (1) {
			ret = __lex_pop_word(lex, &w1);
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

	if ('#' == c->c) {
		w = scf_lex_word_alloc(lex->file, lex->nb_lines, lex->pos, SCF_LEX_WORD_HASH);

		free(c);
		c = _lex_pop_char(lex);

		if ('#' == c->c) {
			w->type = SCF_LEX_WORD_HASH2;
			w->text = scf_string_cstr("##");

			lex->pos += 2;
			free(c);
		} else {
			w->text = scf_string_cstr("#");

			lex->pos++;
			_lex_push_char(lex, c);
		}

		c = NULL;
		*pword = w;
		return _lex_macro(lex);
	}

	scf_loge("unknown char: %c, utf: %#x, in file: %s, line: %d\n", c->c, c->c, lex->file->data, lex->nb_lines);
	return -1;
}

static int __parse_macro_argv(scf_lex_t* lex, scf_macro_t* m)
{
	scf_lex_word_t* w = NULL;
	scf_lex_word_t* w2;

	int comma = 0;
	int id    = 0;

	while (1) {
		int ret = __lex_pop_word(lex, &w);
		if (ret < 0)
			return ret;

		if (!comma) {
			if (SCF_LEX_WORD_RP == w->type) {
				scf_lex_word_free(w);
				break;
			}

			if (SCF_LEX_WORD_COMMA == w->type) {
				scf_lex_word_free(w);
				w = NULL;

				if (!id) {
					scf_loge("an identity is needed before ',' in file: %s, line: %d\n", w->file->data, w->line);
					return -1;
				}

				id    = 0;
				comma = 1;
				continue;
			}
		}

		if (!scf_lex_is_identity(w)) {
			scf_loge("macro arg '%s' should be an identity, file: %s, line: %d\n", w->text->data, w->file->data, w->line);
			scf_lex_word_free(w);
			return -1;
		}

		if (id) {
			scf_loge("',' is needed before macro arg '%s', file: %s, line: %d\n", w->text->data, w->file->data, w->line);
			scf_lex_word_free(w);
			return -1;
		}

		int i;
		for (i = 0; i < m->argv->size; i++) {
			w2 =        m->argv->data[i];

			if (!scf_string_cmp(w2->text, w->text)) {
				scf_loge("macro has same args '%s', file: %s, line: %d\n", w->text->data, w->file->data, w->line);
				scf_lex_word_free(w);
				return -1;
			}
		}

		scf_logw("macro '%s' arg: %s\n", m->w->text->data, w->text->data);

		ret = scf_vector_add(m->argv, w);
		if (ret < 0) {
			scf_lex_word_free(w);
			return ret;
		}
		w = NULL;

		id    = 1;
		comma = 0;
	}

	return 0;
}

static int __parse_macro_define(scf_lex_t* lex)
{
	scf_lex_word_t** pp;
	scf_lex_word_t*  w = NULL;
	scf_macro_t*     m;
	scf_macro_t*     m0;

	int ret = __lex_pop_word(lex, &w);
	if (ret < 0)
		return ret;

	if (!scf_lex_is_identity(w)) {
		scf_loge("macro '%s' should be an identity, file: %s, line: %d\n", w->text->data, w->file->data, w->line);
		scf_lex_word_free(w);
		return -1;
	}

	m = scf_macro_alloc(w);
	if (!m) {
		scf_lex_word_free(w);
		return -ENOMEM;
	}

	w   = NULL;
	ret = __lex_pop_word(lex, &w);
	if (ret < 0) {
		scf_macro_free(m);
		return ret;
	}

	pp = &m->text_list;

	if (SCF_LEX_WORD_LP == w->type) {
		scf_lex_word_free(w);
		w = NULL;

		m->argv = scf_vector_alloc();
		if (!m->argv) {
			scf_macro_free(m);
			return -ENOMEM;
		}

		ret = __parse_macro_argv(lex, m);
		if (ret < 0) {
			scf_macro_free(m);
			return ret;
		}
	} else {
		*pp =  w;
		pp  = &w->next;
		w   = NULL;
	}

	while (1) {
		ret = __lex_pop_word(lex, &w);
		if (ret < 0) {
			scf_macro_free(m);
			return ret;
		}

		if (SCF_LEX_WORD_LF == w->type) {
			scf_lex_word_free(w);
			w = NULL;
			break;
		}

		*pp =  w;
		pp  = &w->next;
		w   = NULL;
	}

	if (!lex->macros) {
		lex->macros = scf_vector_alloc();
		if (!lex->macros)
			return -ENOMEM;

	} else {
		int i;
		for (i = lex->macros->size - 1; i >= 0; i--) {
			m0 = lex->macros->data[i];

			if (!scf_string_cmp(m->w->text, m0->w->text)) {
				scf_logw("macro '%s' defined before in file: %s, line: %d\n",
						m0->w->text->data, m0->w->file->data, m0->w->line);
				break;
			}
		}
	}

	ret = scf_vector_add(lex->macros, m);
	if (ret < 0) {
		scf_macro_free(m);
		return ret;
	}

	return 0;
}

static int __fill_macro_argv(scf_lex_t* lex, scf_macro_t* m, scf_lex_word_t* use, scf_vector_t* argv)
{
	scf_lex_word_t** pp;
	scf_lex_word_t*  w = NULL;

	int ret = __lex_pop_word(lex, &w);
	if (ret < 0)
		return ret;

	if (SCF_LEX_WORD_LP != w->type) {
		scf_loge("macro '%s' needs args, file: %s, line: %d\n", m->w->text->data, w->file->data, w->line);
		scf_lex_word_free(w);
		return -1;
	}

	scf_lex_word_free(w);
	w = NULL;

	int n_lps = 0;
	int n_rps = 0;
	int i;

	pp = NULL;

	while (1) {
		ret = __lex_pop_word(lex, &w);
		if (ret < 0)
			return ret;

		if (SCF_LEX_WORD_COMMA == w->type) {
			if (!pp) {
				scf_loge("unexpected ',' in macro '%s', file: %s, line: %d\n", m->w->text->data, w->file->data, w->line);

				scf_lex_word_free(w);
				ret = -1;
				goto error;
			}

			scf_lex_word_free(w);
			w  = NULL;

			if (n_rps == n_lps)
				pp = NULL;
			continue;

		} else if (SCF_LEX_WORD_LP == w->type)
			n_lps++;
		else if (SCF_LEX_WORD_RP == w->type) {
			n_rps++;

			if (n_rps > n_lps) {
				scf_lex_word_free(w);
				w = NULL;
				break;
			}
		}

		w->next = NULL;

		if (pp)
			*pp = w;
		else {
			ret = scf_vector_add(argv, w);
			if (ret < 0) {
				scf_lex_word_free(w);
				goto error;
			}
		}

		pp = &w->next;
		w  = NULL;
	}

	if (m->argv->size != argv->size) {
		scf_loge("macro '%s' needs %d args, but in fact give %d args,  file: %s, line: %d\n",
				m->w->text->data, m->argv->size, argv->size, use->file->data, use->line);
		ret = -1;
		goto error;
	}

	return 0;

error:
	for (i = 0; i < argv->size; i++) {
		w  =        argv->data[i];
		scf_slist_clear(w, scf_lex_word_t, next, scf_lex_word_free);
	}

	argv->size = 0;
	return ret;
}

static int __convert_str(scf_lex_word_t* h)
{
	scf_lex_word_t* w;
	scf_string_t*   s;

	if (h->type != SCF_LEX_WORD_CONST_STRING) {
		h->type  = SCF_LEX_WORD_CONST_STRING;

		h->data.s = scf_string_clone(h->text);
		if (!h->data.s)
			return -ENOMEM;
	}

	while ( h->next) {
		w = h->next;

		if (SCF_LEX_WORD_CONST_STRING != w->type)
			s = w->text;
		else
			s = w->data.s;

		int ret = scf_string_cat(h->data.s, s);
		if (ret < 0)
			return ret;

		ret = scf_string_cat(h->text, w->text);
		if (ret < 0)
			return ret;

		h->next = w->next;

		scf_lex_word_free(w);
		w = NULL;
	}

	scf_logw("h: '%s', file: %s, line: %d\n", h->data.s->data, h->file->data, h->line);
	return 0;
}

static scf_macro_t* __find_macro(scf_lex_t* lex, scf_lex_word_t* w)
{
	if (!lex->macros)
		return NULL;

	scf_macro_t* m;
	int i;

	for (i = lex->macros->size - 1; i >= 0; i--) {
		m  = lex->macros->data[i];

		if (!scf_string_cmp(m->w->text, w->text))
			return m;
	}

	return NULL;
}

static int __use_macro(scf_lex_t* lex, scf_macro_t* m, scf_lex_word_t* use)
{
	scf_lex_word_t** pp;
	scf_lex_word_t*  p;
	scf_lex_word_t*  h;
	scf_lex_word_t*  w;
	scf_lex_word_t*  prev;
	scf_vector_t*    argv = NULL;

	if (m->argv) {
		argv = scf_vector_alloc();
		if (!argv)
			return -ENOMEM;

		int ret = __fill_macro_argv(lex, m, use, argv);
		if (ret < 0) {
			scf_vector_free(argv);
			return ret;
		}
	}

	h  = NULL;
	pp = &h;

	int ret  = 0;
	int hash = 0;
	int i;

	for (p = m->text_list; p; p = p->next) {

		if (SCF_LEX_WORD_HASH == p->type) {
			hash = 1;
			continue;
		}

		scf_logd("p: '%s', line: %d, hash: %d\n", p->text->data, p->line, hash);

		if (m->argv) {
			assert(argv);
			assert(argv->size >= m->argv->size);

			for (i = 0; i < m->argv->size; i++) {
				w  =        m->argv->data[i];

				if (!scf_string_cmp(w->text, p->text))
					break;
			}

			if (i < m->argv->size) {
				scf_lex_word_t** tmp = pp;

				for (w = argv->data[i]; w; w = w->next) {

					*pp = scf_lex_word_clone(w);
					if (!*pp) {
						ret = -ENOMEM;
						goto error;
					}

					if (!strcmp((*pp)->text->data, "__LINE__"))
						(*pp)->data.u64 = use->line;

					pp = &(*pp)->next;
				}

				if (1 == hash) {
					ret = __convert_str(*tmp);
					if (ret < 0)
						goto error;

					pp = &(*tmp)->next;
				}

				hash = 0;
				continue;
			}
		}

		*pp = scf_lex_word_clone(p);
		if (!*pp) {
			ret = -ENOMEM;
			goto error;
		}

		if (!strcmp((*pp)->text->data, "__LINE__"))
			(*pp)->data.u64 = use->line;

		pp = &(*pp)->next;

		hash = 0;
	}

error:
	if (argv) {
		for (i = 0; i < argv->size; i++) {
			w  =        argv->data[i];

			if (w)
				scf_slist_clear(w, scf_lex_word_t, next, scf_lex_word_free);
		}

		scf_vector_free(argv);
		argv = NULL;
	}

	if (ret < 0) {
		scf_slist_clear(h, scf_lex_word_t, next, scf_lex_word_free);
		return ret;
	}

#if 0
	w = h;
	while (w) {
		scf_logi("---------- %s, line: %d\n", w->text->data, w->line);
		w = w->next;
	}
#endif
	*pp            = lex->word_list;
	lex->word_list = h;
	return 0;
}

static int __use_hash2(scf_lex_t* lex, scf_lex_word_t* prev)
{
	scf_lex_word_t* after = NULL;

	int ret = __lex_pop_word(lex, &after);
	if (ret < 0)
		return ret;

	switch (after->type) {

		case SCF_LEX_WORD_ID:
			ret = scf_string_cat(prev->text, after->text);
			break;

		default:
			ret = -1;
			scf_loge("needs identity after '##', file: %s, line: %d\n", after->file->data, after->line);
			break;
	};

	scf_lex_word_free(after);
	return ret;
}

int __lex_use_macro(scf_lex_t* lex, scf_lex_word_t** pp)
{
	scf_lex_word_t* w1 = NULL;
	scf_lex_word_t* w  = *pp;
	scf_macro_t*    m;

	*pp = NULL;

	while (SCF_LEX_WORD_ID == w->type) {

		m = __find_macro(lex, w);
		if (m) {
			int ret = __use_macro(lex, m, w);

			scf_lex_word_free(w);
			w = NULL;
			if (ret < 0)
				return ret;

			ret = __lex_pop_word(lex, &w);
			if (ret < 0)
				return ret;
			continue;
		}

		int ret = __lex_pop_word(lex, &w1);
		if (ret < 0) {
			scf_lex_word_free(w);
			return ret;
		}

		if (SCF_LEX_WORD_HASH2 != w1->type) {
			scf_lex_push_word(lex, w1);
			break;
		}

		scf_lex_word_free(w1);
		w1 = NULL;

		ret = __use_hash2(lex, w);
		if (ret < 0) {
			scf_lex_word_free(w);
			return ret;
		}
	}

	*pp = w;
	return 0;
}

int scf_lex_pop_word(scf_lex_t* lex, scf_lex_word_t** pword)
{
	if (!lex || !lex->fp || !pword)
		return -EINVAL;

	scf_lex_word_t* w  = NULL;
	scf_lex_word_t* w1 = NULL;

	int ret = __lex_pop_word(lex, &w);
	if (ret < 0)
		return ret;

	// parse macro
	while (SCF_LEX_WORD_HASH == w->type) {

		ret = __lex_pop_word(lex, &w1);
		if (ret < 0) {
			scf_lex_word_free(w);
			return ret;
		}

		switch (w1->type) {
			case SCF_LEX_WORD_KEY_INCLUDE:
			case SCF_LEX_WORD_KEY_IF:
			case SCF_LEX_WORD_KEY_ELSE:
			case SCF_LEX_WORD_KEY_ENDIF:

				scf_lex_push_word(lex, w1);
				*pword = w;
				return 0;
				break;

			case SCF_LEX_WORD_KEY_DEFINE:
				ret = __parse_macro_define(lex);
				break;
			default:
				scf_loge("unknown macro '%s', file: %s, line: %d\n", w1->text->data, w1->file->data, w1->line);
				ret = -1;
				break;
		};

		scf_lex_word_free(w);
		scf_lex_word_free(w1);
		w  = NULL;
		w1 = NULL;

		if (ret < 0)
			return ret;

		ret = __lex_pop_word(lex, &w);
		if (ret < 0)
			return ret;
	}

	// use macro to pre-process source code
	ret = __lex_use_macro(lex, &w);
	if (ret < 0)
		return ret;

	*pword = w;
	return 0;
}
