#include"scf_dfa.h"
#include"scf_dfa_util.h"
#include"scf_asm.h"
#include"scf_x64_opcode.h"
#include"scf_x64_reg.h"

extern scf_dfa_module_t  dfa_module_inst;

static int _inst_is_text(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_ASM_TEXT == w->type;
}

static int _inst_is_data(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_ASM_DATA == w->type;
}

static int _inst_is_global(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_ASM_GLOBAL == w->type;
}

static int _inst_is_asciz(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_ASM_ASCIZ == w->type;
}

static int _inst_is_str(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_CONST_STRING == w->type;
}

static int _inst_is_percent(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return SCF_LEX_WORD_MOD == w->type;
}

static int _inst_is_opcode(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return !!x64_find_OpCode_by_name(w->text->data);
}

static int _inst_is_reg(scf_dfa_t* dfa, void* word)
{
	scf_lex_word_t* w = word;

	return !!x64_find_register(w->text->data);
}

static int _inst_action_text(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	_asm->current = _asm->text;

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_data(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	_asm->current = _asm->data;

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_global(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->global = w;

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_asciz(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	d->type = w->type;

	scf_logi("w: '%s'\n", w->text->data);

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_str(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

	X64_INST_ADD_CHECK(_asm->current, inst);
	if (d->label) {
		inst->label = d->label;
		d   ->label = NULL;
	}

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_identity(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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
	}

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_opcode(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

static int _inst_action_reg(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

static int _inst_action_comma(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	if (d->n_lp != d->n_rp)
		d->n_comma++;

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_colon(scf_dfa_t* dfa, scf_vector_t* words, void* data)
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

	d->label = scf_lex_word_clone(w);
	if (!d->label)
		return -ENOMEM;

	return SCF_DFA_NEXT_WORD;
}

static int _inst_action_LF(scf_dfa_t* dfa, scf_vector_t* words, void* data)
{
	scf_asm_t*      _asm = dfa->priv;
	dfa_asm_t*       d   = data;
	scf_lex_word_t*  w   = words->data[words->size - 1];

	if (d->opcode) {
		scf_instruction_t* inst;
		scf_x64_OpCode_t*  opcode = (scf_x64_OpCode_t*)d->opcode;
		scf_inst_data_t*   id0;
		scf_inst_data_t*   id1;

		int OpBytes  = opcode->OpBytes;
		int RegBytes = opcode->RegBytes;
		int EG       = opcode->EG;

		if (2 == d->i) {
			id0 = &d->operands[0];
			id1 = &d->operands[1];

			if (__inst_data_is_reg(id0)) {
				OpBytes = id0->base->bytes;

				if (__inst_data_is_reg(id1)) {
					RegBytes = id1->base->bytes;

					opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E2G);
					if (!opcode) {
						scf_loge("valid opcode '%s' NOT found, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
						return SCF_DFA_ERROR;
					}

					inst = x64_make_inst_E2G(opcode, id1->base, id0->base);
					X64_INST_ADD_CHECK(_asm->current, inst);

				}

			} else if (__inst_data_is_reg(id1)) {
				RegBytes = id1->base->bytes;

				opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E2G);
				if (!opcode) {
					scf_loge("valid opcode '%s' NOT found, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
					return SCF_DFA_ERROR;
				}

				scf_rela_t* rela = NULL;

				inst = x64_make_inst_L2G(&rela, opcode, id1->base);
				X64_INST_ADD_CHECK(_asm->current, inst);
				X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id0->label->text);
			}

		} else if (1 == d->i) {
			id0 = &d->operands[0];

			if (__inst_data_is_reg(id0)) {
				OpBytes = id0->base->bytes;

				opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_G);
				if (!opcode) {
					opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E);
					if (!opcode) {
						scf_loge("valid opcode '%s' NOT found, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
						return SCF_DFA_ERROR;
					}

					inst = x64_make_inst_E(opcode, id0->base);
				} else
					inst = x64_make_inst_G(opcode, id0->base);
				X64_INST_ADD_CHECK(_asm->current, inst);

			} else {
				scf_rela_t* rela = NULL;
				uint32_t    disp = 0;

				if (SCF_X64_CALL == d->opcode->type) {
					opcode = x64_find_OpCode(d->opcode->type, 4, 4, SCF_X64_I);
					inst   = x64_make_inst_I(opcode, (uint8_t*)&disp, 4);
					X64_INST_ADD_CHECK(_asm->current, inst);

					rela = calloc(1, sizeof(scf_rela_t));
					if (!rela)
						return -ENOMEM;

					rela->inst_offset = 1;
					X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id0->label->text);
				} else {
					opcode = x64_find_OpCode(d->opcode->type, OpBytes, RegBytes, SCF_X64_E);
					if (!opcode) {
						scf_loge("valid opcode '%s' NOT found, file: %s, line: %d\n", d->opcode->name, w->file->data, w->line);
						return SCF_DFA_ERROR;
					}

					inst = x64_make_inst_L(&rela, opcode);
					X64_INST_ADD_CHECK(_asm->current, inst);
					X64_RELA_ADD_LABEL(_asm->text_relas, rela, inst, id0->label->text);
				}
			}
		} else {
			inst = x64_make_inst(opcode, 8);
			X64_INST_ADD_CHECK(_asm->current, inst);
		}

		if (d->label) {
			inst->label = d->label;
			d   ->label = NULL;
		}

		scf_instruction_print(inst);
		d->opcode = NULL;
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

static int _dfa_init_module_inst(scf_dfa_t* dfa)
{
	SCF_DFA_MODULE_NODE(dfa, inst, text,     _inst_is_text,       _inst_action_text);
	SCF_DFA_MODULE_NODE(dfa, inst, data,     _inst_is_data,       _inst_action_data);
	SCF_DFA_MODULE_NODE(dfa, inst, global,   _inst_is_global,     _inst_action_global);

	SCF_DFA_MODULE_NODE(dfa, inst, asciz,    _inst_is_asciz,      _inst_action_asciz);
	SCF_DFA_MODULE_NODE(dfa, inst, str,      _inst_is_str,        _inst_action_str);

	SCF_DFA_MODULE_NODE(dfa, inst, identity, scf_dfa_is_identity, _inst_action_identity);
	SCF_DFA_MODULE_NODE(dfa, inst, colon,    scf_dfa_is_colon,    _inst_action_colon);

	SCF_DFA_MODULE_NODE(dfa, inst, opcode,   _inst_is_opcode,     _inst_action_opcode);
	SCF_DFA_MODULE_NODE(dfa, inst, reg,      _inst_is_reg,        _inst_action_reg);
	SCF_DFA_MODULE_NODE(dfa, inst, percent,  _inst_is_percent,    scf_dfa_action_next);

	SCF_DFA_MODULE_NODE(dfa, inst, comma,    scf_dfa_is_comma,    _inst_action_comma);
	SCF_DFA_MODULE_NODE(dfa, inst, LF,       scf_dfa_is_LF,       _inst_action_LF);

	scf_asm_t*  _asm = dfa->priv;
	dfa_asm_t*  d    = _asm->dfa_data;

	d->type = SCF_LEX_WORD_ASM_BYTE;

	return SCF_DFA_OK;
}

static int _dfa_init_syntax_inst(scf_dfa_t* dfa)
{
	SCF_DFA_GET_MODULE_NODE(dfa, inst, text,      text);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, data,      data);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, global,    global);

	SCF_DFA_GET_MODULE_NODE(dfa, inst, identity,  identity);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, colon,     colon);

	SCF_DFA_GET_MODULE_NODE(dfa, inst, asciz,     asciz);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, str,       str);

	SCF_DFA_GET_MODULE_NODE(dfa, inst, opcode,    opcode);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, reg,       reg);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, percent,   percent);

	SCF_DFA_GET_MODULE_NODE(dfa, inst, comma,     comma);
	SCF_DFA_GET_MODULE_NODE(dfa, inst, LF,        LF);

	// asm syntax
	scf_vector_add(dfa->syntaxes, text);
	scf_vector_add(dfa->syntaxes, data);
	scf_vector_add(dfa->syntaxes, global);

	scf_vector_add(dfa->syntaxes, opcode);
	scf_vector_add(dfa->syntaxes, identity);

	scf_vector_add(dfa->syntaxes, asciz);
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
	scf_dfa_node_add_child(colon,     LF);

	// .asciz
	scf_dfa_node_add_child(asciz,     str);
	scf_dfa_node_add_child(str,       LF);

	// asm instruction
	scf_dfa_node_add_child(opcode,    identity);

	scf_dfa_node_add_child(opcode,    reg);
	scf_dfa_node_add_child(reg,       comma);
	scf_dfa_node_add_child(comma,     reg);
	scf_dfa_node_add_child(reg,       LF);

	// %reg of 'AT & T'
	scf_dfa_node_add_child(percent,   reg);
	scf_dfa_node_add_child(opcode,    percent);
	scf_dfa_node_add_child(comma,     percent);

	return SCF_DFA_OK;
}

scf_dfa_module_t dfa_module_inst = 
{
	.name        = "inst",
	.init_module = _dfa_init_module_inst,
	.init_syntax = _dfa_init_syntax_inst,
};
