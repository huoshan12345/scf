#include"scf_x64.h"
#include"scf_elf.h"
#include"scf_basic_block.h"
#include"scf_3ac.h"

static int _x64_peephole_common(scf_vector_t* std_insts, scf_instruction_t* inst)
{
	scf_3ac_code_t*    c  = inst->c;
	scf_basic_block_t* bb = c->basic_block;

	scf_instruction_t* inst2;
	scf_instruction_t* std;
	scf_x64_OpCode_t*  OpCode;

	int j;
	for (j  = std_insts->size - 1; j >= 0; j--) {
		std = std_insts->data[j];
#if 0
		scf_loge("std j: %d\n", j);
		scf_3ac_code_print(std->c, NULL);
		scf_instruction_print(std);

		scf_loge("inst: \n");
		scf_instruction_print(inst);
		printf("\n");
#endif
		if (scf_inst_data_same(&std->dst, &inst->dst)) {

			if (scf_inst_data_same(&std->src, &inst->src)) {

				assert(0 == scf_vector_del(inst->c->instructions, inst));

				free(inst);
				inst = NULL;
				return X64_PEEPHOLE_DEL;
			}

			assert(0 == scf_vector_del(std_insts, std));

			if (std->nb_used > 0)
				continue;

			assert(0 == scf_vector_del(std->c->instructions, std));

			free(std);
			std = NULL;
			continue;
		} else if (scf_inst_data_same(&std->src, &inst->src)) {
			continue;
		} else if (scf_inst_data_same(&std->dst, &inst->src)) {

			std->nb_used++;

			if (scf_inst_data_same(&std->src, &inst->dst)) {

				assert(0 == scf_vector_del(inst->c->instructions, inst));

				free(inst);
				inst = NULL;
				return X64_PEEPHOLE_DEL;
			}

			if (inst->src.flag) {
				assert(std->dst.flag);

				if (std->src.base)
					inst2  = x64_make_inst_E2G((scf_x64_OpCode_t*)inst->OpCode, inst->dst.base, std->src.base);
				else {
					OpCode = x64_find_OpCode(inst->OpCode->type, std->src.imm_size, inst->dst.base->bytes, SCF_X64_I2G);

					inst2  = x64_make_inst_I2G(OpCode, inst->dst.base, (uint8_t*)&std->src.imm, std->src.imm_size);
				}
				if (!inst2)
					return -ENOMEM;

				memcpy(inst->code, inst2->code, inst2->len);
				inst->len = inst2->len;

				inst->src.base  = std->src.base;
				inst->src.index = NULL;
				inst->src.scale = 0;
				inst->src.disp  = 0;
				inst->src.flag  = 0;

				free(inst2);
				inst2 = NULL;
				continue;

			} else if (inst->dst.flag) {

				if (x64_inst_data_is_reg(&inst->src)
						&& x64_inst_data_is_reg(&std->src)
						&& inst->src.base != std->src.base) {

					if (!inst->dst.index)
						inst2 = x64_make_inst_G2P((scf_x64_OpCode_t*)inst->OpCode,
								inst->dst.base, inst->dst.disp,
								std->src.base);
					else
						inst2 = x64_make_inst_G2SIB((scf_x64_OpCode_t*)inst->OpCode,
								inst->dst.base,
								inst->dst.index, inst->dst.scale, inst->dst.disp,
								std->src.base);
					if (!inst2)
						return -ENOMEM;

					memcpy(inst->code, inst2->code, inst2->len);
					inst->len = inst2->len;

					inst->src.base  = std->src.base;
					inst->src.index = NULL;
					inst->src.scale = 0;
					inst->src.disp  = 0;
					inst->src.flag  = 0;

					free(inst2);
					inst2 = NULL;
					continue;
				}
			}
		} else if (scf_inst_data_same(&std->src, &inst->dst)) {

			assert(0 == scf_vector_del(std_insts, std));

		} else if (x64_inst_data_is_reg(&std->src)) {

			scf_register_t* r0;
			scf_register_t* r1;

			if (x64_inst_data_is_reg(&inst->dst)) {

				r0 = std ->src.base;
				r1 = inst->dst.base;

				if (X64_COLOR_CONFLICT(r0->color, r1->color))
					assert(0 == scf_vector_del(std_insts, std));
			}

		} else if (x64_inst_data_is_reg(&std->dst)) {

			if (inst->src.base == std->dst.base
					|| inst->src.index == std->dst.base
					|| inst->dst.base  == std->dst.base
					|| inst->dst.index == std->dst.base)
				std->nb_used++;
		}
	}

	assert(0 == scf_vector_add_unique(std_insts, inst));
	return 0;
}

static int _x64_peephole_cmp(scf_vector_t* std_insts, scf_instruction_t* inst)
{
	scf_3ac_code_t*    c  = inst->c;
	scf_basic_block_t* bb = c->basic_block;

	scf_instruction_t* inst2;
	scf_instruction_t* std;

	int j;
	for (j  = std_insts->size - 1; j >= 0; j--) {
		std = std_insts->data[j];

		if (inst->src.flag) {

			if (scf_inst_data_same(&inst->src, &std->src))

				inst->src.base = std->dst.base;

			else if (scf_inst_data_same(&inst->src, &std->dst))

				inst->src.base = std->src.base;
			else
				goto check;

			inst2 = x64_make_inst_E2G((scf_x64_OpCode_t*) inst->OpCode, inst->dst.base, inst->src.base);
			if (!inst2)
				return -ENOMEM;

			inst->src.index = NULL;
			inst->src.scale = 0;
			inst->src.disp  = 0;
			inst->src.flag  = 0;

		} else if (inst->dst.flag) {

			if (scf_inst_data_same(&inst->dst, &std->src))

				inst->dst.base  = std->dst.base;

			else if (scf_inst_data_same(&inst->dst, &std->dst))

				inst->dst.base  = std->src.base;
			else
				goto check;

			if (inst->src.imm_size > 0)
				inst2 = x64_make_inst_I2E((scf_x64_OpCode_t*)inst->OpCode,
						inst->dst.base,
						(uint8_t*)&inst->src.imm, inst->src.imm_size);
			else
				inst2 = x64_make_inst_G2E((scf_x64_OpCode_t*)inst->OpCode,
						inst->dst.base,
						inst->src.base);
			if (!inst2)
				return -ENOMEM;

			inst->dst.index = NULL;
			inst->dst.scale = 0;
			inst->dst.disp  = 0;
			inst->dst.flag  = 0;
		} else
			goto check;

		memcpy(inst->code, inst2->code, inst2->len);
		inst->len = inst2->len;

		free(inst2);
		inst2 = NULL;

check:
		if (x64_inst_data_is_reg(&std->dst)) {

			if (inst->src.base == std->dst.base
					|| inst->src.index == std->dst.base
					|| inst->dst.index == std->dst.base
					|| inst->dst.base  == std->dst.base)
				std->nb_used++;
		}

		if (scf_inst_data_same(&inst->src, &std->dst)
		 || scf_inst_data_same(&inst->dst, &std->dst)) {

			assert(0 == scf_vector_del(std_insts, std));
		}
	}

	return 0;
}

static void _x64_peephole_function(scf_vector_t* tmp_insts, scf_function_t* f, int jmp_back_flag)
{
	scf_register_t* rax = x64_find_register("rax");
	scf_register_t* rsp = x64_find_register("rsp");
	scf_register_t* rbp = x64_find_register("rbp");

	scf_register_t* rdi = x64_find_register("rdi");
	scf_register_t* rsi = x64_find_register("rsi");
	scf_register_t* rdx = x64_find_register("rdx");
	scf_register_t* rcx = x64_find_register("rcx");
	scf_register_t* r8  = x64_find_register("r8");
	scf_register_t* r9  = x64_find_register("r9");

	scf_instruction_t*  inst;
	scf_instruction_t*  inst2;
	scf_basic_block_t*  bb;
	scf_3ac_code_t*     c;
	scf_list_t*         l;

	int i;
	int j;
	int k;

	for (i   = tmp_insts->size - 1; i >= 0; i--) {
		inst = tmp_insts->data[i];

		scf_register_t* r0;
		scf_register_t* r1;
		scf_register_t* r2;

		if (!inst)
			continue;

		if (SCF_X64_MOV != inst->OpCode->type)
			continue;

		if (x64_inst_data_is_reg(&inst->dst)) {

			r0 = inst->dst.base;

			for (k = 0; k < X64_ABI_RET_NB; k++) {
				r2 = x64_find_register_type_id_bytes(0, x64_abi_ret_regs[k], 8);

				if (X64_COLOR_CONFLICT(r2->color, r0->color))
					break;
			}

			if (k < X64_ABI_RET_NB)
				continue;

		} else if (!x64_inst_data_is_local(&inst->dst))
			continue;

		if (jmp_back_flag)
			j = 0;
		else
			j = i + 1;

		for ( ; j < tmp_insts->size; j++) {
			inst2 = tmp_insts->data[j];

			if (!inst2 || inst == inst2)
				continue;

			if (scf_inst_data_same(&inst->dst, &inst2->src))
				break;

			if (x64_inst_data_is_reg(&inst->dst)) {

				r0 = inst ->dst.base;
				r1 = inst2->src.base;

				if (SCF_X64_CALL == inst2->OpCode->type) {

					if (X64_COLOR_CONFLICT(r0->color, rdi->color)
							|| X64_COLOR_CONFLICT(r0->color, rsi->color)
							|| X64_COLOR_CONFLICT(r0->color, rdx->color)
							|| X64_COLOR_CONFLICT(r0->color, rcx->color)
							|| X64_COLOR_CONFLICT(r0->color, r8->color)
							|| X64_COLOR_CONFLICT(r0->color, r9->color))
						break;

				} else {
					if (x64_inst_data_is_reg(&inst2->src)) {
						if (X64_COLOR_CONFLICT(r0->color, r1->color))
							break;
					}

					if (inst2->src.base  == inst->dst.base
							|| inst2->src.index == inst->dst.base
							|| inst2->dst.index == inst->dst.base
							|| inst2->dst.base  == inst->dst.base)
						break;
				}

			} else if (x64_inst_data_is_local(&inst->dst)) {

				if (scf_inst_data_same(&inst->dst, &inst2->src))
					break;
				else if (rsp == inst->dst.base)
					break;
				else if (SCF_OP_VA_START == inst->c->op->type
						|| SCF_OP_VA_ARG == inst->c->op->type
						|| SCF_OP_VA_END == inst->c->op->type)
					break;

				else if (SCF_X64_CMP    == inst2->OpCode->type
						|| SCF_X64_TEST == inst2->OpCode->type) {

					if (scf_inst_data_same(&inst->dst, &inst2->dst))
						break;

				} else if (SCF_X64_LEA  == inst2->OpCode->type
						|| SCF_X64_MOV  == inst2->OpCode->type) {

					if (inst2->src.base == inst->dst.base
							&& inst2->src.index) // maybe array member
						break;
				}
			}
		}

		if (j < tmp_insts->size)
			continue;

		c  = inst->c;

		assert(0 == scf_vector_del(c->instructions,  inst));
		assert(0 == scf_vector_del(tmp_insts,        inst));

//		scf_logd("del: \n");
//		scf_instruction_print(inst);

		free(inst);
		inst = NULL;
	}

	int nb_locals = 0;

	for (i = 0; i < tmp_insts->size; i++) {
		inst      = tmp_insts->data[i];

		if (!inst)
			continue;

		if (x64_inst_data_is_local(&inst->src)
				|| x64_inst_data_is_local(&inst->dst))
			nb_locals++;
	}

	if (nb_locals > 0)
		f->bp_used_flag = 1;
	else
		f->bp_used_flag = 0;

	scf_logw("%s(), f->bp_used_flag: %d\n", f->node.w->text->data, f->bp_used_flag);
}

int x64_optimize_peephole(scf_native_t* ctx, scf_function_t* f)
{
	scf_instruction_t*  std;
	scf_instruction_t*  inst;
	scf_basic_block_t*  bb;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_list_t*         l;
	scf_list_t*         l2;

	scf_vector_t*       std_insts;
	scf_vector_t*       tmp_insts; // instructions for register or local variable

	std_insts = scf_vector_alloc();
	if (!std_insts)
		return -ENOMEM;

	tmp_insts = scf_vector_alloc();
	if (!tmp_insts) {
		scf_vector_free(tmp_insts);
		return -ENOMEM;
	}

	int jmp_back_flag = 0;

	int ret = 0;
	int i;
	int j;

	for (l = scf_list_head(&f->basic_block_list_head); l != scf_list_sentinel(&f->basic_block_list_head);
		l  = scf_list_next(l)) {

		bb = scf_list_data(l, scf_basic_block_t, list);

		if (bb->jmp_flag) {
			scf_vector_clear(std_insts, NULL);

			ret = scf_vector_add(tmp_insts, NULL);
			if (ret < 0)
				goto error;

			l2 = scf_list_head(&bb->code_list_head);
			c  = scf_list_data(l2, scf_3ac_code_t, list);

			assert(1 == c->dsts->size);
			dst = c->dsts->data[0];

			if (dst->bb->index < bb->index)
				jmp_back_flag = 1;
			continue;
		}

		if (bb->jmp_dst_flag) {
			scf_vector_clear(std_insts, NULL);

			ret = scf_vector_add(tmp_insts, NULL);
			if (ret < 0)
				goto error;
		}

		for (l2 = scf_list_head(&bb->code_list_head); l2 != scf_list_sentinel(&bb->code_list_head);
			l2  = scf_list_next(l2)) {

			c   = scf_list_data(l2, scf_3ac_code_t, list);

			if (!c->instructions)
				continue;

			for (i = 0; i < c->instructions->size; ) {
				inst      = c->instructions->data[i];

				assert(inst->OpCode);

				inst->c = c;
//				scf_instruction_print(inst);

				if (SCF_X64_CMP == inst->OpCode->type || SCF_X64_TEST == inst->OpCode->type) {

					ret = _x64_peephole_cmp(std_insts, inst);
					if (ret < 0)
						goto error;

				} else if (SCF_X64_MOV == inst->OpCode->type) {

					ret = _x64_peephole_common(std_insts, inst);
					if (ret < 0)
						goto error;

					if (X64_PEEPHOLE_DEL == ret)
						continue;

				} else {
					scf_vector_clear(std_insts, NULL);
				}

				ret = scf_vector_add(tmp_insts, inst);
				if (ret < 0)
					goto error;
				i++;
			}
		}
	}

	_x64_peephole_function(tmp_insts, f, jmp_back_flag);
#if 0
	for (i = 0; i < tmp_insts->size; i++) {
		inst      = tmp_insts->data[i];

		if (!inst)
			continue;

		if (SCF_X64_MOV != inst->OpCode->type
				|| !x64_inst_data_is_local(&inst->src))
			continue;

		scf_logw("\n");
		scf_instruction_print(inst);

		for (j = i - 1; j >= 0; j--) {
			std = tmp_insts->data[j];

			if (!std)
				break;

			if (scf_inst_data_same(&std->dst, &inst->src)
					|| scf_inst_data_same(&std->dst, &inst->dst)) {
				printf("-------\n");
				scf_instruction_print(std);
				break;
			}
		}
		printf("\n");
	}
#endif
	ret = 0;
error:
	scf_vector_free(tmp_insts);
	scf_vector_free(std_insts);
	return ret;
}
