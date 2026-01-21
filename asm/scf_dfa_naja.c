#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_asm.h"
#include"scf_risc_opcode.h"
#include"scf_naja_reg.c"
#include"scf_naja_inst.c"

extern scf_dfa_module_t  dfa_module_naja;


static int _naja_is_opcode(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return !!risc_find_OpCode_by_name(w->text->data);
}

static int _naja_is_reg(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return !!naja_find_register(w->text->data);
}

static int _naja_action_text(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	_asm->current = _asm->text;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_data(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	_asm->current = _asm->data;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_global(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->global = w;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_fill(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->fill = w;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_type(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->type = w->type;

	scf_logi("w: '%s'\n", w->text->data);

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_number(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*         _asm = dfa->priv;
	dfa_asm_t*          d   = data;
	scf_lex_word_t*     w   = words->data[words->size - 1];
	scf_lex_word_t*     w2;
	scf_instruction_t*  inst;

	w2 = dfa->ops->pop_word(dfa);
	dfa->ops->push_word(dfa, w2);

	if (d->opcode) {
		switch (w2->type) {
			case SCF_LEX_WORD_LP:
				d->operands[d->i].disp = w->data.u64;
				d->operands[d->i].flag = 1;
				break;

			case SCF_LEX_WORD_RP:
				d->operands[d->i].scale = w->data.i;
				d->operands[d->i].flag  = 1;
				break;

			case SCF_LEX_WORD_ID:
			case SCF_LEX_WORD_COMMA:
			case SCF_LEX_WORD_LF:
				if (d->n_lp != d->n_rp) {
					scf_loge("'(' not equal to ')' in file: %s, line: %d\n", w->file->data, w->line);
					return SCF_DFA_ERROR;
				}

				d->operands[d->i].imm      = w->data.u64;
				d->operands[d->i].imm_size = 8;
				d->operands[d->i].flag     = 0;

				if (SCF_RISC_CALL == d->opcode->type
						|| (SCF_RISC_JZ <= d->opcode->type && SCF_RISC_JMP >= d->opcode->type)) {
					int flag = 0x1;

					if (SCF_LEX_WORD_ID == w2->type) {
						switch (w2->text->data[0]) {
							case 'b':
								break;
							case 'f':
								flag = 0x2;
								break;
							default:
								scf_loge("the char followed a number label MUST be 'b' or 'f', but real '%c' in file: %s, line: %d\n",
										w2->text->data[0], w->file->data, w->line);
								return SCF_DFA_ERROR;
								break;
						};

						w2 = dfa->ops->pop_word(dfa);
						dfa->ops->free_word(w2);
						w2 = NULL;
					}

					d->operands[d->i].imm <<= 8;
					d->operands[d->i].imm  |= flag;
				}

				d->i++;
				break;
			default:
				scf_loge("number '%s', w2: %s, file: %s, line: %d\n", w->text->data, w2->text->data, w->file->data, w->line);
				return -1;
				break;
		};

	} else if (d->fill) {
		switch (w2->type) {
			case SCF_LEX_WORD_COMMA:
			case SCF_LEX_WORD_LF:
				if (d->n_lp != d->n_rp) {
					scf_loge("'(' not equal to ')' in file: %s, line: %d\n", w->file->data, w->line);
					return SCF_DFA_ERROR;
				}

				d->operands[d->i].imm  = w->data.u64;
				d->operands[d->i].flag = 0;
				d->i++;
				break;
			default:
				scf_loge("number '%s', w2: %s, file: %s, line: %d\n", w->text->data, w2->text->data, w->file->data, w->line);
				return -1;
				break;
		};

	} else if (SCF_LEX_WORD_LF == w2->type || SCF_LEX_WORD_COMMA == w2->type) {
		int n;
		switch (d->type) {
			case SCF_LEX_WORD_ASM_BYTE:
				n = 1;
				break;
			case SCF_LEX_WORD_ASM_WORD:
				n = 2;
				break;

			case SCF_LEX_WORD_ASM_LONG:
				n = 4;
				break;
			case SCF_LEX_WORD_ASM_QUAD:
				n = 8;
				break;
			default:
				scf_loge("const number '%s' MUST be '.byte', '.word', '.long' or '.quad' type, file: %s, line: %d\n",
						w->text->data, w->file->data, w->line);
				return -1;
				break;
		};

		inst = calloc(1, sizeof(scf_instruction_t));
		if (!inst)
			return -ENOMEM;

		memcpy(inst->code, (uint8_t*)&w->data, n);
		inst->len = n;

		RISC_INST_ADD_CHECK(_asm->current, inst);
		if (d->label) {
			inst->label = d->label;
			d   ->label = NULL;
		}
	}

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_str(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*         _asm = dfa->priv;
	dfa_asm_t*          d   = data;
	scf_lex_word_t*     w   = words->data[words->size - 1];
	scf_instruction_t*  inst;

	inst = calloc(1, sizeof(scf_instruction_t));
	if (!inst)
		return -ENOMEM;

	int n;
	switch (d->type) {
		case SCF_LEX_WORD_ASM_ASCIZ:
			n = w->data.s->len + 1;
			break;

		case SCF_LEX_WORD_ASM_ASCII:
			n = w->data.s->len;
			break;
		default:
			scf_loge("const string '%s' MUST be '.asciz' or '.ascii' type, file: %s, line: %d\n",
					w->text->data, w->file->data, w->line);

			scf_instruction_free(inst);
			return -1;
			break;
	};

	if (n <= sizeof(inst->code)) {
		memcpy(inst->code, w->data.s->data, n);
		inst->len = n;
	} else {
		inst->bin = scf_string_cstr_len(w->data.s->data, n);
		if (!inst->bin) {
			scf_instruction_free(inst);
			return -ENOMEM;
		}
	}

	RISC_INST_ADD_CHECK(_asm->current, inst);
	if (d->label) {
		inst->label = d->label;
		d   ->label = NULL;
	}

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_identity(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];
	scf_lex_word_t*  w2;

	if (d->opcode) {
		d->operands[d->i].label = w;
		d->operands[d->i].flag  = 1;

		w = dfa->ops->pop_word(dfa);
		dfa->ops->push_word(dfa, w);

		if (SCF_LEX_WORD_LF == w->type || SCF_LEX_WORD_COMMA == w->type) {
			if (d->n_lp != d->n_rp) {
				scf_loge("'(' not equal to ')' in file: %s, line: %d\n", w->file->data, w->line);
				return SCF_DFA_ERROR;
			}

			d->i++;
		}

	} else if (d->global) {
		if (scf_vector_find_cmp(_asm->global, w, __lex_word_cmp))
			return SCF_DFA_NEXT_WORD;

		w2 = scf_lex_word_clone(w);
		if (!w2)
			return SCF_DFA_ERROR;

		int ret = scf_vector_add(_asm->global, w2);
		if (ret < 0) {
			scf_lex_word_free(w2);
			return ret;
		}

	} else {
		w2 = dfa->ops->pop_word(dfa);
		dfa->ops->push_word(dfa, w2);

		if (SCF_LEX_WORD_LF == w2->type || SCF_LEX_WORD_COMMA == w2->type) {

			if (SCF_LEX_WORD_ASM_QUAD != d->type) {
				scf_loge("the type of label '%s' MUST be .quad, file: %s, line: %d\n",
						w->text->data, w->file->data, w->line);
				return -1;
			}

			scf_instruction_t* inst = calloc(1, sizeof(scf_instruction_t));
			if (!inst)
				return -ENOMEM;
			inst->len = 8;
			RISC_INST_ADD_CHECK(_asm->current, inst);

			if (d->label) {
				inst->label = d->label;
				d   ->label = NULL;
			}

			scf_rela_t* rela = calloc(1, sizeof(scf_rela_t));
			if (!rela)
				return -ENOMEM;

			rela->name = scf_string_clone(w->text);
			if (!rela->name) {
				scf_rela_free(rela);
				return -ENOMEM;
			}

			rela->inst = inst;
			rela->type = R_X86_64_64;

			int ret;
			if (_asm->current == _asm->text)
				ret = scf_vector_add(_asm->text_relas, rela);
			else
				ret = scf_vector_add(_asm->data_relas, rela);

			if (ret < 0) {
				scf_rela_free(rela);
				return ret;
			}
		}
	}

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_opcode(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->opcode = (scf_OpCode_t*)risc_find_OpCode_by_name(w->text->data);
	if (!d->opcode) {
		scf_loge("opcode '%s' NOT found\n", w->text->data);
		return SCF_DFA_ERROR;
	}

	d->i       = 0;
	d->n_comma = 0;
	d->n_lp    = 0;
	d->n_rp    = 0;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_reg(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];
	scf_register_t*  r   = naja_find_register(w->text->data);

	if (!r) {
		scf_loge("register '%s' NOT found\n", w->text->data);
		return SCF_DFA_ERROR;
	}

	switch (d->n_comma) {
		case 0:
			d->operands[d->i].base = r;
			break;
		case 1:
			d->operands[d->i].index = r;
			break;
		default:
			scf_loge("\n");
			return SCF_DFA_ERROR;
			break;
	};

	if (d->n_lp == d->n_rp)
		d->i++;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_lp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_asm_t*  d = data;

	d->n_lp++;
	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_rp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_asm_t*  d = data;

	if (++d->n_rp == d->n_lp) {
		d->operands[d->i].flag = 1;
		d->i++;
	}

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	if (d->n_lp != d->n_rp)
		d->n_comma++;

	return SCF_DFA_NEXT_WORD;
}

static int _naja_action_colon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];
	scf_lex_word_t*  w0;

	if (words->size < 2) {
		scf_loge("no label before '%s', file: %s, line: %d\n", w->text->data, w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	w = words->data[words->size - 2];

	if (SCF_LEX_WORD_ID == w->type) {
		w0 = scf_vector_find_cmp(_asm->labels, w, __lex_word_cmp);
		if (w0) {
			scf_loge("repeated label '%s' in file: %s, line: %d, first in file: %s, line: %d\n",
					w->text->data, w->file->data, w->line, w0->file->data, w0->line);
			return SCF_DFA_ERROR;
		}

		w0 = scf_lex_word_clone(w);
		if (!w0)
			return -ENOMEM;

		int ret = scf_vector_add(_asm->labels, w0);
		if (ret < 0) {
			scf_lex_word_free(w0);
			return ret;
		}
	}

	scf_logi("w->text->data: %s, w->data.u32: %d, w->type: %d\n", w->text->data, w->data.u32, w->type);
	d->label = scf_lex_word_clone(w);
	if (!d->label)
		return -ENOMEM;

	return SCF_DFA_NEXT_WORD;
}

static int __asm_naja_ADD(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 3) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id0    = &d->operands[0];
	scf_inst_data_t*    id1    = &d->operands[1];
	scf_inst_data_t*    id2    = &d->operands[2];
	scf_rela_t*         rela   = NULL;

	if (!__inst_data_is_reg(id0) || !__inst_data_is_reg(id1)) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	if (__inst_data_is_reg(id2)) {
		inst = naja_inst_ADD_G(NULL, id0->base, id1->base, id2->base);
		RISC_INST_ADD_CHECK(_asm->current, inst);

	} else if (__inst_data_is_const(id2)) {
		inst = naja_inst_ADD_IMM(NULL, NULL, id0->base, id1->base, id2->imm);
		RISC_INST_ADD_CHECK(_asm->current, inst);
	} else {
		inst = naja_inst_ADD_IMM(NULL, NULL, id0->base, id1->base, 0);

		RISC_INST_ADD_CHECK(_asm->current, inst);
		RISC_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id2->label->text);
		rela->type = R_AARCH64_ADD_ABS_LO12_NC;
	}

	*__inst = inst;
	return 0;
}

static int __asm_naja_RET(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	scf_instruction_t*  inst;

	inst = naja_inst_RET(NULL);
	RISC_INST_ADD_CHECK(_asm->current, inst);

	*__inst = inst;
	return 0;
}

static int __asm_naja_JMP(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 1) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id   = &d->operands[0];
	scf_rela_t*         rela = NULL;

	if (__inst_data_is_reg(id)) {
		inst = naja_inst_BR(NULL, id->base);
		RISC_INST_ADD_CHECK(_asm->current, inst);
	} else {
		uint32_t disp = 0;

		if (!id->label)
			disp = id->imm;

		inst = __naja_inst_JMP(NULL, disp);
		RISC_INST_ADD_CHECK(_asm->current, inst);

		if (id->label) {
			rela = calloc(1, sizeof(scf_rela_t));
			if (!rela)
				return -ENOMEM;

			RISC_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id->label->text);
			rela->type = R_AARCH64_CALL26;
		}
	}

	*__inst = inst;
	return 0;
}

static int __asm_naja_CALL(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 1) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id   = &d->operands[0];
	scf_rela_t*         rela = NULL;

	if (__inst_data_is_reg(id)) {
		inst = naja_inst_BLR(NULL, id->base);
		RISC_INST_ADD_CHECK(_asm->current, inst);
	} else {
		uint32_t disp = 0;

		if (!id->label)
			disp = id->imm;

		inst = __naja_inst_BL(NULL, disp);
		RISC_INST_ADD_CHECK(_asm->current, inst);

		if (id->label) {
			rela = calloc(1, sizeof(scf_rela_t));
			if (!rela)
				return -ENOMEM;

			RISC_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id->label->text);
			rela->type = R_AARCH64_CALL26;
		}
	}

	*__inst = inst;
	return 0;
}

static int __asm_naja_ADRP(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 2) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id0    = &d->operands[0];
	scf_inst_data_t*    id1    = &d->operands[1];
	scf_rela_t*         rela   = NULL;

	if (!__inst_data_is_reg(id0) || !id1->label) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	inst = __naja_inst_ADRP(NULL, id0->base);
	RISC_INST_ADD_CHECK(_asm->current, inst);
	RISC_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id1->label->text);
	rela->type = R_AARCH64_ADR_PREL_PG_HI21;

	*__inst = inst;
	return 0;
}

static int __asm_naja_MOV(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 2) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id0 = &d->operands[0];
	scf_inst_data_t*    id1 = &d->operands[1];

	if (!__inst_data_is_reg(id0)) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	if (__inst_data_is_reg(id1)) {
		inst = naja_inst_MOV_G(NULL, id0->base, id1->base);

	} else if (__inst_data_is_const(id1))
		inst = naja_inst_MOV_IMM(NULL, id0->base, id1->imm);
	else {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}
	RISC_INST_ADD_CHECK(_asm->current, inst);

	*__inst = inst;
	return 0;
}

static int __asm_naja_PUSH(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 1) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id = &d->operands[0];

	if (!__inst_data_is_reg(id)) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	inst = naja_inst_PUSH(NULL, id->base);
	RISC_INST_ADD_CHECK(_asm->current, inst);

	*__inst = inst;
	return 0;
}

static int __asm_naja_POP(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	if (d->i < 1) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	scf_instruction_t*  inst;
	scf_inst_data_t*    id = &d->operands[0];

	if (!__inst_data_is_reg(id)) {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return -EINVAL;
	}

	inst = naja_inst_POP(NULL, id->base);
	RISC_INST_ADD_CHECK(_asm->current, inst);

	*__inst = inst;
	return 0;
}

static int _naja_action_LF(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*         _asm  = dfa->priv;
	dfa_asm_t*          d    = data;
	scf_lex_word_t*     w    = words->data[words->size - 1];
	scf_instruction_t*  inst = NULL;

	if (d->opcode) {
		int ret = 0;
		switch (d->opcode->type)
		{
			case SCF_RISC_PUSH:
				ret = __asm_naja_PUSH(&inst, _asm, d, w);
				break;
			case SCF_RISC_POP:
				ret = __asm_naja_POP(&inst, _asm, d, w);
				break;

			case SCF_RISC_MOV:
				ret = __asm_naja_MOV(&inst, _asm, d, w);
				break;

			case SCF_RISC_ADRP:
				ret = __asm_naja_ADRP(&inst, _asm, d, w);
				break;

			case SCF_RISC_ADD:
				ret = __asm_naja_ADD(&inst, _asm, d, w);
				break;

			case SCF_RISC_JMP:
				ret = __asm_naja_JMP(&inst, _asm, d, w);
				break;

			case SCF_RISC_CALL:
				ret = __asm_naja_CALL(&inst, _asm, d, w);
				break;

			case SCF_RISC_RET:
				ret = __asm_naja_RET(&inst, _asm, d, w);
				break;
			default:
				scf_loge("opcode '%s' NOT support, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
				return -1;
				break;
		};

		if (ret < 0)
			return ret;

		if (d->label) {
			inst->label = d->label;
			d   ->label = NULL;
		}

		scf_instruction_print(inst);
		d->opcode = NULL;

	} else if (d->fill) {
		if (d->i < 3) {
			scf_loge(".fill needs 3 operands, file: %s, line: %d\n", d->fill->file->data, d->fill->line);
			return SCF_DFA_ERROR;
		}

		int64_t  n    = d->operands[0].imm;
		int64_t  size = d->operands[1].imm;
		uint64_t imm  = d->operands[2].imm;
		int64_t  i;

		inst = calloc(1, sizeof(scf_instruction_t));
		if (!inst)
			return -ENOMEM;

		if (n * size <= sizeof(inst->code)) {
			for (i = 0; i < n; i++)
				memcpy(inst->code + i * size, (uint8_t*)&imm, size);

			inst->len = n * size;
		} else {
			inst->bin = scf_string_alloc();
			if (inst->bin) {
				scf_instruction_free(inst);
				return -ENOMEM;
			}

			for (i = 0; i < n; i++) {
				int ret = scf_string_cat_cstr_len(inst->bin, (uint8_t*)&imm, size);
				if (ret < 0) {
					scf_instruction_free(inst);
					return -ENOMEM;
				}
			}
		}

		RISC_INST_ADD_CHECK(_asm->current, inst);
		d->fill = NULL;
	}

	for (int i = 0; i < 4; i++) {
		d->operands[i].base  = NULL;
		d->operands[i].index = NULL;
		d->operands[i].scale = 0;
		d->operands[i].disp  = 0;

		d->operands[i].flag  = 0;
		d->operands[i].label = NULL;
		d->operands[i].imm   = 0;
		d->operands[i].imm_size = 0;
	}

	d->global = NULL;

	return SCF_DFA_OK;
}

static int _naja_action_hash(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	while (w = dfa->ops->pop_word(dfa))
	{
		if (SCF_LEX_WORD_LF == w->type) {
			dfa->ops->push_word(dfa, w);
			break;
		}

		dfa->ops->free_word(w);
	}

	return SCF_DFA_NEXT_WORD;
}

int _naja_set_offset_for_jmps(scf_vector_t* text)
{
	scf_instruction_t*  inst;
	scf_instruction_t*  dst;
	int i;
	int j;

	for (i = 0; i < text->size; i++) {
		inst      = text->data[i];

		if (!inst->OpCode)
			continue;

		uint32_t opcode;
		uint32_t label;

		opcode  = inst->code[0];
		opcode |= inst->code[1] <<  8;
		opcode |= inst->code[2] << 16;
		opcode |= inst->code[3] << 24;

		if (0x32 == (opcode >> 26) && 1 == (opcode & 1)) {
			label = (opcode >> 5) & 0x1fffff;

		} else if (0x30 == (opcode >> 26)
				|| 0x31 == (opcode >> 26)) {
			label = opcode & 0x3ffffff;
		} else
			continue;

		int32_t  bytes = 0;
		uint32_t flag  = label & 0xff;
		label >>= 8;

		if (1 == flag) {
			for (j = i; j >= 0; j--) {
				dst = text->data[j];

				if (dst->label
						&& scf_lex_is_const_integer(dst->label)
						&& dst->label->data.u32 == label) {
					inst->next = dst;
					break;
				}
			}

			if (j < 0) {
				scf_loge("number label %d NOT found\n", label);
				return -1;
			}

			for ( ; j < i; j++) {
				dst = text->data[j];

				if (dst->len > 0)
					bytes -= dst->len;
				else if (dst->bin)
					bytes -= dst->bin->len;
			}

		} else if (2 == flag) {
			for (j = i; j < text->size; j++) {
				dst       = text->data[j];

				if (dst->label
						&& scf_lex_is_const_integer(dst->label)
						&& dst->label->data.u32 == label) {
					inst->next = dst;
					break;
				}

				if (dst->len > 0)
					bytes += dst->len;
				else if (dst->bin)
					bytes += dst->bin->len;
			}

			if (j >= text->size) {
				scf_loge("number label %d NOT found\n", label);
				return -1;
			}
		} else
			continue;

		naja_set_jmp_offset(inst, bytes);
	}

	return 0;
}

static int _dfa_init_module_naja(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, naja, text,     scf_asm_is_text,      _naja_action_text);
	SCF_DFA_MODULE_NODE(dfa, naja, data,     scf_asm_is_data,      _naja_action_data);
	SCF_DFA_MODULE_NODE(dfa, naja, global,   scf_asm_is_global,    _naja_action_global);
	SCF_DFA_MODULE_NODE(dfa, naja, fill,     scf_asm_is_fill,      _naja_action_fill);

	SCF_DFA_MODULE_NODE(dfa, naja, type,     scf_asm_is_type,      _naja_action_type);
	SCF_DFA_MODULE_NODE(dfa, naja, str,      scf_asm_is_str,       _naja_action_str);
	SCF_DFA_MODULE_NODE(dfa, naja, number,   scf_asm_is_number,    _naja_action_number);

	SCF_DFA_MODULE_NODE(dfa, naja, identity, scf_dfa_is_identity,  _naja_action_identity);
	SCF_DFA_MODULE_NODE(dfa, naja, colon,    scf_dfa_is_colon,     _naja_action_colon);

	SCF_DFA_MODULE_NODE(dfa, naja, opcode,   _naja_is_opcode,       _naja_action_opcode);
	SCF_DFA_MODULE_NODE(dfa, naja, reg,      _naja_is_reg,          _naja_action_reg);

	SCF_DFA_MODULE_NODE(dfa, naja, lp,       scf_dfa_is_lp,        _naja_action_lp);
	SCF_DFA_MODULE_NODE(dfa, naja, rp,       scf_dfa_is_rp,        _naja_action_rp);

	SCF_DFA_MODULE_NODE(dfa, naja, comma,    scf_dfa_is_comma,     _naja_action_comma);
	SCF_DFA_MODULE_NODE(dfa, naja, LF,       scf_dfa_is_LF,        _naja_action_LF);
	SCF_DFA_MODULE_NODE(dfa, naja, HASH,     scf_dfa_is_hash,      _naja_action_hash);

	scf_asm_t*  _asm = dfa->priv;
	dfa_asm_t*  d    = _asm->dfa_data;

	d->type = SCF_LEX_WORD_ASM_BYTE;

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_naja(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, naja, text,      text);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, data,      data);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, global,    global);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, fill,      fill);

	SCF_DFA_GET_MODULE_NODE(dfa, naja, identity,  identity);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, colon,     colon);

	SCF_DFA_GET_MODULE_NODE(dfa, naja, type,      type);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, str,       str);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, number,    number);

	SCF_DFA_GET_MODULE_NODE(dfa, naja, opcode,    opcode);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, reg,       reg);

	SCF_DFA_GET_MODULE_NODE(dfa, naja, lp,        lp);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, rp,        rp);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, comma,     comma);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, LF,        LF);
	SCF_DFA_GET_MODULE_NODE(dfa, naja, HASH,      HASH);

	// asm syntax
	scf_vector_add(dfa->syntaxes, text);
	scf_vector_add(dfa->syntaxes, data);
	scf_vector_add(dfa->syntaxes, global);
	scf_vector_add(dfa->syntaxes, fill);

	scf_vector_add(dfa->syntaxes, opcode);
	scf_vector_add(dfa->syntaxes, identity);
	scf_vector_add(dfa->syntaxes, number);

	scf_vector_add(dfa->syntaxes, type);
	scf_vector_add(dfa->syntaxes, HASH);
	scf_vector_add(dfa->syntaxes, LF);

	// .text .data
	scf_dfa_node_add_child(text,      LF);
	scf_dfa_node_add_child(data,      LF);

	// label:
	scf_dfa_node_add_child(identity,  colon);
	scf_dfa_node_add_child(number,    colon);
	scf_dfa_node_add_child(colon,     LF);

	// .asciz
	scf_dfa_node_add_child(type,      str);
	scf_dfa_node_add_child(type,      number);
	scf_dfa_node_add_child(type,      identity);

	scf_dfa_node_add_child(str,       LF);
	scf_dfa_node_add_child(number,    LF);
	scf_dfa_node_add_child(identity,  LF);

	// asm instruction
	scf_dfa_node_add_child(opcode,    number);
	scf_dfa_node_add_child(opcode,    reg);
	scf_dfa_node_add_child(opcode,    identity);

	scf_dfa_node_add_child(identity,  comma);
	scf_dfa_node_add_child(number,    comma);
	scf_dfa_node_add_child(reg,       comma);

	scf_dfa_node_add_child(comma,     number);
	scf_dfa_node_add_child(comma,     reg);
	scf_dfa_node_add_child(comma,     identity);

	scf_dfa_node_add_child(identity,  LF);
	scf_dfa_node_add_child(number,    LF);
	scf_dfa_node_add_child(reg,       LF);

	// memory addr such as: (rax), 8(rax), disp(rax, rcx, 8),
	scf_dfa_node_add_child(opcode,    lp);
	scf_dfa_node_add_child(identity,  lp);
	scf_dfa_node_add_child(number,    lp);

	scf_dfa_node_add_child(lp,        reg);
	scf_dfa_node_add_child(lp,        comma); // ( , rcx, 8), empty base == 0

	scf_dfa_node_add_child(reg,       rp);
	scf_dfa_node_add_child(number,    rp);
	scf_dfa_node_add_child(rp,        comma);
	scf_dfa_node_add_child(rp,        LF);

	// .fill
	scf_dfa_node_add_child(fill,      number);

	// .global
	scf_dfa_node_add_child(global,    identity);
	scf_dfa_node_add_child(comma,     LF);
	scf_dfa_node_add_child(global,    LF);

	return SCF_DFA_OK;
}

scf_dfa_module_t dfa_module_naja = 
{
	.name        = "naja",
	.init_module = _dfa_init_module_naja,
	.init_syntax = _dfa_init_syntax_naja,
};
