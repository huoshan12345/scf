#ifndef SCF_LEX_H
#define SCF_LEX_H

#include"scf_lex_word.h"

typedef struct scf_char_s  scf_char_t;
typedef struct scf_lex_s   scf_lex_t;

typedef struct {
	char*   text;
	int     type;
} scf_key_word_t;

typedef struct {
	int     origin;
	int     escape;
} scf_escape_char_t;

#define SCF_UTF8_MAX 6
#define SCF_UTF8_LF  1
struct scf_char_s
{
	scf_char_t*     next;
	int             c;

	int             len;
	uint8_t         utf8[SCF_UTF8_MAX];
	uint8_t         flag;
};

struct scf_lex_s
{
	scf_lex_t*      next;

	scf_lex_word_t* word_list;
	scf_char_t*     char_list;

	scf_vector_t*   macros;

	FILE*           fp; // file pointer to the code

	scf_string_t*   file; // original code file name
	int             nb_lines;
	int             pos;
};


scf_char_t*  _lex_pop_char (scf_lex_t* lex);
void         _lex_push_char(scf_lex_t* lex, scf_char_t* c);


int	 scf_lex_open (scf_lex_t** plex, const char* path);
int  scf_lex_close(scf_lex_t*   lex);

void scf_lex_push_word(scf_lex_t* lex, scf_lex_word_t*   word);
int  scf_lex_pop_word (scf_lex_t* lex, scf_lex_word_t** pword);


int _lex_number_base_16(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s);
int _lex_number_base_10(scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s);
int _lex_number_base_8 (scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s);
int _lex_number_base_2 (scf_lex_t* lex, scf_lex_word_t** pword, scf_string_t* s);

int _lex_dot    (scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0);

int _lex_op1_ll1(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0, int type0);
int _lex_op2_ll1(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0, int type0, char* chs, int* types, int n);
int _lex_op3_ll1(scf_lex_t* lex, scf_lex_word_t** pword, scf_char_t* c0, char ch1_0, char ch1_1, char ch2, int type0, int type1, int type2, int type3);

#endif
