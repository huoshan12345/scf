#include"scf_risc.h"

int naja_inst_I2G(scf_3ac_code_t* c, scf_register_t* rd, uint64_t imm, int bytes)
{
	scf_instruction_t* inst;

	uint64_t invert = ~imm;
	uint32_t opcode;

	if (0 == (invert >> 32)) {

		// mvn rd, invert[15:0]
		opcode = (0xf << 26) | (rd->id << 21) | (1 << 18) | (0x3 << 16) | (invert & 0xffff);
		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		if (invert >> 16) {
			// movk rd, imm[31:16]
			opcode = (0xf << 26) | (rd->id << 21) | (1 << 19) | (0x3 << 16)| ((imm >> 16) & 0xffff);
			inst   = risc_make_inst(c, opcode);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		}

		return 0;
	}

	// mov rd, imm[15:0]
	opcode = (0xf << 26) | (rd->id << 21) | (0x3 << 16) | (imm & 0xffff);
	inst   = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	imm >>= 16;
	if (imm & 0xffff) {

		// movk rd, imm[31:16]
		opcode = (0xf << 26) | (rd->id << 21) | (1 << 19) | (0x3 << 16) | (imm & 0xffff);
		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);
	}

	imm >>= 16;
	if (imm & 0xffff) {

		// movk rd, imm[47:32]
		opcode = (0xf << 26) | (rd->id << 21) | (2 << 19) | (0x3 << 16) | (imm & 0xffff);
		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);
	}

	imm >>= 16;
	if (imm & 0xffff) {

		// movk rd, imm[63:48]
		opcode = (0xf << 26) | (rd->id << 21) | (3 << 19) | (0x3 << 16) | (imm & 0xffff);
		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);
	}

	return 0;
}

int naja_inst_ADR2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_variable_t* vs)
{
	scf_register_t* fp   = f->rops->find_register("fp");
	scf_instruction_t*    inst = NULL;
	scf_rela_t*           rela = NULL;

	int64_t  offset;
	uint32_t opcode;
	uint32_t SIZE = 0;
	uint32_t S    = 1;

	int size = f->rops->variable_size(vs);

	if (vs->local_flag || vs->tmp_flag) {

		offset = vs->bp_offset;

		if (offset >= 0 && offset <= 0x3fff)

			opcode = (0 << 26) | (rd->id << 21) | (0x3 << 19) | (offset << 5) | fp->id;

		else if (offset < 0 && -offset <= 0x3fff)

			opcode = (1 << 26) | (rd->id << 21) | (0x3 << 19) | ((-offset) << 5) | fp->id;

		else {
			int ret = naja_inst_I2G(c, rd, offset, 8);
			if (ret < 0)
				return ret;

			opcode = (0 << 26) | (rd->id << 21) | (rd->id << 5) | fp->id;
		}

		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);

	} else if (vs->global_flag) {
		offset = 0;

		opcode = (0x2a << 26) | (rd->id << 21);
		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		RISC_RELA_ADD_CHECK(f->data_relas, rela, c, vs, NULL);
		rela->type = R_AARCH64_ADR_PREL_PG_HI21;

		opcode = (0 << 26) | (rd->id << 21) | (0x3 << 19) | rd->id;
		inst   = risc_make_inst(c, opcode);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		RISC_RELA_ADD_CHECK(f->data_relas, rela, c, vs, NULL);
		rela->type = R_AARCH64_ADD_ABS_LO12_NC;

	} else {
		scf_loge("temp var should give a register\n");
		return -EINVAL;
	}

	return 0;
}

int naja_inst_M2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rb, scf_variable_t* vs)
{
	scf_register_t*    fp   = f->rops->find_register("fp");
	scf_register_t*    ri   = NULL;
	scf_instruction_t* inst = NULL;
	scf_rela_t*        rela = NULL;

	int64_t  offset;
	uint32_t opcode;
	uint32_t SIZE = 0;

	int size = f->rops->variable_size(vs);

	if (!rb) {
		if (vs->local_flag || vs->tmp_flag) {

			offset = vs->bp_offset;
			rb     = fp;

		} else if (vs->global_flag) {
			offset = 0;

			int ret = naja_inst_ADR2G(c, f, rd, vs);
			if (ret < 0)
				return -EINVAL;

			rb = rd;

		} else {
			scf_loge("temp var should give a register\n");
			return -EINVAL;
		}

	} else {
		if (vs->local_flag || vs->tmp_flag)
			offset = vs->bp_offset;
		else
			offset = vs->offset;
	}

	if (1 == size)
		SIZE = 0;
	else if (2 == size) {

		if (offset & 0x1) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 1;
		SIZE = 1;

	} else if (4 == size) {

		if (offset & 0x3) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 2;
		SIZE = 2;

	} else if (8 == size) {

		if (offset & 0x7) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 3;
		SIZE = 3;
	} else
		return -EINVAL;


	if (offset >= -0xfff && offset <= 0xfff)
		opcode = (0x4 << 26) | ((offset & 0x1fff) << 5) | rb->id;
	else {
		int ret = risc_select_free_reg(&ri, c, f, 0);
		if (ret < 0) {
			scf_loge("\n");
			return -EINVAL;
		}

		ret = naja_inst_I2G(c, ri, offset, 4);
		if (ret < 0)
			return ret;

		opcode = (0xd << 26) | (SIZE << 10) | (ri->id << 5) | rb->id;
	}

	if (rd->bytes > size && scf_variable_signed(vs))
		opcode |= 0x1 << 18;

	else if (scf_variable_float(vs) && 4 == size)
		opcode |= 0x1 << 18;

	scf_loge("SIZE: %d, size: %d\n", SIZE, size);

	opcode |= (rd->id << 21) | SIZE << 19;
	opcode |= RISC_COLOR_TYPE(rd->color) << 30;

	inst    = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

int naja_inst_G2M(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rs, scf_register_t* rb, scf_variable_t* vs)
{
	scf_register_t*    fp   = f->rops->find_register("fp");
	scf_register_t*    ri   = NULL;
	scf_instruction_t* inst = NULL;
	scf_rela_t*        rela = NULL;

	int64_t  offset;
	uint32_t opcode;
	uint32_t SIZE = 0;

	int size = f->rops->variable_size(vs);

	if (!rb) {
		if (vs->local_flag || vs->tmp_flag) {

			offset = vs->bp_offset;
			rb     = fp;

		} else if (vs->global_flag) {
			offset = 0;

			int ret = risc_select_free_reg(&rb, c, f, 0);
			if (ret < 0) {
				scf_loge("\n");
				return -EINVAL;
			}

			ret = naja_inst_ADR2G(c, f, rb, vs);
			if (ret < 0)
				return -EINVAL;

		} else {
			scf_loge("temp var should give a register\n");
			return -EINVAL;
		}

	} else {
		if (vs->local_flag || vs->tmp_flag)
			offset = vs->bp_offset;
		else
			offset = vs->offset;
	}

	if (1 == size)
		SIZE = 0;
	else if (2 == size) {

		if (offset & 0x1) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 1;
		SIZE = 1;

	} else if (4 == size) {

		if (offset & 0x3) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 2;
		SIZE = 2;

	} else if (8 == size) {

		if (offset & 0x7) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 3;
		SIZE = 3;

	} else
		return -EINVAL;

	scf_loge("offset: %ld, SIZE: %d\n", offset, SIZE);

	if (offset >= -0xfff && offset <= 0xfff)
		opcode = (0x6 << 26) | ((offset & 0x1fff) << 5) | rb->id;
	else {
		int ret = risc_select_free_reg(&ri, c, f, 0);
		if (ret < 0) {
			scf_loge("\n");
			return -EINVAL;
		}

		ret = naja_inst_I2G(c, ri, offset, 4);
		if (ret < 0)
			return ret;

		opcode = (0xe << 26) | (SIZE << 10) | (ri->id << 5) | rb->id;
	}

	opcode |= (rs->id << 21) | SIZE << 19;
	opcode |= RISC_COLOR_TYPE(rs->color) << 30;

	if (scf_variable_float(vs) && 4 == size)
		opcode |= (1 << 18);

	inst    = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

int naja_inst_ISTR2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_variable_t* v)
{
	scf_instruction_t*    inst = NULL;
	scf_rela_t*           rela = NULL;

	int size1 = f->rops->variable_size(v);

	assert(8 == rd->bytes);
	assert(8 == size1);

	v->global_flag = 1;
	v->local_flag  = 0;
	v->tmp_flag    = 0;

	uint32_t opcode;

	opcode = (0x2a << 26) | (rd->id << 21);
	inst   = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	RISC_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
	rela->type = R_AARCH64_ADR_PREL_PG_HI21;

	opcode = (0 << 26) | (rd->id << 21) | (0x3 << 19) | rd->id;
	inst   = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	RISC_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
	rela->type = R_AARCH64_ADD_ABS_LO12_NC;

	return 0;
}

int naja_inst_G2P(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rs, scf_register_t* rb, int32_t offset, int size)
{
	scf_register_t* ri   = NULL;
	scf_instruction_t*    inst = NULL;

	uint32_t opcode;
	uint32_t SIZE = 0;

	if (!rb)
		return -EINVAL;

	if (1 == size)
		SIZE = 0;
	else if (2 == size) {

		if (offset & 0x1) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 1;
		SIZE = 1;

	} else if (4 == size) {

		if (offset & 0x3) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 2;
		SIZE = 2;

	} else if (8 == size) {

		if (offset & 0x7) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 3;
		SIZE = 3;

	} else
		return -EINVAL;

	if (offset >= -0xfff && offset <= 0xfff)
		opcode = (0x6 << 26) | ((offset & 0x1fff) << 5) | rb->id;
	else {
		int ret = risc_select_free_reg(&ri, c, f, 0);
		if (ret < 0) {
			scf_loge("\n");
			return -EINVAL;
		}

		ret = naja_inst_I2G(c, ri, offset, 4);
		if (ret < 0)
			return ret;

		opcode = (0xe << 26) | (SIZE << 10) | (ri->id << 5) | rb->id;
	}

	opcode |= (rs->id << 21) | SIZE << 19;
	opcode |= RISC_COLOR_TYPE(rs->color) << 30;

	inst    = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

int naja_inst_P2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rb, int32_t offset, int size)
{
	scf_register_t* ri   = NULL;
	scf_instruction_t*    inst = NULL;

	uint32_t opcode;
	uint32_t SIZE = 0;

	if (!rb)
		return -EINVAL;

	if (1 == size)
		SIZE = 0;
	else if (2 == size) {

		if (offset & 0x1) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 1;
		SIZE = 1;

	} else if (4 == size) {

		if (offset & 0x3) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 2;
		SIZE = 2;

	} else if (8 == size) {

		if (offset & 0x7) {
			scf_loge("memory align error\n");
			return -EINVAL;
		}

		offset >>= 3;
		SIZE = 3;

	} else
		return -EINVAL;

	if (offset >= -0xfff && offset <= 0xfff)
		opcode = (0x4 << 26) | ((offset & 0x1fff) << 5) | rb->id;
	else {
		int ret = risc_select_free_reg(&ri, c, f, 0);
		if (ret < 0) {
			scf_loge("\n");
			return -EINVAL;
		}

		ret = naja_inst_I2G(c, ri, offset, 4);
		if (ret < 0)
			return ret;

		opcode = (0xd << 26) | (SIZE << 10) | (ri->id << 5) | rb->id;
	}

	opcode |= (rd->id << 21) | SIZE << 19;
	opcode |= RISC_COLOR_TYPE(rd->color) << 30;

	inst    = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

int naja_inst_ADRP2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rb, int32_t offset)
{
	scf_register_t* r    = NULL;
	scf_instruction_t*    inst = NULL;

	uint32_t opcode = 0;

	if (offset >= 0 && offset <= 0x3fff)
		opcode = (0 << 26) | (rd->id << 21) | (3 << 19) | (offset << 5) | rb->id;

	else if (offset < 0 && offset >= -0x3fff)
		opcode = (1 << 26) | (rd->id << 21) | (3 << 19) | ((-offset) << 5) | rb->id;

	else {
		int ret = risc_select_free_reg(&r, c, f, 0);
		if (ret < 0)
			return ret;

		ret = naja_inst_I2G(c, r, offset, 4);
		if (ret < 0)
			return ret;

		opcode = (0 << 26) | (rd->id << 21) | (r->id << 5) | rb->id;
	}

	inst   = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

int naja_inst_ADRSIB2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_sib_t* sib)
{
	scf_register_t* rb   = sib->base;
	scf_register_t* ri   = sib->index;
	scf_instruction_t*    inst = NULL;

	assert(0 == sib->disp);

	if (!rb || !ri)
		return -EINVAL;

	uint32_t opcode;
	uint32_t SH;

	if (1 == sib->scale)
		SH = 0;

	else if (2 == sib->scale)
		SH = 1;

	else if (4 == sib->scale)
		SH = 2;

	else if (8 == sib->scale)
		SH = 3;
	else
		return -EINVAL;

	opcode = (0 << 26) | (rd->id << 21) | (SH << 10) | (ri->id << 5) | rb->id;
	inst   = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

int naja_inst_SIB2G(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_sib_t* sib)
{
	scf_register_t* rb   = sib->base;
	scf_register_t* ri   = sib->index;
	scf_instruction_t*    inst = NULL;

	assert(0 == sib->disp);

	if (!rb || !ri)
		return -EINVAL;

	int scale  = sib->scale;
	int size   = sib->size;

	uint32_t opcode;
	uint32_t SIZE = 0;

	if (1 == size)
		SIZE = 0;

	else if (2 == size)
		SIZE = 1;

	else if (4 == size)
		SIZE = 2;

	else if (8 == size)
		SIZE = 3;
	else
		return -EINVAL;

	opcode  = (0xd << 26) | (rd->id << 21) | (SIZE << 10) | (ri->id << 5) | rb->id;
	opcode |= SIZE << 19;
	opcode |= RISC_COLOR_TYPE(rd->color) << 30;

	inst    = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

int naja_inst_G2SIB(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rs, scf_sib_t* sib)
{
	scf_register_t* rb   = sib->base;
	scf_register_t* ri   = sib->index;
	scf_instruction_t*    inst = NULL;

	assert(0 == sib->disp);

	if (!rb || !ri)
		return -EINVAL;

	int scale  = sib->scale;
	int size   = sib->size;

	uint32_t opcode;
	uint32_t SIZE = 0;

	if (1 == size)
		SIZE = 0;

	else if (2 == size)
		SIZE = 1;

	else if (4 == size)
		SIZE = 2;

	else if (8 == size)
		SIZE = 3;
	else
		return -EINVAL;

	opcode  = (0xe << 26) | (rs->id << 21) | (SIZE << 10) | (ri->id << 5) | rb->id;
	opcode |= SIZE << 19;
	opcode |= RISC_COLOR_TYPE(rs->color) << 26;

	inst    = risc_make_inst(c, opcode);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

int naja_inst_M2GF(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rb, scf_variable_t* vs)
{
	return naja_inst_M2G(c, f, rd, rb, vs);
}

scf_instruction_t* naja_inst_PUSH(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x7 << 26) | (r->id << 21) | (3 << 19) | 0x1e;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_POP(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x5 << 26) | (r->id << 21) | (3 << 19) | 0x1e;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_RET(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = 0x38 << 26;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MOV_SP(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xf << 26) | (rd->id << 21) | (0x1 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MOV_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xf << 26) | (rd->id << 21) | (0x1 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MVN(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xf << 26) | (rd->id << 21) | (0x3 << 19) | (0x2 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FMOV_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1f << 26) | (rd->id << 21) | (0x3 << 19) | (0x3 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MOVSX(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs, int size)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           SH;

	if (1 == size)
		SH = 0;
	else if (2 == size)
		SH = 1;
	else if (4 == size)
		SH = 2;
	else
		return NULL;

	opcode = (0xf << 26) | (rd->id << 21) | (SH << 19) | (1 << 18) | (0x2 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MOVZX(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs, int size)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           SH;

	if (1 == size)
		SH = 0;
	else if (2 == size)
		SH = 1;
	else if (4 == size)
		SH = 2;
	else
		return NULL;

	opcode = (0xf << 26) | (rd->id << 21) | (SH << 19) | (0x2 << 16)| rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTSS2SD(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           S;

	opcode = (0x1f << 26) | (rd->id << 21) | (2 << 19) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTSD2SS(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1f << 26) | (rd->id << 21) | (3 << 19) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTF2SI(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           SH;

	if (4 == rd->bytes)
		SH = 2;
	else
		SH = 3;

	opcode = (0x1f << 26) | (rd->id << 21) | (SH << 19) | (1 << 18) | (0x1 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTF2UI(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           SH;

	if (4 == rd->bytes)
		SH = 2;
	else
		SH = 3;

	opcode = (0x1f << 26) | (rd->id << 21) | (SH << 19) | (0x1 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTSI2F(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           SH;

	if (4 == rd->bytes)
		SH = 2;
	else
		SH = 3;

	opcode = (0x1f << 26) | (rd->id << 21) | (SH << 19) | (1 << 18) | (0x2 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTUI2F(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           SH;

	if (4 == rd->bytes)
		SH = 2;
	else
		SH = 3;

	opcode = (0x1f << 26) | (rd->id << 21) | (SH << 19) | (0x2 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SUB_IMM(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rs, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0x3fff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (1 << 26) | (rd->id << 21) | (3 << 19) | (imm << 5) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CMP_IMM(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rs, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0x3fff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (1 << 26) | (0x1f << 21) | (3 << 19) | (imm << 5) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_ADD_IMM(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rs, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0x3fff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (0 << 26) | (rd->id << 21) | (3 << 19) | (imm << 5) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_ADD_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0 << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SHL(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0xf << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SHR(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0xf << 26) | (rd->id << 21) | (1 << 19) | (rs1->id << 5) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_ASR(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0xf << 26) | (rd->id << 21) | (2 << 19) | (rs1->id << 5) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_AND_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x8 << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_OR_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x9 << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SUB_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1 << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CMP_G(scf_3ac_code_t* c, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1 << 26) | (0x1f << 21) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FCMP(scf_3ac_code_t* c, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x11 << 26) | (0x1f << 21) | (rs1->id << 5) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_NEG(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xf << 26) | (rd->id << 21) | (0x7 << 18) | (0x2 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_TEQ(scf_3ac_code_t* c, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x8 << 26) | (0x1f << 21) | (rs->id << 5) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FADD(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x10 << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FSUB(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x11 << 26) | (rd->id << 21) | (rs1->id << 5) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MUL(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x2 << 26) | (rd->id << 21) | (2 << 19) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FMUL(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x12 << 26) | (rd->id << 21) | (2 << 19) | (1 << 18) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FDIV(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x13 << 26) | (rd->id << 21) | (2 << 19) | (1 << 18) | (rs1->id << 5) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_DIV(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x3 << 26) | (rd->id << 21) | (2 << 19) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SDIV(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x3 << 26) | (rd->id << 21) | (2 << 19) | (1 << 18) | (rs1->id << 5) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MSUB(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rm, scf_register_t* rn, scf_register_t* ra)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x2 << 26) | (rd->id << 21) | (1 << 19) | (1 << 18) | (ra->id << 10) | (rm->id << 5) | rn->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

int naja_inst_BL(scf_3ac_code_t* c, scf_function_t* f, scf_function_t* pf)
{
	scf_instruction_t* inst;
	scf_rela_t*        rela;
	uint32_t           opcode;

	opcode = (0x1a << 26);
	inst   = risc_make_inst(c, opcode);

	RISC_INST_ADD_CHECK(c->instructions, inst);
	RISC_RELA_ADD_CHECK(f->text_relas,   rela, c, NULL, pf);

	rela->type = R_AARCH64_CALL26;
	return 0;
}

scf_instruction_t* naja_inst_BLR(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1b << 26) | (r->id << 21);
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SETZ(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           cc = 1;

	opcode = (0xc << 26) | (rd->id << 21);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETNZ(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           cc = 0;

	opcode = (0xc << 26) | (rd->id << 21) | (1 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETGT(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xc << 26) | (rd->id << 21) | (3 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETGE(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xc << 26) | (rd->id << 21) | (2 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETLT(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (rd->id << 21) | (5 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETLE(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (rd->id << 21) | (4 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JMP(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = 0xa << 26;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JZ(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JNZ(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (1 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JGT(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (3 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JGE(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (2 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JLT(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (5 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JLE(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xb << 26) | (4 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JA(scf_3ac_code_t* c)
{
	return NULL;
}
scf_instruction_t* naja_inst_JB(scf_3ac_code_t* c)
{
	return NULL;
}
scf_instruction_t* naja_inst_JAE(scf_3ac_code_t* c)
{
	return NULL;
}
scf_instruction_t* naja_inst_JBE(scf_3ac_code_t* c)
{
	return NULL;
}

void naja_set_jmp_offset(scf_instruction_t* inst, int32_t bytes)
{
	uint32_t opcode;

	opcode  = inst->code[0];
	opcode |= inst->code[1] <<  8;
	opcode |= inst->code[2] << 16;
	opcode |= inst->code[3] << 24;

	if (0xb == (opcode >> 26) && 1 == (opcode & 1)) {

		if (bytes  >= 0 && bytes < (0x1 << 20)) {
			bytes >>= 2;
			bytes <<= 5;

		} else if (bytes < 0 && bytes > -(0x1 << 20)) {

			bytes >>= 2;
			bytes  &= 0x1fffff;
			bytes <<= 5;
		} else
			assert(0);

		inst->code[0] |= 0xff &  bytes;
		inst->code[1] |= 0xff & (bytes >>  8);
		inst->code[2] |= 0xff & (bytes >> 16);
		inst->code[3] |= 0x3  & (bytes >> 24);

	} else {
		assert(0xa == (opcode >> 26));

		bytes >>= 2;

		assert(bytes < (0x1 << 26) && bytes > -(0x1 << 26));

		inst->code[0] |= 0xff &  bytes;
		inst->code[1] |= 0xff & (bytes >>  8);
		inst->code[2] |= 0xff & (bytes >> 16);
		inst->code[3] |= 0x3  & (bytes >> 24);
	}
}

int naja_cmp_update(scf_3ac_code_t* c, scf_function_t* f, scf_instruction_t* cmp)
{
	scf_instruction_t* inst;
	scf_register_t*    r16 = f->rops->find_register_type_id_bytes(0, 16, 8);
	scf_register_t*    r17 = f->rops->find_register_type_id_bytes(0, 17, 8);
	scf_register_t*    r0;

	uint32_t opcode;
	uint32_t mov;
	uint32_t i0;
	uint32_t i1;
	uint32_t SH;

	opcode  = cmp->code[0];
	opcode |= cmp->code[1] <<  8;
	opcode |= cmp->code[2] << 16;
	opcode |= cmp->code[3] << 24;

	switch (opcode >> 21) {

		case 0x3f:
			SH = (opcode >> 19) & 0x3;

			if (0x3 == SH) {
				i0   = opcode & 0x1f;
				r0   = f->rops->find_register_type_id_bytes(0, i0, 8);
				inst = f->iops->MOV_G(c, r16, r0);  // use r16 to backup r0
				RISC_INST_ADD_CHECK(c->instructions, inst);

				opcode &= ~0x1f;
				opcode |=  0x10;
			} else {
				i0   =  opcode & 0x1f;
				i1   = (opcode >> 5) & 0x1f;

				r0   = f->rops->find_register_type_id_bytes(0, i0, 8);
				inst = f->iops->MOV_G(c, r16, r0);  // use r16 to backup r0
				RISC_INST_ADD_CHECK(c->instructions, inst);

				r0   = f->rops->find_register_type_id_bytes(0, i1, 8);
				inst = f->iops->MOV_G(c, r17, r0);  // use r17 to backup r1
				RISC_INST_ADD_CHECK(c->instructions, inst);

				opcode &= ~0x1f;
				opcode |=  0x10;

				opcode &= ~(0x1f << 5);
				opcode |=  (0x11 << 5);
			}
			break;
		default:
			scf_loge("%#x\n", opcode);
			return -EINVAL;
			break;
	};

	cmp->code[0] = 0xff &  opcode;
	cmp->code[1] = 0xff & (opcode >>  8);
	cmp->code[2] = 0xff & (opcode >> 16);
	cmp->code[3] = 0xff & (opcode >> 24);
	return 0;
}


scf_inst_ops_t  inst_ops_naja =
{
	.name      = "naja",

	.BL        = naja_inst_BL,
	.BLR       = naja_inst_BLR,
	.PUSH      = naja_inst_PUSH,
	.POP       = naja_inst_POP,
	.TEQ       = naja_inst_TEQ,
	.NEG       = naja_inst_NEG,

	.MOVZX     = naja_inst_MOVZX,
	.MOVSX     = naja_inst_MOVSX,
	.MVN       = naja_inst_MVN,
	.MOV_G     = naja_inst_MOV_G,
	.MOV_SP    = naja_inst_MOV_SP,

	.ADD_G     = naja_inst_ADD_G,
	.ADD_IMM   = naja_inst_ADD_IMM,
	.SUB_G     = naja_inst_SUB_G,
	.SUB_IMM   = naja_inst_SUB_IMM,
	.CMP_G     = naja_inst_CMP_G,
	.CMP_IMM   = naja_inst_CMP_IMM,
	.AND_G     = naja_inst_AND_G,
	.OR_G      = naja_inst_OR_G,

	.MUL       = naja_inst_MUL,
	.DIV       = naja_inst_DIV,
	.SDIV      = naja_inst_SDIV,
	.MSUB      = naja_inst_MSUB,

	.SHL       = naja_inst_SHL,
	.SHR       = naja_inst_SHR,
	.ASR       = naja_inst_ASR,

	.CVTSS2SD  = naja_inst_CVTSS2SD,
	.CVTSD2SS  = naja_inst_CVTSD2SS,
	.CVTF2SI   = naja_inst_CVTF2SI,
	.CVTF2UI   = naja_inst_CVTF2UI,
	.CVTSI2F   = naja_inst_CVTSI2F,
	.CVTUI2F   = naja_inst_CVTUI2F,

	.FCMP      = naja_inst_FCMP,
	.FADD      = naja_inst_FADD,
	.FSUB      = naja_inst_FSUB,
	.FMUL      = naja_inst_FMUL,
	.FDIV      = naja_inst_FDIV,
	.FMOV_G    = naja_inst_FMOV_G,

	.JA        = naja_inst_JA,
	.JB        = naja_inst_JB,
	.JZ        = naja_inst_JZ,
	.JNZ       = naja_inst_JNZ,
	.JGT       = naja_inst_JGT,
	.JGE       = naja_inst_JGE,
	.JLT       = naja_inst_JLT,
	.JLE       = naja_inst_JLE,
	.JAE       = naja_inst_JAE,
	.JBE       = naja_inst_JBE,
	.JMP       = naja_inst_JMP,
	.RET       = naja_inst_RET,

	.SETZ      = naja_inst_SETZ,
	.SETNZ     = naja_inst_SETNZ,
	.SETGT     = naja_inst_SETGT,
	.SETGE     = naja_inst_SETGE,
	.SETLT     = naja_inst_SETLT,
	.SETLE     = naja_inst_SETLE,

	.I2G       = naja_inst_I2G,
	.M2G       = naja_inst_M2G,
	.M2GF      = naja_inst_M2GF,
	.G2M       = naja_inst_G2M,
	.G2P       = naja_inst_G2P,
	.P2G       = naja_inst_P2G,
	.ISTR2G    = naja_inst_ISTR2G,
	.SIB2G     = naja_inst_SIB2G,
	.G2SIB     = naja_inst_G2SIB,
	.ADR2G     = naja_inst_ADR2G,
	.ADRP2G    = naja_inst_ADRP2G,
	.ADRSIB2G  = naja_inst_ADRSIB2G,

	.set_jmp_offset = naja_set_jmp_offset,
	.cmp_update     = naja_cmp_update,
};
