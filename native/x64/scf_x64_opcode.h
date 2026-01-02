#ifndef SCF_X64_OPCODE_H
#define SCF_X64_OPCODE_H

#include"scf_native.h"
#include"scf_x64_util.h"
#include"scf_elf.h"

typedef struct {
	int			type;

	char*		name;

	int			len;

	uint8_t		OpCodes[3];
	int			nb_OpCodes;

	// RegBytes only valid for immediate
	// same to OpBytes for E2G or G2E
	int			OpBytes;
	int			RegBytes;
	int			EG;

	uint8_t		ModRM_OpCode;
	int			ModRM_OpCode_used;

	int         nb_regs;
	uint32_t    regs[2];
} scf_x64_OpCode_t;


scf_x64_OpCode_t*   x64_find_OpCode        (const int   type, const int OpBytes, const int RegBytes, const int EG);
scf_x64_OpCode_t*   x64_find_OpCode_by_type(const int   type);
scf_x64_OpCode_t*   x64_find_OpCode_by_name(const char* name);

scf_instruction_t*  x64_make_inst  (scf_x64_OpCode_t* OpCode, int size);
scf_instruction_t*  x64_make_inst_G(scf_x64_OpCode_t* OpCode, scf_register_t* r);
scf_instruction_t*  x64_make_inst_E(scf_x64_OpCode_t* OpCode, scf_register_t* r);
scf_instruction_t*  x64_make_inst_I(scf_x64_OpCode_t* OpCode, uint8_t* imm, int size);
void                x64_make_inst_I2(scf_instruction_t* inst, scf_x64_OpCode_t* OpCode, uint8_t* imm, int size);

scf_instruction_t*  x64_make_inst_I2G(scf_x64_OpCode_t* OpCode, scf_register_t* r_dst, uint8_t* imm, int size);
scf_instruction_t*  x64_make_inst_I2E(scf_x64_OpCode_t* OpCode, scf_register_t* r_dst, uint8_t* imm, int size);

scf_instruction_t*  x64_make_inst_G2E(scf_x64_OpCode_t* OpCode, scf_register_t* r_dst, scf_register_t* r_src);
scf_instruction_t*  x64_make_inst_E2G(scf_x64_OpCode_t* OpCode, scf_register_t* r_dst, scf_register_t* r_src);

scf_instruction_t*  x64_make_inst_P2G(scf_x64_OpCode_t* OpCode, scf_register_t* r_dst,  scf_register_t* r_base, int32_t offset);
scf_instruction_t*  x64_make_inst_G2P(scf_x64_OpCode_t* OpCode, scf_register_t* r_base, int32_t offset, scf_register_t* r_src);
scf_instruction_t*  x64_make_inst_I2P(scf_x64_OpCode_t* OpCode, scf_register_t* r_base, int32_t offset, uint8_t* imm, int size);

scf_instruction_t*  x64_make_inst_SIB2G(scf_x64_OpCode_t* OpCode, scf_register_t* r_dst,  scf_register_t* r_base,  scf_register_t* r_index, int32_t scale, int32_t disp);
scf_instruction_t*  x64_make_inst_G2SIB(scf_x64_OpCode_t* OpCode, scf_register_t* r_base, scf_register_t* r_index, int32_t scale, int32_t disp, scf_register_t* r_src);
scf_instruction_t*  x64_make_inst_I2SIB(scf_x64_OpCode_t* OpCode, scf_register_t* r_base, scf_register_t* r_index, int32_t scale, int32_t disp, uint8_t* imm, int32_t size);

scf_instruction_t*  x64_make_inst_SIB(scf_x64_OpCode_t* OpCode, scf_register_t* r_base, scf_register_t* r_index, int32_t scale, int32_t disp, int size);
scf_instruction_t*  x64_make_inst_P  (scf_x64_OpCode_t* OpCode, scf_register_t* r_base, int32_t offset, int size);

scf_instruction_t*  x64_make_inst_L  (scf_rela_t** prela, scf_x64_OpCode_t* OpCode);
scf_instruction_t*  x64_make_inst_I2L(scf_rela_t** prela, scf_x64_OpCode_t* OpCode, uint8_t* imm, int32_t size);
scf_instruction_t*  x64_make_inst_G2L(scf_rela_t** prela, scf_x64_OpCode_t* OpCode, scf_register_t* r_src);
scf_instruction_t*  x64_make_inst_L2G(scf_rela_t** prela, scf_x64_OpCode_t* OpCode, scf_register_t* r_dst);

#define X64_INST_ADD_CHECK(vec, inst) \
			do { \
				if (!(inst)) { \
					scf_loge("\n"); \
					return -ENOMEM; \
				} \
				int ret = scf_vector_add((vec), (inst)); \
				if (ret < 0) { \
					scf_loge("\n"); \
					free(inst); \
					return ret; \
				} \
			} while (0)

#define X64_RELA_ADD_CHECK(vec, rela, c, v, f) \
	do { \
		if (rela) { \
			(rela)->code = (c); \
			(rela)->var  = (v); \
			(rela)->func = (f); \
			(rela)->inst = (c)->instructions->data[(c)->instructions->size - 1]; \
			(rela)->addend = -4; \
			(rela)->type = R_X86_64_PC32; \
			int ret = scf_vector_add((vec), (rela)); \
			if (ret < 0) { \
				scf_loge("\n"); \
				free(rela); \
				return ret; \
			} \
		} \
	} while (0)

#define X64_RELA_ADD_LABEL(vec, rela, _inst, _label) \
	do { \
		if (rela) { \
			(rela)->inst  = (_inst); \
			(rela)->addend = -4; \
			(rela)->type = R_X86_64_PC32; \
			(rela)->name = scf_string_clone(_label); \
			if (!(rela)->name) { \
				scf_loge("\n"); \
				scf_rela_free(rela); \
				return -ENOMEM; \
			} \
			int ret = scf_vector_add((vec), (rela)); \
			if (ret < 0) { \
				scf_loge("\n"); \
				scf_rela_free(rela); \
				return ret; \
			} \
		} \
	} while (0)

#endif
