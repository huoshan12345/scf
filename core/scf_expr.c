#include"scf_parse.h"

scf_expr_t* scf_expr_alloc()
{
	scf_expr_t* e = calloc(1, sizeof(scf_expr_t));
	if (!e)
		return NULL;

	e->nodes = calloc(1, sizeof(scf_node_t*));
	if (!e->nodes) {
		free(e);
		return NULL;
	}

	e->type = SCF_OP_EXPR;
	return e;
}

int scf_expr_copy(scf_node_t* e2, scf_node_t* e)
{
	scf_node_t* node;
	int i;

	for (i = 0; i < e->nb_nodes; i++) {

		node = scf_node_clone(e->nodes[i]);
		if (!node)
			return -ENOMEM;

		scf_node_add_child(e2, node);

		int ret = scf_expr_copy(e2->nodes[i], e->nodes[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

scf_expr_t* scf_expr_clone(scf_node_t* e)
{
	scf_expr_t* e2 = scf_node_clone(e);
	if (!e2)
		return NULL;

	if (scf_expr_copy(e2, e) < 0) {
		scf_expr_free(e2);
		return NULL;
	}

	return e2;
}

void scf_expr_free(scf_expr_t* e)
{
	if (e) {
		scf_node_free(e);
		e = NULL;
	}
}

static int __expr_node_add_node(scf_node_t** pparent, scf_node_t* child)
{
	scf_node_t* parent = *pparent;
	if (!parent) {
		*pparent = child;
		return 0;
	}

	if (parent->priority > child->priority) {
		assert(parent->op);

		if (parent->op->nb_operands > parent->nb_nodes)
			return scf_node_add_child(parent, child);

		assert(parent->nb_nodes >= 1);
		return __expr_node_add_node(&(parent->nodes[parent->nb_nodes - 1]), child);

	} else if (parent->priority < child->priority) {
		assert(child->op);

		if (child->op->nb_operands > 0)
			assert(child->op->nb_operands > child->nb_nodes);

		child->parent = parent->parent;
		if (scf_node_add_child(child, parent) < 0)
			return -1;

		*pparent = child;
		return 0;
	}

	// parent->priority == child->priority
	scf_logi("parent: %p, parent->type: %d, FOR: %d, SCF_VAR_INT: %d\n", parent, parent->type, SCF_OP_FOR, SCF_VAR_INT);
	assert(parent->op);
	assert(child->op);

	if (SCF_OP_ASSOCIATIVITY_LEFT == child->op->associativity) {
		if (child->op->nb_operands > 0)
			assert(child->op->nb_operands > child->nb_nodes);

		child->parent = parent->parent;

		scf_node_add_child(child, parent); // add parent to child's child node

		*pparent = child; // child is the new parent node
		return 0;
	}

	if (parent->op->nb_operands > parent->nb_nodes)
		return scf_node_add_child(parent, child);

	assert(parent->nb_nodes >= 1);
	return __expr_node_add_node(&(parent->nodes[parent->nb_nodes - 1]), child);
}

int scf_expr_add_node(scf_expr_t* e, scf_node_t* node)
{
	assert(e);
	assert(node);

	if (scf_type_is_var(node->type))
		node->priority = -1;

	else if (scf_type_is_operator(node->type)) {

		node->op = scf_find_base_operator_by_type(node->type);
		if (!node->op) {
			scf_loge("\n");
			return -1;
		}
		node->priority = node->op->priority;
	} else {
		scf_loge("\n");
		return -1;
	}

	if (__expr_node_add_node(&(e->nodes[0]), node) < 0)
		return -1;

	e->nodes[0]->parent = e;
	e->nb_nodes = 1;
	return 0;	
}

void scf_expr_simplify(scf_expr_t** pe)
{
	scf_expr_t** pp = pe;
	scf_expr_t*  e;

	while (SCF_OP_EXPR == (*pp)->type) {
		e  = *pp;
		pp = &(e->nodes[0]);
	}

	if (pp != pe) {
		e   = *pp;
		*pp = NULL;

		e->parent = (*pe)->parent;

		scf_expr_free(*pe);
		*pe = e;
	}
}
