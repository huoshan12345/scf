#include"scf_risc.h"

static uint32_t naja_shift(int bytes)
{
	if (bytes <= 1)
		return 0;
	else if (bytes <= 2)
		return 1;
	else if (bytes <= 4)
		return 2;
	return 3;
}

scf_instruction_t* __naja_inst_ADRP(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t*  inst;
	uint32_t            opcode;

	opcode = (0x35 << 26) | (rd->id << 21);

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_ADRP, 8,8, SCF_RISC_E2G);
		inst->dst.base = rd;
		inst->src.flag = 1;
	}

	return inst;
}

scf_instruction_t* __naja_inst_SIB2G(scf_3ac_code_t* c, scf_register_t* rd, scf_sib_t* sib)
{
	scf_register_t*     rb   = sib->base;
	scf_register_t*     ri   = sib->index;
	scf_instruction_t*  inst = NULL;

	assert(0 == sib->disp);

	if (!rb || !ri)
		return NULL;

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
		return NULL;

	opcode  = (0xa << 26) | (rd->id << 21) | (SIZE << 10) | (ri->id << 4) | rb->id;
	opcode |= SIZE << 18;
	opcode |= RISC_COLOR_TYPE(rd->color) << 30;

	inst = risc_make_inst(c, opcode);
	return inst;
}

scf_instruction_t* __naja_inst_G2SIB(scf_3ac_code_t* c, scf_register_t* rs, scf_sib_t* sib)
{
	scf_register_t*     rb   = sib->base;
	scf_register_t*     ri   = sib->index;
	scf_instruction_t*  inst = NULL;

	assert(0 == sib->disp);

	if (!rb || !ri)
		return NULL;

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
		return NULL;

	opcode  = (0xb << 26) | (rs->id << 21) | (SIZE << 10) | (ri->id << 4) | rb->id;
	opcode |= SIZE << 18;
	opcode |= RISC_COLOR_TYPE(rs->color) << 16;

	inst = risc_make_inst(c, opcode);
	return inst;
}

scf_instruction_t* naja_inst_PUSH(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0x9 << 26) | (r->id << 21) | (3 << 18) | 0xe;
	opcode |= RISC_COLOR_TYPE(r->color) << 17;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_PUSH, 8,8, SCF_RISC_G);
		inst->src.base = r;
	}

	return inst;
}

scf_instruction_t* naja_inst_POP(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0x8 << 26) | (r->id << 21) | (3 << 18) | 0xe;
	opcode |= RISC_COLOR_TYPE(r->color) << 17;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_POP, 8,8, SCF_RISC_G);
		inst->dst.base = r;
	}

	return inst;
}

scf_instruction_t* naja_inst_RET(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = 0x36 << 26;

	inst = risc_make_inst(c, opcode);
	if (inst)
		inst->OpCode = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_RET, 8,8, SCF_RISC_G);

	return inst;
}

scf_instruction_t* naja_inst_MOV_SP(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xc << 26) | (rd->id << 21) | (3 << 18) | (0xf << 4) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MOV_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xc << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (0xf << 4) | rs->id;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_MOV, 8,8, SCF_RISC_G2E);
		inst->dst.base = rd;
		inst->src.base = rs;
	}

	return inst;
}

scf_instruction_t* naja_inst_MOV_IMM(scf_3ac_code_t* c, scf_register_t* rd, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0xffff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (0xe << 26) | (rd->id << 21) | (0x2 << 16) | (imm & 0xffff);

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_MOV, 2,2, SCF_RISC_I2G);
		inst->dst.base = rd;
		inst->src.imm  = imm;
		inst->src.imm_size = 2;
	}

	return inst;
}

scf_instruction_t* naja_inst_MVN(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xe << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (1 << 16) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FMOV_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1c << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | rs->id;
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

	opcode = (0xd << 26) | (rd->id << 21) | (SH << 18) | (1 << 17) | rs->id;
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

	opcode = (0xd << 26) | (rd->id << 21) | (SH << 18) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTSS2SD(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           S;

	opcode = (0x1d << 26) | (rd->id << 21) | (3 << 18) | (1 << 12) | (1 << 6) | (2 << 4) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CVTSD2SS(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x1d << 26) | (rd->id << 21) | (2 << 18) | (1 << 12) | (1 << 6) | (3 << 4) | rs->id;
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

	opcode = (0x1d << 26) | (rd->id << 21) | (SH << 18) | (3 << 12) | (1 << 6) | (naja_shift(rs->bytes) << 4) | rs->id;
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

	opcode = (0x1d << 26) | (rd->id << 21) | (SH << 18) | (2 << 12) | (1 << 6) | (naja_shift(rs->bytes) << 4) | rs->id;
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

	opcode = (0x1d << 26) | (rd->id << 21) | (SH << 18) | (1 << 12) | (3 << 6) | (naja_shift(rs->bytes) << 4) | rs->id;
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

	opcode = (0x1d << 26) | (rd->id << 21) | (SH << 18) | (1 << 12) | (2 << 6) | (naja_shift(rs->bytes) << 4) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SUB_IMM(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rs, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0xfff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (1 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (3 << 16) | (imm << 4) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CMP_IMM(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rs, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0xfff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (1 << 26) | (0xf << 21) | (naja_shift(rs->bytes) << 18) | (3 << 16) | (imm << 4) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_ADD_IMM(scf_3ac_code_t* c, scf_function_t* f, scf_register_t* rd, scf_register_t* rs, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0xfff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (0 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (3 << 16) | (imm << 4) | rs->id;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_ADD, 8,8, SCF_RISC_E2G);
		inst->dst.base = rd;
		inst->src.base = rs;

		inst->srcs[0].imm = imm;
		inst->srcs[0].imm_size = 2;
	}

	return inst;
}

scf_instruction_t* naja_inst_ADD_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode    = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_ADD, 8,8, SCF_RISC_E2G);
		inst->dst.base  = rd;
		inst->src.base  = rs0;

		inst->srcs[0].base = rs1;
	}

	return inst;
}

scf_instruction_t* naja_inst_SHL(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0xc << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SHR(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0xc << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (1 << 16) | (rs1->id << 4) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_ASR(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode  = (0xc << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (2 << 16) | (rs1->id << 4) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_AND_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x5 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_OR_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x7 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SUB_G(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (1 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_CMP_G(scf_3ac_code_t* c, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (1 << 26) | (0xf << 21) | (naja_shift(rs0->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FCMP(scf_3ac_code_t* c, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x11 << 26) | (0xf << 21) | (naja_shift(rs0->bytes) << 18) | (1 << 16) | (rs1->id << 4) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_NEG(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0xe << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_TEQ(scf_3ac_code_t* c, scf_register_t* rs)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x5 << 26) | (0xf << 21) | (naja_shift(rs->bytes) << 18) | (rs->id << 4) | rs->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FADD(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x10 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FSUB(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x11 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MUL(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (2 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (0xf << 12) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FMUL(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x12 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (0xf << 12) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_FDIV(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x13 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst    = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_DIV(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (3 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_SDIV(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rs0, scf_register_t* rs1)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (3 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (1 << 17) | (rs1->id << 4) | rs0->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_MSUB(scf_3ac_code_t* c, scf_register_t* rd, scf_register_t* rm, scf_register_t* rn, scf_register_t* ra)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (2 << 26) | (rd->id << 21) | (naja_shift(rd->bytes) << 18) | (1 << 16) | (ra->id << 12) | (rm->id << 4) | rn->id;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_BR(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | (r->id << 21);

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_JMP, 8,8, SCF_RISC_E);
		inst->dst.base = r;
	}

	return inst;
}

scf_instruction_t* naja_inst_BLR(scf_3ac_code_t* c, scf_register_t* r)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x33 << 26) | (r->id << 21);

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode   = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_CALL, 8,8, SCF_RISC_E);
		inst->dst.base = r;
	}

	return inst;
}

scf_instruction_t* __naja_inst_BL(scf_3ac_code_t* c, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0x3ffffff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = (0x31 << 26) | imm;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode  = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_CALL, 4,4, SCF_RISC_I);
		inst->dst.imm = imm;
		inst->dst.imm_size = 4;
	}

	return inst;
}

scf_instruction_t* __naja_inst_JMP(scf_3ac_code_t* c, uint64_t imm)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	if (imm > 0x3ffffff) {
		scf_loge("NOT support too big imm: %#lx\n", imm);
		return NULL;
	}

	opcode = 0x30 << 26 | imm;

	inst = risc_make_inst(c, opcode);
	if (inst) {
		inst->OpCode  = (scf_OpCode_t*)risc_find_OpCode(SCF_RISC_JMP, 4,4, SCF_RISC_I);
		inst->dst.imm = imm;
		inst->dst.imm_size = 4;
	}

	return inst;
}

scf_instruction_t* naja_inst_SETZ(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           cc = 1;

	opcode = (0x34 << 26) | (rd->id << 21);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETNZ(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;
	uint32_t           cc = 0;

	opcode = (0x34 << 26) | (rd->id << 21) | (1 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETGT(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x34 << 26) | (rd->id << 21) | (3 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETGE(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x34 << 26) | (rd->id << 21) | (2 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETLT(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x34 << 26) | (rd->id << 21) | (5 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}
scf_instruction_t* naja_inst_SETLE(scf_3ac_code_t* c, scf_register_t* rd)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x34 << 26) | (rd->id << 21) | (4 << 1);
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JMP(scf_3ac_code_t* c)
{
	return __naja_inst_JMP(c, 0);
}

scf_instruction_t* naja_inst_JZ(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JNZ(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | (1 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JGT(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | (3 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JGE(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | (2 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JLT(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | (5 << 1) | 1;
	inst   = risc_make_inst(c, opcode);

	return inst;
}

scf_instruction_t* naja_inst_JLE(scf_3ac_code_t* c)
{
	scf_instruction_t* inst;
	uint32_t           opcode;

	opcode = (0x32 << 26) | (4 << 1) | 1;
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

	if (0x32 == (opcode >> 26) && 1 == (opcode & 1)) {

		if (bytes  >= 0 && bytes < (0x1 << 20)) {
			bytes >>= 2;
			bytes <<= 5;

		} else if (bytes < 0 && bytes > -(0x1 << 20)) {

			bytes >>= 2;
			bytes  &= 0x1fffff;
			bytes <<= 5;
		} else
			assert(0);

		opcode &= ~0x1ffffe0;
		opcode |= bytes;

		inst->code[0] = 0xff &  opcode;
		inst->code[1] = 0xff & (opcode >>  8);
		inst->code[2] = 0xff & (opcode >> 16);
		inst->code[3] = 0xff & (opcode >> 24);

	} else if (0x30 == (opcode >> 26)
			|| 0x31 == (opcode >> 26)) {

		bytes >>= 2;

		assert(bytes < (0x1 << 26) && bytes > -(0x1 << 26));

		bytes  &=  0x3ffffff;
		opcode &= ~0x3ffffff;
		opcode |= bytes;

		inst->code[0] = 0xff &  opcode;
		inst->code[1] = 0xff & (opcode >>  8);
		inst->code[2] = 0xff & (opcode >> 16);
		inst->code[3] = 0xff & (opcode >> 24);
	}
}
