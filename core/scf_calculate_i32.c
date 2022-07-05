#include"scf_calculate.h"

#define SCF_I32_BINARY_OP(name, op) \
int scf_i32_##name(scf_ast_t* ast, scf_variable_t** pret, scf_variable_t* src0, scf_variable_t* src1) \
{ \
	scf_type_t*     t = scf_block_find_type_type(ast->current_block, SCF_VAR_I32); \
	scf_variable_t* r = SCF_VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL); \
	if (!r) \
		return -ENOMEM; \
	r->data.i = src0->data.i op src1->data.i; \
	if (pret) \
		*pret = r; \
	return 0; \
}

#define SCF_I32_UNARY_OP(name, op) \
int scf_i32_##name(scf_ast_t* ast, scf_variable_t** pret, scf_variable_t* src0, scf_variable_t* src1) \
{ \
	scf_type_t*     t = scf_block_find_type_type(ast->current_block, SCF_VAR_I32); \
	scf_variable_t* r = SCF_VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL); \
	if (!r) \
		return -ENOMEM; \
	r->data.i = op src0->data.i; \
	if (pret) \
		*pret = r; \
	return 0; \
}

#define SCF_I32_UPDATE_OP(name, op) \
int scf_i32_##name(scf_ast_t* ast, scf_variable_t** pret, scf_variable_t* src0, scf_variable_t* src1) \
{ \
	op src0->data.i; \
	if (pret) \
		*pret = scf_variable_ref(src0); \
	return 0; \
}

SCF_I32_BINARY_OP(add, +)
SCF_I32_BINARY_OP(sub, -)
SCF_I32_BINARY_OP(mul, *)
SCF_I32_BINARY_OP(div, /)
SCF_I32_BINARY_OP(mod, %)

SCF_I32_BINARY_OP(shl, <<)
SCF_I32_BINARY_OP(shr, >>)

SCF_I32_UPDATE_OP(inc, ++);
SCF_I32_UPDATE_OP(dec, --);

SCF_I32_UNARY_OP(neg, -)

SCF_I32_BINARY_OP(bit_and, &)
SCF_I32_BINARY_OP(bit_or,  |)
SCF_I32_UNARY_OP(bit_not, ~)

SCF_I32_BINARY_OP(logic_and, &&)
SCF_I32_BINARY_OP(logic_or,  ||)
SCF_I32_UNARY_OP(logic_not, !)

SCF_I32_BINARY_OP(gt, >)
SCF_I32_BINARY_OP(lt, <)
SCF_I32_BINARY_OP(eq, ==)
SCF_I32_BINARY_OP(ne, !=)
SCF_I32_BINARY_OP(ge, >=)
SCF_I32_BINARY_OP(le, <=)

