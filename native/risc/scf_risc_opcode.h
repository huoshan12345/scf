#ifndef SCF_RISC_OPCODE_H
#define SCF_RISC_OPCODE_H

#include"scf_native.h"
#include"scf_risc_util.h"

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
} scf_risc_OpCode_t;

scf_risc_OpCode_t*  risc_find_OpCode_by_name(const char* name);
scf_risc_OpCode_t*  risc_find_OpCode_by_type(const int   type);
scf_risc_OpCode_t*  risc_find_OpCode        (const int   type, const int OpBytes, const int RegBytes, const int EG);

scf_instruction_t*  risc_make_inst(scf_3ac_code_t* c, uint32_t opcode);

#define RISC_INST_ADD_CHECK(vec, inst) \
			do { \
				if (!(inst)) { \
					scf_loge("\n"); \
					return -ENOMEM; \
				} \
				int ret = scf_vector_add((vec), (inst)); \
				if (ret < 0) { \
					scf_loge("\n"); \
					scf_instruction_free(inst); \
					return ret; \
				} \
			} while (0)

#define RISC_RELA_ADD_CHECK(vec, rela, c, v, f) \
	do { \
		rela = calloc(1, sizeof(scf_rela_t)); \
		if (!rela) \
			return -ENOMEM; \
		\
		(rela)->code = (c); \
		(rela)->var  = (v); \
		(rela)->func = (f); \
		(rela)->inst = (c)->instructions->data[(c)->instructions->size - 1]; \
		(rela)->addend = 0; \
		\
		int ret = scf_vector_add((vec), (rela)); \
		if (ret < 0) { \
			scf_rela_free(rela); \
			return ret; \
		} \
	} while (0)

#define RISC_RELA_ADD_LABEL(vec, rela, _inst, _label) \
	do { \
		rela = calloc(1, sizeof(scf_rela_t)); \
		if (!rela) \
			return -ENOMEM; \
		\
		(rela)->inst  = (_inst); \
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
	} while (0)

#endif
