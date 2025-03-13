#include"scf_eda.h"

#define EDA_INST_SRC_CHECK() \
	if (!c->srcs || c->srcs->size != 1) \
	return -EINVAL; \
	\
	scf_eda_context_t* eda  = ctx->priv; \
	scf_function_t*    f    = eda->f; \
	\
	scf_3ac_operand_t* src  = c->srcs->data[0]; \
	\
	if (!src || !src->dag_node) \
	return -EINVAL;

#define EDA_INST_DST_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
	return -EINVAL; \
	\
	scf_eda_context_t* eda  = ctx->priv; \
	scf_function_t*    f    = eda->f; \
	\
	scf_3ac_operand_t* dst  = c->dsts->data[0]; \
	\
	if (!dst || !dst->dag_node) \
	return -EINVAL;

#define EDA_INST_OP2_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	if (!c->srcs || c->srcs->size != 1) \
		return -EINVAL; \
	\
	scf_eda_context_t* eda  = ctx->priv; \
	scf_function_t*    f    = eda->f; \
	\
	scf_3ac_operand_t* dst  = c->dsts->data[0]; \
	scf_3ac_operand_t* src  = c->srcs->data[0]; \
	\
	if (!src || !src->dag_node) \
		return -EINVAL; \
	\
	if (!dst || !dst->dag_node) \
		return -EINVAL; \
	\
	if (src->dag_node->var->size != dst->dag_node->var->size) {\
		scf_loge("size: %d, %d\n", src->dag_node->var->size, dst->dag_node->var->size); \
		return -EINVAL; \
	}

#define EDA_INST_OP3_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	if (!c->srcs || c->srcs->size != 2) \
		return -EINVAL; \
	\
	scf_eda_context_t* eda  = ctx->priv; \
	scf_function_t*    f    = eda->f; \
	\
	scf_3ac_operand_t* dst  = c->dsts->data[0]; \
	scf_3ac_operand_t* src0 = c->srcs->data[0]; \
	scf_3ac_operand_t* src1 = c->srcs->data[1]; \
	\
	if (!src0 || !src0->dag_node) \
		return -EINVAL; \
	\
	if (!src1 || !src1->dag_node) \
		return -EINVAL; \
	\
	if (!dst || !dst->dag_node) \
		return -EINVAL; \
	\
	if (src0->dag_node->var->size != src1->dag_node->var->size) {\
		scf_loge("size: %d, %d\n", src0->dag_node->var->size, src1->dag_node->var->size); \
		return -EINVAL; \
	}

#define EDA_INST_IN_CHECK(_in, _N) \
	do { \
		int i; \
		if (!(_in)->var->arg_flag) { \
			if ((_in)->n_pins != _N) \
				return -EINVAL; \
			\
			for (i = 0; i < _N; i++) { \
				if (!(_in)->pins[i]) \
					return -EINVAL; \
			} \
		} \
	} while (0)

#define EDA_PIN_ADD_INPUT(_in, _i, _ef, _p) \
		do { \
			ScfEcomponent* c = (_ef)->components[(_p)->cid]; \
			ScfEcomponent* R = NULL; \
			\
			if (!(_in)->pins[_i]) { \
				EDA_INST_ADD_COMPONENT(_ef, R, SCF_EDA_Resistor); \
				\
				EDA_PIN_ADD_PIN(c, (_p)->id, R, 0); \
				\
				(_in)->pins[_i] = R->pins[1]; \
			} else { \
				R = (_ef)->components[(_in)->pins[_i]->cid]; \
				\
				EDA_PIN_ADD_PIN(c, (_p)->id, R, 0); \
			} \
		} while (0)


static int __eda_bit_nand(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B    = f->ef->components[0];
	ScfEcomponent*  NAND = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, NAND, SCF_EDA_NAND);

	EDA_PIN_ADD_PIN(NAND, SCF_EDA_NAND_POS, B, SCF_EDA_Battery_POS);
	EDA_PIN_ADD_PIN(NAND, SCF_EDA_NAND_NEG, B, SCF_EDA_Battery_NEG);

	*in0 = NAND->pins[SCF_EDA_NAND_IN0];
	*in1 = NAND->pins[SCF_EDA_NAND_IN1];
	*out = NAND->pins[SCF_EDA_NAND_OUT];
	return 0;
}

static int __eda_bit_not(scf_function_t* f, ScfEpin** in, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  NOT = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, NOT, SCF_EDA_NOT);

	EDA_PIN_ADD_PIN(NOT, SCF_EDA_NOT_POS, B, SCF_EDA_Battery_POS);
	EDA_PIN_ADD_PIN(NOT, SCF_EDA_NOT_NEG, B, SCF_EDA_Battery_NEG);

	*in  = NOT->pins[SCF_EDA_NOT_IN];
	*out = NOT->pins[SCF_EDA_NOT_OUT];
	return 0;
}

static int __eda_bit_and(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  AND = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, AND, SCF_EDA_AND);

	EDA_PIN_ADD_PIN(AND, SCF_EDA_AND_POS, B, SCF_EDA_Battery_POS);
	EDA_PIN_ADD_PIN(AND, SCF_EDA_AND_NEG, B, SCF_EDA_Battery_NEG);

	*in0 = AND->pins[SCF_EDA_AND_IN0];
	*in1 = AND->pins[SCF_EDA_AND_IN1];
	*out = AND->pins[SCF_EDA_AND_OUT];
	return 0;
}

static int __eda_bit_or(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B  = f->ef->components[0];
	ScfEcomponent*  OR = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, OR, SCF_EDA_OR);

	EDA_PIN_ADD_PIN(OR, SCF_EDA_OR_POS, B, SCF_EDA_Battery_POS);
	EDA_PIN_ADD_PIN(OR, SCF_EDA_OR_NEG, B, SCF_EDA_Battery_NEG);

	*in0 = OR->pins[SCF_EDA_OR_IN0];
	*in1 = OR->pins[SCF_EDA_OR_IN1];
	*out = OR->pins[SCF_EDA_OR_OUT];
	return 0;
}

static int __eda_bit_xor(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  XOR = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, XOR, SCF_EDA_XOR);

	EDA_PIN_ADD_PIN(XOR, SCF_EDA_XOR_POS, B, SCF_EDA_Battery_POS);
	EDA_PIN_ADD_PIN(XOR, SCF_EDA_XOR_NEG, B, SCF_EDA_Battery_NEG);

	*in0 = XOR->pins[SCF_EDA_XOR_IN0];
	*in1 = XOR->pins[SCF_EDA_XOR_IN1];
	*out = XOR->pins[SCF_EDA_XOR_OUT];
	return 0;
}

static int __eda_bit_add(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out, ScfEpin** cf)
{
	ScfEpin* and0 = NULL;
	ScfEpin* and1 = NULL;

	int ret = __eda_bit_xor(f, in0, in1, out);
	if (ret < 0)
		return ret;

	ret = __eda_bit_and(f, &and0, &and1, cf);
	if (ret < 0)
		return ret;

	EDA_PIN_ADD_PIN_EF(f->ef, and0, *in0);
	EDA_PIN_ADD_PIN_EF(f->ef, and1, *in1);
	return 0;
}

static int __eda_bit_adc(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** in2, ScfEpin** out, ScfEpin** cf)
{
	ScfEpin* out0 = NULL;
	ScfEpin* cf0  = NULL;

	ScfEpin* in3  = NULL;
	ScfEpin* cf1  = NULL;

	ScfEpin* or0  = NULL;
	ScfEpin* or1  = NULL;

	int ret = __eda_bit_add(f, in0, in1, &out0, &cf0);
	if (ret < 0)
		return ret;

	ret = __eda_bit_add(f, in2, &in3, out, &cf1);
	if (ret < 0)
		return ret;
	EDA_PIN_ADD_PIN_EF(f->ef, out0, in3);

	ret = __eda_bit_or(f, &or0, &or1, cf);
	if (ret < 0)
		return ret;
	EDA_PIN_ADD_PIN_EF(f->ef, or0,  cf0);
	EDA_PIN_ADD_PIN_EF(f->ef, or1,  cf1);
	return 0;
}

static int _eda_inst_bit_not_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP2_CHECK()

	scf_dag_node_t* in  = src->dag_node;
	scf_dag_node_t* out = dst->dag_node;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in ->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {
		ScfEpin* pi = NULL;
		ScfEpin* po = NULL;

		int ret = __eda_bit_not(f, &pi, &po);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in, i, f->ef, pi);

		if (in->var->arg_flag) {
			in->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in->pins[i]->io_lid = i;
		}

		out->pins[i] = po;
	}

	return 0;
}

static int _eda_inst_bit_and_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* in0 = src0->dag_node;
	scf_dag_node_t* in1 = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0 = NULL;
		ScfEpin* p1 = NULL;
		ScfEpin* po = NULL;

		int ret = __eda_bit_and(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		if (in0->var->arg_flag) {
			in0->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in0->pins[i]->io_lid = i;
		}

		if (in1->var->arg_flag) {
			in1->pins[i]->flags |= SCF_EDA_PIN_IN;
			in1->pins[i]->io_lid = i;
		}

		out->pins[i] = po;
	}

	return 0;
}

static int _eda_inst_bit_or_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* in0 = src0->dag_node;
	scf_dag_node_t* in1 = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0 = NULL;
		ScfEpin* p1 = NULL;
		ScfEpin* po = NULL;

		int ret = __eda_bit_or(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		if (in0->var->arg_flag) {
			in0->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in0->pins[i]->io_lid = i;
		}

		if (in1->var->arg_flag) {
			in1->pins[i]->flags |= SCF_EDA_PIN_IN;
			in1->pins[i]->io_lid = i;
		}

		out->pins[i] = po;
	}

	return 0;
}

static int _eda_inst_shl_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* src = src0->dag_node;
	scf_dag_node_t* sht = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;
	ScfEcomponent*  B   = f->ef->components[0];

	int i;
	int j;
	int N = eda_variable_size(src->var);

	EDA_INST_IN_CHECK(src, N);
	EDA_INST_IN_CHECK(sht, N);

	src->n_pins = N;
	sht->n_pins = N;
	out->n_pins = N;

	ScfEpin* res[SCF_EDA_MAX_BITS];
	ScfEpin* tmp[SCF_EDA_MAX_BITS];

	for (j = 0; j < N && j < 6; j++) {
		ScfEpin* p1 = NULL;
		ScfEpin* p2 = NULL;

		int ret = __eda_bit_not(f, &p1, &p2);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_INPUT(sht, j, f->ef, p1);

		for (i = 0; i < (1 << j); i++) {
			ScfEpin* p00 = NULL;
			ScfEpin* p01 = NULL;
			ScfEpin* po = NULL;

			ret = __eda_bit_and(f, &p00, &p01, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p01, p2);

			if (j > 0)
				EDA_PIN_ADD_PIN_EF(f->ef, p00, res[i]);
			else
				EDA_PIN_ADD_INPUT(src, i, f->ef, p00);

			tmp[i] = po;
		}

		for ( ; i < N; i++) {
			ScfEpin* p00 = NULL;
			ScfEpin* p01 = NULL;
			ScfEpin* po0 = NULL;

			ScfEpin* p10 = NULL;
			ScfEpin* p11 = NULL;
			ScfEpin* po1 = NULL;

			ScfEpin* p20 = NULL;
			ScfEpin* p21 = NULL;
			ScfEpin* po  = NULL;
/*
x = sht[j]
y = src[i - (1 << j)]
z = src[i]

out[i] =     (x & y) |   (~x & z)
       =  ~(~(x & y) &  ~(~x & z))
       = (x NAND y) NAND (~x NAND z)
 */
			ret = __eda_bit_nand(f, &p00, &p10, &po0);
			if (ret < 0)
				return ret;

			ret = __eda_bit_nand(f, &p01, &p11, &po1);
			if (ret < 0)
				return ret;

			ret = __eda_bit_nand(f, &p20, &p21, &po);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p10, p2);
			EDA_PIN_ADD_PIN_EF(f->ef, p11, p1);

			EDA_PIN_ADD_PIN_EF(f->ef, p20, po0);
			EDA_PIN_ADD_PIN_EF(f->ef, p21, po1);

			if (j > 0) {
				EDA_PIN_ADD_PIN_EF(f->ef, p00, res[i]);
				EDA_PIN_ADD_PIN_EF(f->ef, p01, res[i - (1 << j)]);
			} else {
				EDA_PIN_ADD_INPUT(src, i,            f->ef, p00);
				EDA_PIN_ADD_INPUT(src, i - (1 << j), f->ef, p01);
			}

			tmp[i] = po;
		}

		for (i = 0; i < N; i++) {
			res[i] = tmp[i];
			tmp[i] = NULL;
		}
	}

	for (i = 0; i < N; i++) {
		if (src->var->arg_flag) {
			src->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			src->pins[i]->io_lid = i;
		}

		if (sht->var->arg_flag) {
			sht->pins[i]->flags |= SCF_EDA_PIN_IN;
			sht->pins[i]->io_lid = i;
		}

		out->pins[i] = res[i];
	}

	return 0;
}

static int _eda_inst_shr_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* src = src0->dag_node;
	scf_dag_node_t* sht = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;
	ScfEcomponent*  B   = f->ef->components[0];

	int i;
	int j;
	int N    = eda_variable_size  (src->var);
	int sign = scf_variable_signed(src->var);

	EDA_INST_IN_CHECK(src, N);
	EDA_INST_IN_CHECK(sht, N);

	src->n_pins = N;
	sht->n_pins = N;
	out->n_pins = N;

	ScfEpin* res[SCF_EDA_MAX_BITS];
	ScfEpin* tmp[SCF_EDA_MAX_BITS];

	for (j = 0; j < N && j < 6; j++) {
		ScfEpin* p1 = NULL;
		ScfEpin* p2 = NULL;

		int ret = __eda_bit_not(f, &p1, &p2);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_INPUT(sht, j, f->ef, p1);

		for (i = 0; i < N; i++) {
			ScfEpin* p00 = NULL;
			ScfEpin* p01 = NULL;
			ScfEpin* po0 = NULL;

			ScfEpin* p10 = NULL;
			ScfEpin* p11 = NULL;
			ScfEpin* po1 = NULL;

			ScfEpin* p20 = NULL;
			ScfEpin* p21 = NULL;
			ScfEpin* po  = NULL;
/*
x = sht[j]
y = src[i + (1 << j)]
z = src[i]

out[i] =     (x & y) |   (~x & z)
       =  ~(~(x & y) &  ~(~x & z))
       = (x NAND y) NAND (~x NAND z)
 */

			if (i >= N - (1 << j) && !sign) {

				ret = __eda_bit_and(f, &p00, &p10, &po);
				if (ret < 0)
					return ret;
				EDA_PIN_ADD_PIN_EF(f->ef, p10, p2);

			} else {
				ret = __eda_bit_nand(f, &p00, &p10, &po0);
				if (ret < 0)
					return ret;

				ret = __eda_bit_nand(f, &p01, &p11, &po1);
				if (ret < 0)
					return ret;

				ret = __eda_bit_nand(f, &p20, &p21, &po);
				if (ret < 0)
					return ret;

				EDA_PIN_ADD_PIN_EF(f->ef, p10, p2);
				EDA_PIN_ADD_PIN_EF(f->ef, p11, p1);

				EDA_PIN_ADD_PIN_EF(f->ef, p21, po1);
				EDA_PIN_ADD_PIN_EF(f->ef, p20, po0);

				int k = i + (1 << j);
				if (k >= N)
					k =  N - 1;

				scf_logi("sign: %d, j: %d, i: %d, k: %d\n", sign, j, i, k);

				if (j > 0)
					EDA_PIN_ADD_PIN_EF(f->ef, p01, res[k]);
				else
					EDA_PIN_ADD_INPUT(src, k, f->ef, p01);
			}

			if (j > 0)
				EDA_PIN_ADD_PIN_EF(f->ef, p00, res[i]);
			else
				EDA_PIN_ADD_INPUT(src, i, f->ef, p00);

			tmp[i] = po;
		}

		for (i = 0; i < N; i++) {
			res[i] = tmp[i];
			tmp[i] = NULL;
		}
	}

	for (i = 0; i < N; i++) {
		if (src->var->arg_flag) {
			src->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			src->pins[i]->io_lid = i;
		}

		if (sht->var->arg_flag) {
			sht->pins[i]->flags |= SCF_EDA_PIN_IN;
			sht->pins[i]->io_lid = i;
		}

		out->pins[i] = res[i];
	}

	return 0;
}

static int _eda_inst_add_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* in0 = src0->dag_node;
	scf_dag_node_t* in1 = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;

	ScfEcomponent*  B   = f->ef->components[0];
	ScfEpin*        pc  = NULL;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* p2  = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		if (i > 0) {
			int ret = __eda_bit_adc(f, &p0, &p1, &p2, &res, &cf);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p2, pc);

		} else {
			int ret = __eda_bit_add(f, &p0, &p1, &res, &cf);
			if (ret < 0)
				return ret;
		}

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		pc         = cf; // carry flag
		cf->flags |= SCF_EDA_PIN_CF;

		if (in0->var->arg_flag) {
			in0->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in0->pins[i]->io_lid = i;
		}

		if (in1->var->arg_flag) {
			in1->pins[i]->flags |= SCF_EDA_PIN_IN;
			in1->pins[i]->io_lid = i;
		}

		out->pins[i] = res; // result
	}

	return 0;
}

static int _eda_inst_sub_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* in0 = src0->dag_node;
	scf_dag_node_t* in1 = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;

	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  R0  = NULL;
	ScfEpin*        pc  = NULL;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	EDA_INST_ADD_COMPONENT(f->ef, R0, SCF_EDA_Resistor);

	EDA_PIN_ADD_PIN(R0, 1, B, SCF_EDA_Battery_POS);
	pc = R0->pins[0];

	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* p2  = NULL;
		ScfEpin* p3  = NULL;
		ScfEpin* not = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		int ret = __eda_bit_not(f, &p1, &not);
		if (ret < 0)
			return ret;

		ret = __eda_bit_adc(f, &p0, &p2, &p3, &res, &cf);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p2, not);
		EDA_PIN_ADD_PIN_EF(f->ef, p3, pc);

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		pc         = cf;
		cf->flags |= SCF_EDA_PIN_CF;

		if (in0->var->arg_flag) {
			in0->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in0->pins[i]->io_lid = i;
		}

		if (in1->var->arg_flag) {
			in1->pins[i]->flags |= SCF_EDA_PIN_IN;
			in1->pins[i]->io_lid = i;
		}

		out->pins[i] = res;
	}

	return 0;
}

static int __eda_bit_mla(scf_function_t* f, ScfEpin** a, ScfEpin** b, ScfEpin** c, ScfEpin** d, ScfEpin** out, ScfEpin** cf)
{
	ScfEpin* ab0 = NULL;
	ScfEpin* cd0 = NULL;

	ScfEpin* ab1 = NULL;
	ScfEpin* cd1 = NULL;

	int ret = __eda_bit_and(f, a, b, &ab0);
	if (ret < 0)
		return ret;

	ret = __eda_bit_and(f, c, d, &cd0);
	if (ret < 0)
		return ret;

	ret = __eda_bit_add(f, &ab1, &cd1, out, cf);
	if (ret < 0)
		return ret;

	EDA_PIN_ADD_PIN_EF(f->ef, ab0, ab1);
	EDA_PIN_ADD_PIN_EF(f->ef, cd0, cd1);
	return 0;
}

static int _eda_inst_mul_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	scf_dag_node_t* in0 = src0->dag_node;
	scf_dag_node_t* in1 = src1->dag_node;
	scf_dag_node_t* out = dst ->dag_node;

	ScfEpin*  adds[SCF_EDA_MAX_BITS] = {NULL};
	ScfEpin*  cfs [SCF_EDA_MAX_BITS] = {NULL};

	int n_adds = 0;
	int n_cfs  = 0;

	int N = eda_variable_size(in0->var);
	int i;

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0j = NULL;
		ScfEpin* p0k = NULL;
		ScfEpin* p1j = NULL;
		ScfEpin* p1k = NULL;
		ScfEpin* res = NULL;
		ScfEpin* cf  = NULL;

		if (0 == i) {
			int ret = __eda_bit_and(f, &p0j, &p1k, &res);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_INPUT(in0, 0, f->ef, p0j);
			EDA_PIN_ADD_INPUT(in1, 0, f->ef, p1k);

			out->pins[0] = res;

			if (in0->var->arg_flag) {
				in0->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}

			if (in1->var->arg_flag) {
				in1->pins[i]->flags |= SCF_EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
			continue;
		}

		int j = 0;
		int k = i;

		while (j < k) {
			int ret = __eda_bit_mla(f, &p0j, &p1k, &p0k, &p1j, &adds[n_adds], &cfs[n_cfs]);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_INPUT(in0, j, f->ef, p0j);
			EDA_PIN_ADD_INPUT(in0, k, f->ef, p0k);

			EDA_PIN_ADD_INPUT(in1, j, f->ef, p1j);
			EDA_PIN_ADD_INPUT(in1, k, f->ef, p1k);

			cfs[n_cfs]->flags |= SCF_EDA_PIN_CF;

			in0->pins[j]->flags |= SCF_EDA_PIN_IN0;
			in0->pins[k]->flags |= SCF_EDA_PIN_IN0;

			n_adds++;
			n_cfs++;

			j++;
			k--;
		}

		if (j == k) {
			int ret = __eda_bit_and(f, &p0j, &p1k, &adds[n_adds]);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_INPUT(in0, j, f->ef, p0j);
			EDA_PIN_ADD_INPUT(in1, j, f->ef, p1k);

			in0->pins[j]->flags |= SCF_EDA_PIN_IN0;

			n_adds++;
		}

		while (n_adds > 1) {
			j = 0;
			k = n_adds - 1;

			while (j < k) {
				p0j = NULL;
				p1k = NULL;

				int ret = __eda_bit_add(f, &p0j, &p1k, &res, &cfs[n_cfs]);
				if (ret < 0)
					return ret;

				EDA_PIN_ADD_PIN_EF(f->ef, p0j, adds[j]);
				EDA_PIN_ADD_PIN_EF(f->ef, p1k, adds[k]);

				cfs[n_cfs++]->flags |= SCF_EDA_PIN_CF;

				adds[j] = res;

				j++;
				k--;
			}

			if (j == k)
				n_adds = j + 1;
			else
				n_adds = j;
			scf_logd("i: %d, n_adds: %d, j: %d, k: %d\n\n", i, n_adds, j, k);
		}

		out->pins[i] = adds[0];

		for (j = 0; j < n_cfs; j++)
			adds[j] = cfs[j];

		n_adds = n_cfs;
		n_cfs  = 0;

		if (in0->var->arg_flag) {
			in0->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in0->pins[i]->io_lid = i;
		}

		if (in1->var->arg_flag) {
			in1->pins[i]->flags |= SCF_EDA_PIN_IN;
			in1->pins[i]->io_lid = i;
		}
	}

	return 0;
}

static int _eda_inst_div_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_inc_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_SRC_CHECK()

	scf_dag_node_t* in = src->dag_node;
	ScfEpin*        pc = NULL;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in->n_pins = N;

// (x | y) & ~(x & y) == ~y when x = 1
	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		if (i > 0) {
			int ret = __eda_bit_add(f, &p0, &p1, &res, &cf);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p1, pc);

		} else {
			int ret = __eda_bit_not(f, &p0, &res);
			if (ret < 0)
				return ret;

			cf = p0;
		}

		EDA_PIN_ADD_INPUT(in, i, f->ef, p0);

		pc         = cf;
		cf->flags |= SCF_EDA_PIN_CF;

		if (in->var->arg_flag) {
			in->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in->pins[i]->io_lid = i;
		}

		in->pins[i] = res;
	}

	return 0;
}

static int _eda_inst_dec_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_SRC_CHECK()

	scf_dag_node_t* in = src->dag_node;
	ScfEpin*        pc = NULL;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in->n_pins = N;
// y-- == y - 1 == y + (-1) == y + 0xff...ff, every bit add 1

// x + y == (x | y) & ~(x & y) == ~y when x = 1
	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		if (i > 0) {
			ScfEpin* p2  = NULL;
			ScfEpin* not = NULL;
			ScfEpin* cf0 = NULL;
			ScfEpin* cf1 = NULL;
			ScfEpin* cf2 = NULL;
			ScfEpin* cf3 = NULL;

			int ret = __eda_bit_not(f, &cf0, &not); // ~cf0 == cf0 + 1
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, cf0, pc);

			ret = __eda_bit_add(f, &p0, &p1, &res, &cf1);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p1, not);

			ret = __eda_bit_or(f, &cf2, &cf3, &cf);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, cf2, cf0);
			EDA_PIN_ADD_PIN_EF(f->ef, cf3, cf1);

		} else {
			int ret = __eda_bit_not(f, &p0, &res);
			if (ret < 0)
				return ret;

			cf = p0;
		}

		EDA_PIN_ADD_INPUT(in, i, f->ef, p0);

		pc         = cf;
		cf->flags |= SCF_EDA_PIN_CF;

		if (in->var->arg_flag) {
			in->pins[i]->flags |= SCF_EDA_PIN_IN | SCF_EDA_PIN_IN0;
			in->pins[i]->io_lid = i;
		}

		in->pins[i] = res;
	}

	return 0;
}

static int _eda_inst_assign_array_index_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	scf_eda_context_t*  eda    = ctx->priv;
	scf_function_t*     f      = eda->f;

	scf_3ac_operand_t*  base   = c->srcs->data[0];
	scf_3ac_operand_t*  index  = c->srcs->data[1];
	scf_3ac_operand_t*  scale  = c->srcs->data[2];
	scf_3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	return -EINVAL;
}

static int _eda_inst_array_index_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	scf_eda_context_t*  eda    = ctx->priv;
	scf_function_t*     f      = eda->f;

	scf_3ac_operand_t*  dst    = c->dsts->data[0];
	scf_3ac_operand_t*  base   = c->srcs->data[0];
	scf_3ac_operand_t*  index  = c->srcs->data[1];
	scf_3ac_operand_t*  scale  = c->srcs->data[2];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = scf_vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	scf_variable_t*     vd  = dst  ->dag_node->var;
	scf_variable_t*     vb  = base ->dag_node->var;
	scf_variable_t*     vi  = index->dag_node->var;
	scf_variable_t*     vs  = scale->dag_node->var;

	return -EINVAL;
}

static int _eda_inst_teq_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_logic_not_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_cmp_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setz_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setnz_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setgt_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setge_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setlt_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setle_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_eq_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_ne_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_gt_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_ge_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_lt_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_le_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_cast_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	scf_eda_context_t* eda = ctx->priv;
	scf_function_t*    f   = eda->f;
	scf_3ac_operand_t* src = c->srcs->data[0];
	scf_3ac_operand_t* dst = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (0 == dst->dag_node->color)
		return -EINVAL;

	return -EINVAL;
}

static int _eda_inst_return_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size < 1) {
		scf_loge("\n");
		return -EINVAL;
	}

	scf_eda_context_t*  eda  = ctx->priv;
	scf_function_t*     f    = eda->f;
	scf_3ac_operand_t*  src  = NULL;
	scf_dag_node_t*     out  = NULL;

	int i;
	int j;

	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		out = src->dag_node;

		if (out->n_pins <= 0) {
			scf_loge("out: %p\n", out);
			return -EINVAL;
		}

		for (j = 0; j < out->n_pins; j++) {
			out->pins[j]->flags |= SCF_EDA_PIN_OUT;
			out->pins[j]->io_lid = j;
		}
	}

	return 0;
}

static int _eda_inst_nop_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_end_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_goto_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jz_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jnz_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jgt_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jge_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jlt_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jle_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_load_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_reload_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_save_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_assign_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	EDA_INST_OP2_CHECK()

	scf_dag_node_t* in  = src->dag_node;
	scf_dag_node_t* out = dst->dag_node;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in ->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {
		out->pins[i] = in->pins[i];
	}
	return 0;
}

static int _eda_inst_and_assign_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_or_assign_handler(scf_native_t* ctx, scf_3ac_code_t* c)
{
	return 0;
}

static eda_inst_handler_pt eda_inst_handlers[] =
{
	[SCF_OP_ARRAY_INDEX] =  _eda_inst_array_index_handler,

	[SCF_OP_TYPE_CAST]   =  _eda_inst_cast_handler,
	[SCF_OP_LOGIC_NOT] 	 =  _eda_inst_logic_not_handler,
	[SCF_OP_BIT_NOT]     =  _eda_inst_bit_not_handler,

	[SCF_OP_3AC_INC]     =  _eda_inst_inc_handler,
	[SCF_OP_3AC_DEC]     =  _eda_inst_dec_handler,

	[SCF_OP_BIT_AND]     =  _eda_inst_bit_and_handler,
	[SCF_OP_BIT_OR]      =  _eda_inst_bit_or_handler,

	[SCF_OP_SHL]         =  _eda_inst_shl_handler,
	[SCF_OP_SHR]         =  _eda_inst_shr_handler,

	[SCF_OP_ADD]         =  _eda_inst_add_handler,
	[SCF_OP_SUB]         =  _eda_inst_sub_handler,
	[SCF_OP_MUL]         =  _eda_inst_mul_handler,
	[SCF_OP_DIV]         =  _eda_inst_div_handler,

	[SCF_OP_3AC_TEQ]     =  _eda_inst_teq_handler,
	[SCF_OP_3AC_CMP]     =  _eda_inst_cmp_handler,

	[SCF_OP_3AC_SETZ]    =  _eda_inst_setz_handler,
	[SCF_OP_3AC_SETNZ]   =  _eda_inst_setnz_handler,
	[SCF_OP_3AC_SETGT]   =  _eda_inst_setgt_handler,
	[SCF_OP_3AC_SETGE]   =  _eda_inst_setge_handler,
	[SCF_OP_3AC_SETLT]   =  _eda_inst_setlt_handler,
	[SCF_OP_3AC_SETLE]   =  _eda_inst_setle_handler,

	[SCF_OP_EQ] 		 =  _eda_inst_eq_handler,
	[SCF_OP_NE] 		 =  _eda_inst_ne_handler,
	[SCF_OP_GT] 		 =  _eda_inst_gt_handler,
	[SCF_OP_GE]          =  _eda_inst_ge_handler,
	[SCF_OP_LT]          =  _eda_inst_lt_handler,
	[SCF_OP_LE]          =  _eda_inst_le_handler,

	[SCF_OP_ASSIGN] 	 =  _eda_inst_assign_handler,

	[SCF_OP_AND_ASSIGN]  =  _eda_inst_and_assign_handler,
	[SCF_OP_OR_ASSIGN]   =  _eda_inst_or_assign_handler,

	[SCF_OP_RETURN] 	 =  _eda_inst_return_handler,
	[SCF_OP_GOTO] 		 =  _eda_inst_goto_handler,

	[SCF_OP_3AC_JZ] 	 =  _eda_inst_jz_handler,
	[SCF_OP_3AC_JNZ] 	 =  _eda_inst_jnz_handler,
	[SCF_OP_3AC_JGT] 	 =  _eda_inst_jgt_handler,
	[SCF_OP_3AC_JGE] 	 =  _eda_inst_jge_handler,
	[SCF_OP_3AC_JLT] 	 =  _eda_inst_jlt_handler,
	[SCF_OP_3AC_JLE] 	 =  _eda_inst_jle_handler,

	[SCF_OP_3AC_NOP] 	 =  _eda_inst_nop_handler,
	[SCF_OP_3AC_END] 	 =  _eda_inst_end_handler,

	[SCF_OP_3AC_SAVE]    =  _eda_inst_save_handler,
	[SCF_OP_3AC_LOAD]    =  _eda_inst_load_handler,

	[SCF_OP_3AC_RESAVE]  =  _eda_inst_save_handler,
	[SCF_OP_3AC_RELOAD]  =  _eda_inst_reload_handler,

	[SCF_OP_3AC_ASSIGN_ARRAY_INDEX] = _eda_inst_assign_array_index_handler,
};

eda_inst_handler_pt scf_eda_find_inst_handler(const int op_type)
{
	if (op_type < 0 || op_type >= SCF_N_3AC_OPS)
		return NULL;

	return eda_inst_handlers[op_type];
}
