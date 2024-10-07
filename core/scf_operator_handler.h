#ifndef SCF_OPERATOR_HANDLER_H
#define SCF_OPERATOR_HANDLER_H

#include"scf_ast.h"

typedef int	(*scf_operator_handler_pt)(scf_ast_t* ast, scf_node_t** nodes, int nb_nodes, void* data);

scf_operator_handler_pt  scf_find_3ac_operator_handler(const int type);

int scf_function_to_3ac(scf_ast_t* ast, scf_function_t* f, scf_list_t* _3ac_list_head);

#endif
