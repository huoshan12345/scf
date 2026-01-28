#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_asm.h"
#include"scf_x64_opcode.h"
#include"scf_x64_reg.h"

extern scf_dfa_module_t  dfa_module_x64;


static int _x64_is_opcode(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return !!x64_find_OpCode_by_name(w->text->data);
}

static int _x64_is_reg(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return !!x64_find_register(w->text->data);
}

static int _x64_action_text(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	_asm->current = _asm->text;

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_data(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	_asm->current = _asm->data;

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_global(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->global = w;

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_fill(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->fill = w;
	d->i    = 0;

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_type(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->type = w->type;

	scf_logi("w: '%s'\n", w->text->data);

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_number(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

				if (SCF_X64_CALL == d->opcode->type
						|| (SCF_X64_JZ <= d->opcode->type && SCF_X64_JMP >= d->opcode->type)) {
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

		X64_INST_ADD_CHECK(_asm->current, inst, NULL);
		asm_inst_set_label(inst, d);
	}

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_str(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

	X64_INST_ADD_CHECK(_asm->current, inst, NULL);
	asm_inst_set_label(inst, d);

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_identity(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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
			X64_INST_ADD_CHECK(_asm->current, inst, NULL);
			asm_inst_set_label(inst, d);

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

static int _x64_action_opcode(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->opcode = (scf_OpCode_t*)x64_find_OpCode_by_name(w->text->data);
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

static int _x64_action_reg(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];
	scf_register_t*  r   = x64_find_register(w->text->data);

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

static int _x64_action_lp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_asm_t*  d = data;

	d->n_lp++;
	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_rp(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	dfa_asm_t*  d = data;

	if (++d->n_rp == d->n_lp) {
		d->operands[d->i].flag = 1;
		d->i++;
	}

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	if (d->n_lp != d->n_rp)
		d->n_comma++;

	return SCF_DFA_NEXT_WORD;
}

static int _x64_action_colon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

static int __x64_op2(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	scf_x64_OpCode_t*  opcode = (scf_x64_OpCode_t*)d->opcode;
	scf_instruction_t* inst   = NULL;
	scf_inst_data_t*   id0    = &d->operands[0];
	scf_inst_data_t*   id1    = &d->operands[1];

	int OpBytes  = opcode->OpBytes;
	int RegBytes = opcode->RegBytes;
	int EG       = opcode->EG;

	if (__inst_data_is_reg(id0)) {
		OpBytes = id0->base->bytes;

		if (__inst_data_is_reg(id1)) {
			RegBytes = id1->base->bytes;

			opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E2G);
			if (!opcode) {
				opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_G2E);

				if (!opcode) {
					scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
					return SCF_DFA_ERROR;
				}

				inst = x64_make_inst_G2E(opcode, id1->base, id0->base);
			} else
				inst = x64_make_inst_E2G(opcode, id1->base, id0->base);
			X64_INST_ADD_CHECK(_asm->current, inst, NULL);

		} else if (__inst_data_is_const(id1)) {

		} else {
			RegBytes = OpBytes;

			opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_G2E);
			if (!opcode) {
				scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
				return SCF_DFA_ERROR;
			}

			int32_t disp = id1->disp;
			if (id1->label)
				disp = 0x1000; // make disp to 4bytes, only for disp from a label

			if (id1->index)
				inst = x64_make_inst_G2SIB(opcode, id1->base, id1->index, id1->scale, disp, id0->base);
			else
				inst = x64_make_inst_G2P(opcode, id1->base, disp, id0->base);
			X64_INST_ADD_CHECK(_asm->current, inst, NULL);

			if (id0->label) {
				scf_rela_t* rela = calloc(1, sizeof(scf_rela_t));
				if (!rela)
					return -ENOMEM;

				*(uint32_t*)(inst->code + inst->len - 4) = 0;

				rela->inst_offset = inst->len - 4;
				X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id0->label->text);
			}
		}

	} else if (__inst_data_is_const(id0)) {
		uint64_t imm = id0->imm;

		if (imm >> 32)
			OpBytes = 8;
		else if (imm >> 16)
			OpBytes = 4;
		else if (imm >> 8)
			OpBytes = 2;
		else
			OpBytes = 1;

		if (__inst_data_is_reg(id1)) {
			RegBytes = id1->base->bytes;

			int i;
			for (i = OpBytes; i <= RegBytes; i <<= 1) {
				opcode = x64_find_OpCode(d->opcode->type, i, RegBytes, SCF_X64_I2E);
				if (opcode) {
					inst = x64_make_inst_I2E(opcode, id1->base, (uint8_t*)&imm, i);
					break;
				}
			}

			if (i > RegBytes) {
				for (i = OpBytes; i <= RegBytes; i <<= 1) {
					opcode = x64_find_OpCode(d->opcode->type, i, RegBytes, SCF_X64_I2G);
					if (opcode) {
						inst = x64_make_inst_I2G(opcode, id1->base, (uint8_t*)&imm, i);
						break;
					}
				}

				if (i > RegBytes) {
					scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
					return SCF_DFA_ERROR;
				}
			}

			X64_INST_ADD_CHECK(_asm->current, inst, NULL);
		}

	} else if (__inst_data_is_reg(id1)) {
		RegBytes = id1->base->bytes;
		OpBytes  = RegBytes;

		scf_logi("OpBytes: %d, RegBytes: %d\n", OpBytes, RegBytes);

		opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E2G);
		if (!opcode) {
			scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
			return SCF_DFA_ERROR;
		}

		int32_t disp = id0->disp;
		if (id0->label)
			disp = 0x1000; // make disp to 4bytes, only for disp from a label

		if (id1->index)
			inst = x64_make_inst_SIB2G(opcode, id1->base, id0->base, id0->index, id0->scale, disp);
		else
			inst = x64_make_inst_P2G(opcode, id1->base, id0->base, disp);
		X64_INST_ADD_CHECK(_asm->current, inst, NULL);

		if (id0->label) {
			scf_rela_t* rela = calloc(1, sizeof(scf_rela_t));
			if (!rela)
				return -ENOMEM;

			*(uint32_t*)(inst->code + inst->len - 4) = 0;

			rela->inst_offset = inst->len - 4;
			X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id0->label->text);
		}
	} else {
		scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
		return SCF_DFA_ERROR;
	}

	*__inst = inst;
	return 0;
}

static int __x64_op1(scf_instruction_t** __inst, scf_asm_t* _asm, dfa_asm_t* d, scf_lex_word_t* w)
{
	scf_x64_OpCode_t*  opcode = (scf_x64_OpCode_t*)d->opcode;
	scf_instruction_t* inst   = NULL;
	scf_inst_data_t*   id     = &d->operands[0];

	int OpBytes  = opcode->OpBytes;
	int RegBytes = opcode->RegBytes;
	int EG       = opcode->EG;

	if (__inst_data_is_reg(id)) {
		OpBytes = id->base->bytes;

		opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_G);
		if (!opcode) {
			opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E);
			if (!opcode) {
				scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
				return SCF_DFA_ERROR;
			}

			inst = x64_make_inst_E(opcode, id->base);
		} else
			inst = x64_make_inst_G(opcode, id->base);
		X64_INST_ADD_CHECK(_asm->current, inst, NULL);

	} else {
		scf_rela_t* rela = NULL;

		if (SCF_X64_CALL == d->opcode->type
				|| (SCF_X64_JZ <= d->opcode->type && SCF_X64_JMP >= d->opcode->type)) {
			uint32_t disp = 0;

			if (!id->label)
				disp = id->imm;

			opcode = x64_find_OpCode(d->opcode->type, 4, 4, SCF_X64_I);
			inst   = x64_make_inst_I(opcode, (uint8_t*)&disp, 4);
			X64_INST_ADD_CHECK(_asm->current, inst, NULL);

			if (id->label) {
				rela = calloc(1, sizeof(scf_rela_t));
				if (!rela)
					return -ENOMEM;

				rela->inst_offset = inst->len - 4;
				X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id->label->text);
			}
		} else {
			opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E);
			if (!opcode) {
				scf_loge("find proper opcode '%s' failed, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
				return SCF_DFA_ERROR;
			}

			inst = x64_make_inst_L(&rela, opcode);
			X64_INST_ADD_CHECK(_asm->current, inst, rela);
			X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id->label->text);
		}
	}

	*__inst = inst;
	return 0;
}

static int _x64_action_LF(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*         _asm  = dfa->priv;
	dfa_asm_t*          d    = data;
	scf_lex_word_t*     w    = words->data[words->size - 1];
	scf_instruction_t*  inst = NULL;

	if (d->opcode) {
		scf_x64_OpCode_t*  opcode = (scf_x64_OpCode_t*)d->opcode;
		scf_inst_data_t*   id0;

		int OpBytes  = opcode->OpBytes;
		int RegBytes = opcode->RegBytes;
		int EG       = opcode->EG;

		int ret = 0;
		switch (d->i) {
			case 2:
				ret = __x64_op2(&inst, _asm, d, w);
				break;
			case 1:
				ret = __x64_op1(&inst, _asm, d, w);
				break;
			case 0:
				inst = x64_make_inst(opcode, 8);
				X64_INST_ADD_CHECK(_asm->current, inst, NULL);
				break;
			default:
				break;
		};

		if (ret < 0)
			return ret;
		asm_inst_set_label(inst, d);

		scf_instruction_print(inst);
		d->opcode = NULL;

	} else if (d->fill) {
		if (SCF_LEX_WORD_ASM_ORG == d->fill->type) {
			if (d->i < 1) {
				scf_loge(".org needs 1 operand, file: %s, line: %d\n", d->fill->file->data, d->fill->line);
				return SCF_DFA_ERROR;
			}

			d->org = d->operands[0].imm;

		} else if (SCF_LEX_WORD_ASM_ALIGN == d->fill->type) {
			if (d->i < 1) {
				scf_loge(".align needs 1 operand, file: %s, line: %d\n", d->fill->file->data, d->fill->line);
				return SCF_DFA_ERROR;
			}

			d->align = 1 << d->operands[0].imm;

		} else if (SCF_LEX_WORD_ASM_FILL == d->fill->type) {
			if (d->i < 3) {
				scf_loge(".fill needs 3 operands, file: %s, line: %d\n", d->fill->file->data, d->fill->line);
				return SCF_DFA_ERROR;
			}

			int64_t  n    = d->operands[0].imm;
			int64_t  size = d->operands[1].imm;
			uint64_t imm  = d->operands[2].imm;

			if (scf_asm_fill(&inst, n, size, imm) < 0)
				return SCF_DFA_ERROR;

			X64_INST_ADD_CHECK(_asm->current, inst, NULL);
			asm_inst_set_label(inst, d);
		}

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

static int _x64_action_hash(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

int _x64_set_offset_for_jmps(scf_vector_t* text)
{
	scf_instruction_t*  inst;
	scf_instruction_t*  dst;
	int i;
	int j;

	for (i = 0; i < text->size; i++) {
		inst      = text->data[i];

		if (!inst->OpCode)
			continue;

		if (inst->OpCode->type != SCF_X64_CALL
				&& (inst->OpCode->type < SCF_X64_JZ || inst->OpCode->type > SCF_X64_JMP))
			continue;

		uint32_t label = *(uint32_t*)(inst->code + inst->len - 4);
		uint32_t flag  = label & 0xff;
		label >>= 8;

		switch (flag) {
			case 1:
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
				break;

			case 2:
				for (j = i + 1; j < text->size; j++) {
					dst           = text->data[j];

					if (dst->label
							&& scf_lex_is_const_integer(dst->label)
							&& dst->label->data.u32 == label) {
						inst->next = dst;
						break;
					}
				}

				if (j >= text->size) {
					scf_loge("number label %d NOT found\n", label);
					return -1;
				}
				break;
			default:
				break;
		};

		inst->flag = flag;
	}

	while (1) {
		int drop_bytes = 0;

		if (scf_asm_len(text) < 0)
			return -1;

		for (i = 0; i < text->size; i++) {
			inst      = text->data[i];

			if (!inst->OpCode || !inst->next)
				continue;

			if (inst->OpCode->type != SCF_X64_CALL
					&& (inst->OpCode->type < SCF_X64_JZ || inst->OpCode->type > SCF_X64_JMP))
				continue;

			int32_t bytes = 0;
			switch (inst->flag) {
				case 1:
				case 2:
					bytes = inst->next->offset - (inst->offset + inst->len);
					break;
				default:
					break;
			};

			scf_x64_OpCode_t* opcode = (scf_x64_OpCode_t*)inst->OpCode;
			int n_bytes = 4;

			if (SCF_X64_CALL != opcode->type) {
				if (-128 <= bytes && bytes <= 127)
					n_bytes = 1;

				opcode = x64_find_OpCode(opcode->type, n_bytes, n_bytes, SCF_X64_I);
			}

			int old_len = inst->len;
			x64_make_inst_I2(inst, opcode, (uint8_t*)&bytes, n_bytes);

			int diff = old_len - inst->len;
			assert(diff >= 0);

			drop_bytes += diff;
		}

		if (0 == drop_bytes)
			break;
	}

	return 0;
}

static int _dfa_init_module_x64(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, x64, text,     scf_asm_is_text,      _x64_action_text);
	SCF_DFA_MODULE_NODE(dfa, x64, data,     scf_asm_is_data,      _x64_action_data);
	SCF_DFA_MODULE_NODE(dfa, x64, global,   scf_asm_is_global,    _x64_action_global);
	SCF_DFA_MODULE_NODE(dfa, x64, fill,     scf_asm_is_fill,      _x64_action_fill);
	SCF_DFA_MODULE_NODE(dfa, x64, align,    scf_asm_is_align,     _x64_action_fill);
	SCF_DFA_MODULE_NODE(dfa, x64, org,      scf_asm_is_org,       _x64_action_fill);

	SCF_DFA_MODULE_NODE(dfa, x64, type,     scf_asm_is_type,      _x64_action_type);
	SCF_DFA_MODULE_NODE(dfa, x64, str,      scf_asm_is_str,       _x64_action_str);
	SCF_DFA_MODULE_NODE(dfa, x64, number,   scf_asm_is_number,    _x64_action_number);

	SCF_DFA_MODULE_NODE(dfa, x64, identity, scf_dfa_is_identity,  _x64_action_identity);
	SCF_DFA_MODULE_NODE(dfa, x64, colon,    scf_dfa_is_colon,     _x64_action_colon);

	SCF_DFA_MODULE_NODE(dfa, x64, opcode,   _x64_is_opcode,       _x64_action_opcode);
	SCF_DFA_MODULE_NODE(dfa, x64, reg,      _x64_is_reg,          _x64_action_reg);
	SCF_DFA_MODULE_NODE(dfa, x64, percent,  scf_asm_is_percent,   scf_dfa_action_next);

	SCF_DFA_MODULE_NODE(dfa, x64, lp,       scf_dfa_is_lp,        _x64_action_lp);
	SCF_DFA_MODULE_NODE(dfa, x64, rp,       scf_dfa_is_rp,        _x64_action_rp);

	SCF_DFA_MODULE_NODE(dfa, x64, comma,    scf_dfa_is_comma,     _x64_action_comma);
	SCF_DFA_MODULE_NODE(dfa, x64, LF,       scf_dfa_is_LF,        _x64_action_LF);
	SCF_DFA_MODULE_NODE(dfa, x64, HASH,     scf_dfa_is_hash,      _x64_action_hash);

	scf_asm_t*  _asm = dfa->priv;
	dfa_asm_t*  d    = _asm->dfa_data;

	d->type = SCF_LEX_WORD_ASM_BYTE;

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_x64(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, x64, text,      text);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, data,      data);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, global,    global);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, fill,      fill);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, align,     align);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, org,       org);

	SCF_DFA_GET_MODULE_NODE(dfa, x64, identity,  identity);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, colon,     colon);

	SCF_DFA_GET_MODULE_NODE(dfa, x64, type,      type);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, str,       str);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, number,    number);

	SCF_DFA_GET_MODULE_NODE(dfa, x64, opcode,    opcode);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, reg,       reg);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, percent,   percent);

	SCF_DFA_GET_MODULE_NODE(dfa, x64, lp,        lp);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, rp,        rp);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, comma,     comma);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, LF,        LF);
	SCF_DFA_GET_MODULE_NODE(dfa, x64, HASH,      HASH);

	// asm syntax
	scf_vector_add(dfa->syntaxes, text);
	scf_vector_add(dfa->syntaxes, data);
	scf_vector_add(dfa->syntaxes, global);
	scf_vector_add(dfa->syntaxes, fill);
	scf_vector_add(dfa->syntaxes, align);
	scf_vector_add(dfa->syntaxes, org);

	scf_vector_add(dfa->syntaxes, opcode);
	scf_vector_add(dfa->syntaxes, identity);
	scf_vector_add(dfa->syntaxes, number);

	scf_vector_add(dfa->syntaxes, type);
	scf_vector_add(dfa->syntaxes, HASH);
	scf_vector_add(dfa->syntaxes, LF);

	// .text .data
	scf_dfa_node_add_child(text,      LF);
	scf_dfa_node_add_child(data,      LF);

	// .global
	scf_dfa_node_add_child(global,    identity);
	scf_dfa_node_add_child(identity,  comma);
	scf_dfa_node_add_child(comma,     identity);
	scf_dfa_node_add_child(identity,  LF);
	scf_dfa_node_add_child(comma,     LF);
	scf_dfa_node_add_child(global,    LF);

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

	// %reg of 'AT & T'
	scf_dfa_node_add_child(percent,   reg);
	scf_dfa_node_add_child(opcode,    percent);
	scf_dfa_node_add_child(comma,     percent);

	// memory addr such as: (rax), 8(rax), disp(rax, rcx, 8),
	scf_dfa_node_add_child(opcode,    lp);
	scf_dfa_node_add_child(identity,  lp);
	scf_dfa_node_add_child(number,    lp);

	scf_dfa_node_add_child(lp,        percent);
	scf_dfa_node_add_child(lp,        reg);
	scf_dfa_node_add_child(lp,        comma); // ( , rcx, 8), empty base == 0

	scf_dfa_node_add_child(reg,       rp);
	scf_dfa_node_add_child(number,    rp);
	scf_dfa_node_add_child(rp,        comma);
	scf_dfa_node_add_child(rp,        LF);

	scf_dfa_node_add_child(fill,      number); // .fill
	scf_dfa_node_add_child(align,     number); // .align
	scf_dfa_node_add_child(org,       number); // .org

	return SCF_DFA_OK;
}

scf_dfa_module_t dfa_module_x64 = 
{
	.name        = "x64",
	.init_module = _dfa_init_module_x64,
	.init_syntax = _dfa_init_syntax_x64,
};
