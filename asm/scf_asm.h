#ifndef SCF_ASM_H
#define SCF_ASM_H

#include"scf_lex.h"
#include"scf_dfa.h"
#include"scf_stack.h"
#include"scf_dwarf.h"
#include"scf_native.h"
#include"scf_elf.h"

typedef struct scf_asm_s   scf_asm_t;
typedef struct dfa_asm_s   dfa_asm_t;

#define ASM_SHNDX_TEXT   1
#define ASM_SHNDX_DATA   2

struct scf_asm_s
{
	scf_lex_t*         lex_list;
	scf_lex_t*         lex;

	scf_dfa_t*         dfa;
	dfa_asm_t*         dfa_data;

	scf_vector_t*      text;    // .text
	scf_vector_t*      data;    // .data
	scf_vector_t*      current; // point to '.text' or '.data' by asm source code

	scf_vector_t*      text_relas;
	scf_vector_t*      data_relas;

	scf_vector_t*      global; // .global
	scf_vector_t*      labels;

	scf_vector_t*      symtab;
	scf_dwarf_t*       debug;
};

struct dfa_asm_s
{
	scf_lex_word_t*    label;
	scf_lex_word_t*    global;
	scf_lex_word_t*    fill;

	scf_OpCode_t*      opcode;
	scf_inst_data_t    operands[4];
	int                i;

	int                type;

	int                align;
	int                org;

	int                n_comma;
	int                n_lp;
	int                n_rp;
};

int scf_asm_dfa_init(scf_asm_t* _asm, const char* arch);

int scf_asm_open  (scf_asm_t** pasm, const char* arch);
int scf_asm_close (scf_asm_t*  _asm);

int scf_asm_file  (scf_asm_t*  _asm, const char* path);
int scf_asm_to_obj(scf_asm_t*  _asm, const char* obj, const char* arch);

int scf_asm_len(scf_vector_t* instructions);

static inline int __inst_data_is_reg(scf_inst_data_t* id)
{
	if (!id->flag && id->base && 0 == id->imm_size)
		return 1;
	return 0;
}

static inline int __inst_data_is_const(scf_inst_data_t* id)
{
	if (!id->flag && id->imm_size > 0)
		return 1;
	return 0;
}

static inline int __lex_word_cmp(const void* v0, const void* v1)
{
	const scf_lex_word_t* w0 = v0;
	const scf_lex_word_t* w1 = v1;

	return scf_string_cmp(w0->text, w1->text);
}

#endif
