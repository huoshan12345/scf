#include"scf_risc.h"
#include"scf_naja_reg.c"

scf_vector_t* naja_register_colors()
{
	scf_vector_t* colors = scf_vector_alloc();
	if (!colors)
		return NULL;

	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r->id
				|| SCF_RISC_REG_FP == r->id
				|| SCF_RISC_REG_LR == r->id
				|| SCF_RISC_REG_X10 == r->id
				|| SCF_RISC_REG_X11 == r->id)
			continue;

		int ret = scf_vector_add(colors, (void*)r->color);
		if (ret < 0) {
			scf_vector_free(colors);
			return NULL;
		}
	}
#if 0
	srand(time(NULL));
	for (i = 0; i < colors->size; i++) {
		int j = rand() % colors->size;

		void* t = colors->data[i];
		colors->data[i] = colors->data[j];
		colors->data[j] = t;
	}
#endif
	return colors;
}

int naja_reg_cached_vars(scf_register_t* r)
{
	int nb_vars = 0;
	int i;

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r2 = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r2->id
         || SCF_RISC_REG_FP == r2->id
         || SCF_RISC_REG_LR == r2->id
		 || SCF_RISC_REG_X10 == r2->id
		 || SCF_RISC_REG_X11 == r2->id)
			continue;

		if (!naja_color_conflict(r->color, r2->color))
			continue;

		nb_vars += r2->dag_nodes->size;
	}

	return nb_vars;
}

int naja_registers_init()
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r->id
         || SCF_RISC_REG_FP == r->id
         || SCF_RISC_REG_LR == r->id
		 || SCF_RISC_REG_X10 == r->id
		 || SCF_RISC_REG_X11 == r->id)
			continue;

		assert(!r->dag_nodes);

		r->dag_nodes = scf_vector_alloc();
		if (!r->dag_nodes)
			return -ENOMEM;

		r->used = 0;
	}

	return 0;
}

void naja_registers_clear()
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r->id
         || SCF_RISC_REG_FP == r->id
         || SCF_RISC_REG_LR == r->id
		 || SCF_RISC_REG_X10 == r->id
		 || SCF_RISC_REG_X11 == r->id)
			continue;

		if (r->dag_nodes) {
			scf_vector_free(r->dag_nodes);
			r->dag_nodes = NULL;
		}

		r->used = 0;
	}
}

static scf_register_t* risc_reg_cached_min_vars(scf_register_t** regs, int nb_regs)
{
	scf_register_t* r_min = NULL;

	int min = 0;
	int i;

	for (i = 0; i < nb_regs; i++) {
		scf_register_t*	r = regs[i];

		int nb_vars = naja_reg_cached_vars(r);

		if (!r_min) {
			r_min = r;
			min   = nb_vars;
			continue;
		}

		if (min > nb_vars) {
			r_min = r;
			min   = nb_vars;
		}
	}

	return r_min;
}

int naja_caller_save_regs(scf_3ac_code_t* c, scf_function_t* f, uint32_t* regs, int nb_regs, int stack_size, scf_register_t** saved_regs)
{
	int i;
	int j;
	scf_register_t* r;
	scf_register_t* r2;
	scf_instruction_t*    inst;
	scf_register_t* sp   = naja_find_register("sp");

	uint32_t opcode;

	int ret;
	int size = 0;
	int k    = 0;

	for (j = 0; j < nb_regs; j++) {
		r2 = naja_find_register_type_id_bytes(0, regs[j], 8);

		for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {
			r  = &(naja_registers[i]);

			if (SCF_RISC_REG_SP == r->id
             || SCF_RISC_REG_FP == r->id
			 || SCF_RISC_REG_LR == r->id
			 || SCF_RISC_REG_X10 == r->id
			 || SCF_RISC_REG_X11 == r->id)
				continue;

			if (0 == r->dag_nodes->size)
				continue;

			if (naja_color_conflict(r2->color, r->color))
				break;
		}

		if (i == sizeof(naja_registers) / sizeof(naja_registers[0]))
			continue;

		if (stack_size > 0) {
			ret = f->iops->G2P(c, f, r2, sp, size + stack_size, 8);
			if (ret < 0)
				return ret;
		} else {
			inst   = f->iops->PUSH(NULL, r2);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		}

		saved_regs[k++] = r2;
		size += 8;
	}

	if (size & 0xf) {
		r2 = saved_regs[k - 1];

		if (stack_size > 0) {
			ret = f->iops->G2P(c, f, r2, sp, size + stack_size, 8);
			if (ret < 0)
				return ret;
		} else {
			inst   = f->iops->PUSH(NULL, r2);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		}

		saved_regs[k++] = r2;
		size += 8;
	}

	if (stack_size > 0) {
		for (j = 0; j < k / 2; j++) {

			i  = k - 1 - j;
			SCF_XCHG(saved_regs[i], saved_regs[j]);
		}
	}

	return size;
}

int naja_pop_regs(scf_3ac_code_t* c, scf_function_t* f, scf_register_t** regs, int nb_regs, scf_register_t** updated_regs, int nb_updated)
{
	int i;
	int j;

	scf_register_t* sp = naja_find_register("sp");
	scf_register_t* r;
	scf_register_t* r2;
	scf_instruction_t*    inst;

	for (j = nb_regs - 1; j >= 0; j--) {
		r2 = regs[j];

		for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {
			r  = &(naja_registers[i]);

			if (SCF_RISC_REG_SP == r->id
             || SCF_RISC_REG_FP == r->id
             || SCF_RISC_REG_LR == r->id
			 || SCF_RISC_REG_X10 == r->id
			 || SCF_RISC_REG_X11 == r->id)
				continue;

			if (0 == r->dag_nodes->size)
				continue;

			if (naja_color_conflict(r2->color, r->color))
				break;
		}

		if (i == sizeof(naja_registers) / sizeof(naja_registers[0]))
			continue;

		for (i = 0; i < nb_updated; i++) {

			r  = updated_regs[i];

			if (naja_color_conflict(r2->color, r->color))
				break;
		}

		if (i == nb_updated) {
			inst = f->iops->POP(c, r2);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		} else {
			inst = f->iops->ADD_IMM(c, f, sp, sp, 8);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		}
	}
	return 0;
}

int naja_registers_reset()
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r->id
				|| SCF_RISC_REG_FP == r->id
				|| SCF_RISC_REG_LR == r->id
				|| SCF_RISC_REG_X10 == r->id
				|| SCF_RISC_REG_X11 == r->id)
			continue;

		if (!r->dag_nodes)
			continue;

		int j = 0;
		while (j < r->dag_nodes->size) {
			scf_dag_node_t* dn = r->dag_nodes->data[j];

			if (dn->var->w)
				scf_logw("drop: v_%d_%d/%s\n", dn->var->w->line, dn->var->w->pos, dn->var->w->text->data);
			else
				scf_logw("drop: v_%#lx\n", 0xffff & (uintptr_t)dn->var);

			int ret = scf_vector_del(r->dag_nodes, dn);
			if (ret < 0) {
				scf_loge("\n");
				return ret;
			}

			dn->loaded     = 0;
			dn->color      = 0;
		}
	}

	return 0;
}


int naja_overflow_reg(scf_register_t* r, scf_3ac_code_t* c, scf_function_t* f)
{
	int i;

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r2 = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r2->id
				|| SCF_RISC_REG_FP == r2->id
				|| SCF_RISC_REG_LR == r2->id
				|| SCF_RISC_REG_X10 == r2->id
				|| SCF_RISC_REG_X11 == r2->id)
			continue;

		if (!naja_color_conflict(r->color, r2->color))
			continue;

		int ret = risc_save_reg(r2, c, f);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}

	r->used = 1;
	return 0;
}

int naja_overflow_reg2(scf_register_t* r, scf_dag_node_t* dn, scf_3ac_code_t* c, scf_function_t* f)
{
	scf_register_t*	r2;
	scf_dag_node_t* dn2;

	int i;
	int j;

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		r2 = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r2->id
				|| SCF_RISC_REG_FP == r2->id
				|| SCF_RISC_REG_LR == r2->id
				|| SCF_RISC_REG_X10 == r2->id
				|| SCF_RISC_REG_X11 == r2->id)
			continue;

		if (!naja_color_conflict(r->color, r2->color))
			continue;

		for (j  = 0; j < r2->dag_nodes->size; ) {
			dn2 = r2->dag_nodes->data[j];

			if (dn2 == dn) {
				j++;
				continue;
			}

			int ret = risc_save_var(dn2, c, f);
			if (ret < 0)
				return ret;
		}
	}

	r->used = 1;
	return 0;
}

int naja_overflow_reg3(scf_register_t* r, scf_dag_node_t* dn, scf_3ac_code_t* c, scf_function_t* f)
{
	scf_register_t*	r2;
	scf_dn_status_t*    ds2;
	scf_dag_node_t*     dn2;

	int i;
	int j;
	int ret;

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		r2 = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r2->id
				|| SCF_RISC_REG_FP == r2->id
				|| SCF_RISC_REG_LR == r2->id
				|| SCF_RISC_REG_X10 == r2->id
				|| SCF_RISC_REG_X11 == r2->id)
			continue;

		if (!naja_color_conflict(r->color, r2->color))
			continue;

		for (j  = 0; j < r2->dag_nodes->size; ) {
			dn2 = r2->dag_nodes->data[j];

			if (dn2 == dn) {
				j++;
				continue;
			}

			ds2 = scf_vector_find_cmp(c->active_vars, dn2, scf_dn_status_cmp);
			if (!ds2) {
				j++;
				continue;
			}

			if (!ds2->active) {
				j++;
				continue;
			}
#if 1
			scf_variable_t* v  = dn->var;
			scf_variable_t* v2 = dn2->var;
			if (v->w)
				scf_loge("v_%d_%d/%s, bp_offset: %d\n", v->w->line, v->w->pos, v->w->text->data, v->bp_offset);
			else
				scf_loge("v_%#lx, bp_offset: %d\n", 0xffff & (uintptr_t)v, v->bp_offset);

			if (v2->w)
				scf_loge("v2_%d_%d/%s, bp_offset: %d\n", v2->w->line, v2->w->pos, v2->w->text->data, v2->bp_offset);
			else
				scf_loge("v2_%#lx, bp_offset: %d\n", 0xffff & (uintptr_t)v2, v2->bp_offset);
#endif
			int ret = risc_save_var(dn2, c, f);
			if (ret < 0)
				return ret;
		}
	}

	r->used = 1;
	return 0;
}

int naja_reg_used(scf_register_t* r, scf_dag_node_t* dn)
{
	scf_register_t*	r2;
	scf_dag_node_t*     dn2;

	int i;
	int j;

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		r2 = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r2->id
				|| SCF_RISC_REG_FP == r2->id
				|| SCF_RISC_REG_LR == r2->id
				|| SCF_RISC_REG_X10 == r2->id
				|| SCF_RISC_REG_X11 == r2->id)
			continue;

		if (!naja_color_conflict(r->color, r2->color))
			continue;

		for (j  = 0; j < r2->dag_nodes->size; j++) {
			dn2 = r2->dag_nodes->data[j];

			if (dn2 != dn)
				return 1;
		}
	}
	return 0;
}

scf_register_t* naja_select_overflowed_reg(scf_dag_node_t* dn, scf_3ac_code_t* c, int is_float)
{
	scf_vector_t*       neighbors = NULL;
	scf_graph_node_t*   gn        = NULL;

	scf_register_t* free_regs[sizeof(naja_registers) / sizeof(naja_registers[0])];

	int nb_free_regs = 0;
	int bytes        = 8;
	int ret;
	int i;
	int j;

	assert(c->rcg);

	if (dn) {
		is_float =   scf_variable_float(dn->var);
		bytes    = naja_variable_size (dn->var);
	}

	ret = risc_rcg_find_node(&gn, c->rcg, dn, NULL);
	if (ret < 0)
		neighbors = c->rcg->nodes;
	else
		neighbors = gn->neighbors;

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r->id
				|| SCF_RISC_REG_FP == r->id
				|| SCF_RISC_REG_LR == r->id
				|| SCF_RISC_REG_X10 == r->id
				|| SCF_RISC_REG_X11 == r->id)
			continue;

		if (r->bytes < bytes || RISC_COLOR_TYPE(r->color) != is_float)
			continue;

		for (j = 0; j < neighbors->size; j++) {

			scf_graph_node_t* neighbor = neighbors->data[j];
			risc_rcg_node_t* rn       = neighbor->data;

			if (rn->dag_node) {
				if (rn->dag_node->color <= 0)
					continue;

				if (naja_color_conflict(r->color, rn->dag_node->color))
					break;
			} else {
				assert(rn->reg);

				if (naja_color_conflict(r->color, rn->reg->color))
					break;
			}
		}

		if (j == neighbors->size)
			free_regs[nb_free_regs++] = r;
	}

	if (nb_free_regs > 0)
		return risc_reg_cached_min_vars(free_regs, nb_free_regs);

	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (SCF_RISC_REG_SP == r->id
				|| SCF_RISC_REG_FP == r->id
				|| SCF_RISC_REG_LR == r->id
				|| SCF_RISC_REG_X10 == r->id
				|| SCF_RISC_REG_X11 == r->id)
			continue;

		if (r->bytes < bytes || RISC_COLOR_TYPE(r->color) != is_float)
			continue;

		if (c->dsts) {
			scf_3ac_operand_t* dst;

			for (j  = 0; j < c->dsts->size; j++) {
				dst =        c->dsts->data[j];

				if (dst->dag_node && dst->dag_node->color > 0 
						&& naja_color_conflict(r->color, dst->dag_node->color))
					break;
			}

			if (j < c->dsts->size)
				continue;
		}

		if (c->srcs) {
			scf_3ac_operand_t* src;

			for (j  = 0; j < c->srcs->size; j++) {
				src =        c->srcs->data[j];

				if (src->dag_node && src->dag_node->color > 0
						&& naja_color_conflict(r->color, src->dag_node->color))
					break;
			}

			if (j < c->srcs->size)
				continue;
		}

		return r;
	}

	return NULL;
}

static void naja_argv_rabi(scf_function_t* f)
{
	scf_variable_t* v;

	f->args_int   = 0;
	f->args_float = 0;

	int bp_int    = -8;
	int bp_floats = -8 - (int)f->rops->ABI_NB * 8;
	int bp_others = 16;

	int i;
	for (i = 0; i < f->argv->size; i++) {
		v  =        f->argv->data[i];

		if (!v->arg_flag) {
			v ->arg_flag = 1;
			assert(f->inline_flag);
		}

		int is_float =      scf_variable_float(v);
		int size     = f->rops->variable_size (v);

		if (is_float) {

			if (f->args_float < f->rops->ABI_NB) {

				v->rabi       = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_float_regs[f->args_float], size);
				v->bp_offset  = bp_floats;
				bp_floats    -= 8;
				f->args_float++;
				continue;
			}
		} else if (f->args_int < f->rops->ABI_NB) {

			v->rabi       = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_regs[f->args_int], size);
			v->bp_offset  = bp_int;
			bp_int       -= 8;
			f->args_int++;
			continue;
		}

		v->rabi       = NULL;
		v->bp_offset  = bp_others;
		bp_others    += 8;
	}
}

void naja_call_rabi(scf_3ac_code_t* c, scf_function_t* f, int* p_nints, int* p_nfloats, int* p_ndoubles)
{
	scf_3ac_operand_t* src = NULL;
	scf_dag_node_t*    dn  = NULL;

	int nfloats = 0;
	int nints   = 0;
	int i;

	for (i  = 1; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];
		dn  =        src->dag_node;

		int is_float =      scf_variable_float(dn->var);
		int size     = f->rops->variable_size (dn->var);

		if (is_float) {
			if (nfloats   < f->rops->ABI_NB)
				dn->rabi2 = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_float_regs[nfloats++], size);
			else
				dn->rabi2 = NULL;
		} else {
			if (nints     < f->rops->ABI_NB)
				dn->rabi2 = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_regs[nints++], size);
			else
				dn->rabi2 = NULL;
		}

		src->rabi = dn->rabi2;
	}

	if (p_nints)
		*p_nints = nints;

	if (p_nfloats)
		*p_nfloats = nfloats;
}

int naja_push_callee_regs(scf_3ac_code_t* c, scf_function_t* f)
{
	scf_instruction_t* inst;
	scf_register_t*    r;

	int i;
	for (i = 0; i < f->rops->ABI_CALLEE_SAVES_NB; i++) {

		scf_logi("f->rops->abi_callee_saves[%d]: %d\n", i, f->rops->abi_callee_saves[i]);
		r  = f->rops->find_register_type_id_bytes(0, f->rops->abi_callee_saves[i], 8);

		if (!r->used) {
			r  = f->rops->find_register_type_id_bytes(0, f->rops->abi_callee_saves[i], 4);

			if (!r->used)
				continue;
		}

		inst   = f->iops->PUSH(NULL, r);
		RISC_INST_ADD_CHECK(f->init_code->instructions, inst);

		f->init_code_bytes += inst->len;
	}

	return 0;
}

int naja_pop_callee_regs(scf_3ac_code_t* c, scf_function_t* f)
{
	scf_instruction_t* inst;
	scf_register_t*    r;

	int i;
	for (i = f->rops->ABI_CALLEE_SAVES_NB - 1; i >= 0; i--) {

		r  = f->rops->find_register_type_id_bytes(0, f->rops->abi_callee_saves[i], 8);

		scf_logi("r: %p, f->rops->abi_callee_saves[%d]: %d\n", r, i, f->rops->abi_callee_saves[i]);

		if (!r->used) {
			r  = f->rops->find_register_type_id_bytes(0, f->rops->abi_callee_saves[i], 4);

			if (!r->used)
				continue;
		}

		inst = f->iops->POP(c, r);
		RISC_INST_ADD_CHECK(c->instructions, inst);
	}

	return 0;
}

#define RISC_ABI_NB              (sizeof(naja_abi_regs)        / sizeof(naja_abi_regs[0]))
#define RISC_ABI_RET_NB          (sizeof(risc_abi_ret_regs)     / sizeof(risc_abi_ret_regs[0]))
#define RISC_ABI_CALLER_SAVES_NB (sizeof(risc_abi_caller_saves) / sizeof(risc_abi_caller_saves[0]))
#define RISC_ABI_CALLEE_SAVES_NB (sizeof(risc_abi_callee_saves) / sizeof(risc_abi_callee_saves[0]))

scf_regs_ops_t  regs_ops_naja =
{
	.name                        = "naja",

	.abi_regs                    = naja_abi_regs,
	.abi_float_regs              = naja_abi_float_regs,
	.abi_ret_regs                = naja_abi_ret_regs,
	.abi_caller_saves            = naja_abi_caller_saves,
	.abi_callee_saves            = naja_abi_callee_saves,

	.ABI_NB                      = sizeof(naja_abi_regs)         / sizeof(uint32_t),
	.ABI_RET_NB                  = sizeof(naja_abi_ret_regs)     / sizeof(uint32_t),
	.ABI_CALLER_SAVES_NB         = sizeof(naja_abi_caller_saves) / sizeof(uint32_t),
	.ABI_CALLEE_SAVES_NB         = sizeof(naja_abi_callee_saves) / sizeof(uint32_t),

	.MAX_BYTES                   = 8,

	.registers_init              = naja_registers_init,
	.registers_reset             = naja_registers_reset,
	.registers_clear             = naja_registers_clear,
	.register_colors             = naja_register_colors,

	.color_conflict              = naja_color_conflict,

	.argv_rabi                   = naja_argv_rabi,
	.call_rabi                   = naja_call_rabi,

	.reg_used                    = naja_reg_used,
	.reg_cached_vars             = naja_reg_cached_vars,

	.variable_size               = naja_variable_size,

	.caller_save_regs            = naja_caller_save_regs,
	.pop_regs                    = naja_pop_regs,

	.find_register               = naja_find_register,
	.find_register_color         = naja_find_register_color,
	.find_register_color_bytes   = naja_find_register_color_bytes,
	.find_register_type_id_bytes = naja_find_register_type_id_bytes,

	.select_overflowed_reg       = naja_select_overflowed_reg,
	.overflow_reg                = naja_overflow_reg,
	.overflow_reg2               = naja_overflow_reg2,
	.overflow_reg3               = naja_overflow_reg3,

	.push_callee_regs            = naja_push_callee_regs,
	.pop_callee_regs             = naja_pop_callee_regs,
};
