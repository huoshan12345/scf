#ifndef SCF_INSTRUCTION_H
#define SCF_INSTRUCTION_H

#include"scf_lex_word.h"
#include"scf_core_types.h"

typedef struct scf_instruction_s  scf_instruction_t;

struct scf_register_s
{
	uint32_t		id;
	int				bytes;
	char*			name;

	intptr_t        color;

	scf_vector_t*	dag_nodes;

	uint32_t        updated;
	uint32_t        used;
};
#define SCF_COLOR_CONFLICT(c0, c1)   ((c0) >> 16 == (c1) >> 16 && (c0) & (c1) & 0xffff)

struct scf_OpCode_s
{
	int				type;
	char*			name;
};

typedef struct {
	scf_register_t* base;
	scf_register_t* index;

	int32_t         scale;
	int32_t         disp;
	int32_t         size;
} scf_sib_t;

typedef struct {
	scf_register_t* base;
	scf_register_t* index;
	int             scale;
	int             disp;

	scf_lex_word_t* label;
	uint64_t        imm;
	int             imm_size;

	uint8_t         flag;
} scf_inst_data_t;

struct scf_instruction_s
{
	scf_3ac_code_t*     c;

	scf_OpCode_t*	    OpCode;
	scf_instruction_t*  next; // only for jcc, jmp, call

	scf_inst_data_t     dst;
	scf_inst_data_t     src;
	scf_inst_data_t     srcs[2];

	scf_lex_word_t*     label;  // asm label
	scf_string_t*       bin;    // asm binary data, maybe in .text or .data
	int                 offset; // asm offset,      maybe in .text or .data

	int                 align; // asm .align
	int                 org;   // asm .org

	int                 len;
	uint8_t             code[32];

	int                 flag;  // asm jcc back or front
	int                 nb_used;
};

typedef struct {
	scf_3ac_code_t*     code;        // related 3ac code
	scf_function_t*     func;
	scf_variable_t*     var;
	scf_string_t*       name;

	scf_instruction_t*  inst;
	int                 inst_offset; // byte offset in instruction
	int64_t             text_offset; // byte offset in .text segment
	uint64_t            type;
	int                 addend;
} scf_rela_t;


static inline int scf_inst_data_empty(scf_inst_data_t* id)
{
	return !(id->flag || id->base || id->imm_size);
}

static inline int scf_inst_data_same(scf_inst_data_t* id0, scf_inst_data_t* id1)
{
	// global var, are considered as different.
	if ((id0->flag && !id0->base) || (id1->flag && !id1->base))
		return 0;

	if (id0->scale == id1->scale
			&& id0->disp  == id1->disp
			&& id0->flag  == id1->flag
			&& id0->imm   == id1->imm
			&& id0->imm_size == id1->imm_size) {

		if (id0->base == id1->base
		|| (id0->base && id1->base && SCF_COLOR_CONFLICT(id0->base->color, id1->base->color))) {

			if (id0->index == id1->index
			|| (id0->index && id1->index && SCF_COLOR_CONFLICT(id0->index->color, id1->index->color)))
				return 1;
		}
	}
	return 0;
}

void  scf_rela_free(scf_rela_t* rela);

void  scf_instruction_free (scf_instruction_t* inst);
void  scf_instruction_print(scf_instruction_t* inst);

#endif
