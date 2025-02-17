#ifndef SCF_EXPR_H
#define SCF_EXPR_H

#include"scf_node.h"

scf_expr_t*  scf_expr_alloc();
scf_expr_t*  scf_expr_clone(scf_expr_t* e);
void         scf_expr_free (scf_expr_t* e);

int          scf_expr_add_node(scf_expr_t*  e, scf_node_t* node);
void         scf_expr_simplify(scf_expr_t** pe);

#endif
