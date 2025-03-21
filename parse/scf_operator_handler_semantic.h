#ifndef SCF_OPERATOR_HANDLER_SEMANTIC_H
#define SCF_OPERATOR_HANDLER_SEMANTIC_H

#include"scf_operator_handler.h"

scf_operator_handler_pt  scf_find_semantic_operator_handler(const int type);

int scf_function_semantic_analysis(scf_ast_t* ast, scf_function_t* f);

int scf_expr_semantic_analysis(scf_ast_t* ast, scf_expr_t* e);

int scf_semantic_analysis(scf_ast_t* ast);

#endif

