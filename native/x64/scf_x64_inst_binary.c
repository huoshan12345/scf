#include"scf_x64.h"

static int _binary_assign_sib_float(scf_register_t* rb, scf_register_t* ri, int32_t scale, int32_t disp, scf_dag_node_t* src, scf_3ac_code_t* c, scf_function_t* f, int OpCode_type)
{
	scf_variable_t*     v    = src->var;
	scf_register_t*     rs   = NULL;
	scf_rela_t*         rela = NULL;

	scf_x64_OpCode_t*   OpCode;
	scf_x64_OpCode_t*   mov;
	scf_instruction_t*  inst;

	int mov_type;
	int ret;

	if (0 == src->color) {
		src->color     = -1;
		v->global_flag =  1;
	}

	ret = x64_select_reg(&rs, src, c, f, 1);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}
	assert(src->color > 0);

	mov_type = x64_float_OpCode_type(SCF_X64_MOV, v->type);
	if (SCF_X64_MOV == OpCode_type)
		goto end;
	OpCode_type = x64_float_OpCode_type(OpCode_type, v->type);

	ret = x64_overflow_reg(rs, c, f);
	if (ret < 0) {
		scf_loge("overflow reg failed\n");
		return ret;
	}

	mov  = x64_find_OpCode(mov_type, v->size, v->size, SCF_X64_E2G);
	if (ri)
		inst = x64_make_inst_SIB2G(mov, rs, rb, ri, scale, disp);
	else
		inst = x64_make_inst_P2G(mov, rs, rb, disp);
	X64_INST_ADD_CHECK(c->instructions, inst);

	OpCode = x64_find_OpCode(OpCode_type, v->size, v->size, SCF_X64_E2G);
	inst   = x64_make_inst_M2G(&rela, OpCode, rs, NULL, src->var);
	X64_INST_ADD_CHECK(c->instructions, inst);

end:
	mov  = x64_find_OpCode(mov_type, v->size, v->size, SCF_X64_G2E);
	if (ri)
		inst = x64_make_inst_G2SIB(mov, rb, ri, scale, disp, rs);
	else
		inst = x64_make_inst_G2P(mov, rb, disp, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _binary_assign_sib_int(x64_sib_t* sib, scf_dag_node_t* src, scf_3ac_code_t* c, scf_function_t* f, int OpCode_type)
{
	scf_variable_t*     v  = src->var;
	scf_register_t*     rs = NULL;

	scf_register_t*     rb = sib->base;
	scf_register_t*     ri = sib->index;
	int32_t scale          = sib->scale;
	int32_t disp           = sib->disp;

	scf_x64_OpCode_t*   OpCode;
	scf_instruction_t*  inst;

	int dsize = sib->size;
	int vsize = x64_variable_size(v);

	assert(dsize <= vsize);

	if (0 == src->color) {
		OpCode = x64_find_OpCode(OpCode_type, dsize, dsize, SCF_X64_I2E);
		if (OpCode) {
			if (ri)
				inst = x64_make_inst_I2SIB(OpCode, rb, ri, scale, disp, (uint8_t*)&v->data, dsize);
			else
				inst = x64_make_inst_I2P(OpCode, rb, disp, (uint8_t*)&v->data, dsize);
			X64_INST_ADD_CHECK(c->instructions, inst);
			return 0;
		}

		src->color = -1;
	}

	int ret = x64_select_reg(&rs, src, c, f, 1);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}
	assert(src->color > 0);

	rs = x64_find_register_color_bytes(rs->color, dsize);

	OpCode = x64_find_OpCode(OpCode_type, dsize, dsize, SCF_X64_G2E);
	if (!OpCode) {
		scf_loge("\n");
		return -EINVAL;
	}

	if (ri)
		inst = x64_make_inst_G2SIB(OpCode, rb, ri, scale, disp, rs);
	else
		inst = x64_make_inst_G2P(OpCode, rb, disp, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _binary_SIB2G(scf_native_t* ctx, scf_3ac_code_t* c, int OpCode_type, int nb_srcs, x64_sib_fill_pt fill)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != nb_srcs)
		return -EINVAL;

	scf_x64_context_t*  x64    = ctx->priv;
	scf_function_t*     f      = x64->f;

	scf_3ac_operand_t*  dst    = c->dsts->data[0];
	scf_3ac_operand_t*  base   = c->srcs->data[0];
	scf_3ac_operand_t*  index  = c->srcs->data[c->srcs->size - 1];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = scf_vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	scf_variable_t*     vd  = dst  ->dag_node->var;
	scf_variable_t*     vb  = base ->dag_node->var;
	scf_variable_t*     vi  = index->dag_node->var;

	scf_register_t*     rd  = NULL;
	x64_sib_t           sib = {0};

	scf_x64_OpCode_t*   lea;
	scf_x64_OpCode_t*   mov;
	scf_instruction_t*  inst;

	int ret = x64_select_reg(&rd, dst->dag_node, c, f, 0);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	ret = fill(&sib, base->dag_node, index->dag_node, c, f);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	int is_float = scf_variable_float(vd);
	if (is_float) {
		if (SCF_VAR_FLOAT == vd->type)
			mov  = x64_find_OpCode(SCF_X64_MOVSS, rd->bytes, rd->bytes, SCF_X64_E2G);
		else if (SCF_VAR_DOUBLE == vd->type)
			mov  = x64_find_OpCode(SCF_X64_MOVSD, rd->bytes, rd->bytes, SCF_X64_E2G);
	} else
		mov  = x64_find_OpCode(SCF_X64_MOV,   rd->bytes, rd->bytes, SCF_X64_E2G);

	if (sib.index) {
		inst = x64_make_inst_SIB2G(mov, rd, sib.base, sib.index, sib.scale, sib.disp);
		X64_INST_ADD_CHECK(c->instructions, inst);
	} else {
		inst = x64_make_inst_P2G(mov, rd, sib.base, sib.disp);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}
	return 0;
}

int x64_binary_assign(scf_native_t* ctx, scf_3ac_code_t* c, int OpCode_type)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	scf_x64_context_t* x64 = ctx->priv;
	scf_function_t*    f   = x64->f;
	scf_3ac_operand_t* src = c->srcs->data[0];
	scf_3ac_operand_t* dst = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = scf_vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	scf_variable_t* v = src->dag_node->var;

	int is_float = scf_variable_float(v);
	if (is_float)
		OpCode_type = x64_float_OpCode_type(OpCode_type, v->type);

	return x64_inst_op2(OpCode_type, dst->dag_node, src->dag_node, c, f);
}

int x64_assign_dereference(scf_native_t* ctx, scf_3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2) {
		scf_loge("\n");
		return -EINVAL;
	}

	scf_x64_context_t*  x64   = ctx->priv;
	scf_function_t*     f     = x64->f;
	scf_3ac_operand_t*  base  = c->srcs->data[0];
	scf_3ac_operand_t*  src   = c->srcs->data[1];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	scf_variable_t* b = base->dag_node->var;
	assert(b->nb_pointers > 0 || b->nb_dimentions > 0 || b->type >= SCF_STRUCT);

	if (!c->instructions) {
		c->instructions = scf_vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	scf_variable_t* v   = src->dag_node->var;
	x64_sib_t       sib = {0};

	int ret = x64_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	int is_float = scf_variable_float(v);
	if (is_float)
		return _binary_assign_sib_float(sib.base, sib.index, sib.scale, sib.disp, src->dag_node, c, f, SCF_X64_MOV);

	return _binary_assign_sib_int(&sib, src->dag_node, c, f, SCF_X64_MOV);
}

int x64_assign_pointer(scf_native_t* ctx, scf_3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3) {
		scf_loge("\n");
		return -EINVAL;
	}

	scf_x64_context_t*  x64    = ctx->priv;
	scf_function_t*     f      = x64->f;
	scf_3ac_operand_t*  base   = c->srcs->data[0];
	scf_3ac_operand_t*  member = c->srcs->data[1];
	scf_3ac_operand_t*  src    = c->srcs->data[2];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	scf_variable_t* b   = base->dag_node->var;
	scf_variable_t* vm  = member->dag_node->var;
	scf_variable_t* vs  = src->dag_node->var;
	scf_register_t* rs  = NULL;
	scf_register_t* r   = NULL;
	x64_sib_t       sib = {0};

	assert(b->nb_pointers > 0 || b->nb_dimentions > 0 || b->type >= SCF_STRUCT);

	if (!c->instructions) {
		c->instructions = scf_vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int ret = x64_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	int is_float = scf_variable_float(vs);
	if (is_float)
		return _binary_assign_sib_float(sib.base, sib.index, sib.scale, sib.disp, src->dag_node, c, f, SCF_X64_MOV);

	int dsize = sib.size;
	int vsize = x64_variable_size(vs);

	assert(dsize <= vsize);

	if (0 == src->dag_node->color)
		src->dag_node->color = -1;

	X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	rs = x64_find_register_color_bytes(rs->color, dsize);

	scf_instruction_t*  inst;
	scf_x64_OpCode_t*   mov;
	scf_x64_OpCode_t*   shl;
	scf_x64_OpCode_t*   shr;
	scf_x64_OpCode_t*   and;
	scf_x64_OpCode_t*   push;
	scf_x64_OpCode_t*   pop;

	mov = x64_find_OpCode(SCF_X64_MOV, dsize, dsize, SCF_X64_G2E);

	if (vm->bit_size > 0) {
		uint64_t mask = ((1ULL << vm->bit_size) - 1) << vm->bit_offset;
		mask = ~mask;

		push = x64_find_OpCode(SCF_X64_PUSH, 8,     8,     SCF_X64_G);
		pop  = x64_find_OpCode(SCF_X64_POP,  8,     8,     SCF_X64_G);
		mov  = x64_find_OpCode(SCF_X64_MOV,  dsize, dsize, SCF_X64_I2G);
		and  = x64_find_OpCode(SCF_X64_AND,  dsize, dsize, SCF_X64_G2E);

		r    = x64_find_register_color_bytes(rs->color, 8);
		inst = x64_make_inst_G(push, r);
		X64_INST_ADD_CHECK(c->instructions, inst);

		inst = x64_make_inst_I2G(mov, rs, (uint8_t*)&mask, dsize);
		X64_INST_ADD_CHECK(c->instructions, inst);

		if (sib.index)
			inst = x64_make_inst_G2SIB(and, sib.base, sib.index, sib.scale, sib.disp, rs);
		else
			inst = x64_make_inst_G2P(and, sib.base, sib.disp, rs);
		X64_INST_ADD_CHECK(c->instructions, inst);

		inst = x64_make_inst_G(pop, r);
		X64_INST_ADD_CHECK(c->instructions, inst);

		int imm = (rs->bytes << 3) - vm->bit_size;
		assert(imm > 0);

		shl  = x64_find_OpCode(SCF_X64_SHL, 1, rs->bytes, SCF_X64_I2E);
		shr  = x64_find_OpCode(SCF_X64_SHR, 1, rs->bytes, SCF_X64_I2E);

		inst = x64_make_inst_I2E(shl, rs, (uint8_t*)&imm, 1);
		X64_INST_ADD_CHECK(c->instructions, inst);

		imm -= vm->bit_offset;
		assert(imm >= 0);

		if (imm > 0) {
			inst = x64_make_inst_I2E(shr, rs, (uint8_t*)&imm, 1);
			X64_INST_ADD_CHECK(c->instructions, inst);
		}

		mov = x64_find_OpCode(SCF_X64_OR, dsize, dsize, SCF_X64_G2E);
	}

	if (!mov) {
		scf_loge("\n");
		return -EINVAL;
	}

	if (sib.index)
		inst = x64_make_inst_G2SIB(mov, sib.base, sib.index, sib.scale, sib.disp, rs);
	else
		inst = x64_make_inst_G2P(mov, sib.base, sib.disp, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

int x64_inst_dereference(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return _binary_SIB2G(ctx, c, SCF_X64_MOV, 1, x64_dereference_reg);
}

int x64_inst_pointer(scf_native_t* ctx, scf_3ac_code_t* c, int lea_flag)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	scf_x64_context_t*  x64    = ctx->priv;
	scf_function_t*     f      = x64->f;

	scf_3ac_operand_t*  dst    = c->dsts->data[0];
	scf_3ac_operand_t*  base   = c->srcs->data[0];
	scf_3ac_operand_t*  member = c->srcs->data[1];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = scf_vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	scf_variable_t*     vd  = dst->dag_node->var;
	scf_variable_t*     vb  = base  ->dag_node->var;
	scf_variable_t*     vm  = member->dag_node->var;

	scf_register_t*     rd  = NULL;
	x64_sib_t           sib = {0};

	scf_x64_OpCode_t*   lea;
	scf_x64_OpCode_t*   mov;
	scf_x64_OpCode_t*   shl;
	scf_x64_OpCode_t*   shr;
	scf_instruction_t*  inst;

	int ret = x64_select_reg(&rd, dst->dag_node, c, f, 0);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	ret = x64_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	if (scf_variable_is_struct(vm)
			|| scf_variable_is_array(vm)
			|| lea_flag) {
		mov = x64_find_OpCode(SCF_X64_LEA, rd->bytes, rd->bytes, SCF_X64_E2G);

	} else {
		int is_float = scf_variable_float(vd);
		if (is_float) {
			if (SCF_VAR_FLOAT == vd->type)
				mov = x64_find_OpCode(SCF_X64_MOVSS, rd->bytes, rd->bytes, SCF_X64_E2G);
			else if (SCF_VAR_DOUBLE == vd->type)
				mov = x64_find_OpCode(SCF_X64_MOVSD, rd->bytes, rd->bytes, SCF_X64_E2G);
		} else
			mov = x64_find_OpCode(SCF_X64_MOV, rd->bytes, rd->bytes, SCF_X64_E2G);
	}

	if (sib.index) {
		inst = x64_make_inst_SIB2G(mov, rd, sib.base, sib.index, sib.scale, sib.disp);
		X64_INST_ADD_CHECK(c->instructions, inst);
	} else {
		inst = x64_make_inst_P2G(mov, rd, sib.base, sib.disp);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}

	if (vm->bit_size > 0) {
		int imm = (rd->bytes << 3) - vm->bit_size - vm->bit_offset;
		assert(imm >= 0);

		shl = x64_find_OpCode(SCF_X64_SHL, 1, rd->bytes, SCF_X64_I2E);
		shr = x64_find_OpCode(SCF_X64_SHR, 1, rd->bytes, SCF_X64_I2E);

		if (imm > 0) {
			inst = x64_make_inst_I2E(shl, rd, (uint8_t*)&imm, 1);
			X64_INST_ADD_CHECK(c->instructions, inst);
		}

		imm += vm->bit_offset;
		inst = x64_make_inst_I2E(shr, rd, (uint8_t*)&imm, 1);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}

	return 0;
}
