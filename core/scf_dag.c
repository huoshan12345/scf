#include"scf_dag.h"
#include"scf_3ac.h"

scf_dn_index_t* scf_dn_index_alloc()
{
	scf_dn_index_t* di = calloc(1, sizeof(scf_dn_index_t));
	if (!di)
		return NULL;

	di->refs = 1;
	return di;
}

scf_dn_index_t* scf_dn_index_ref(scf_dn_index_t* di)
{
	if (di)
		di->refs++;
	return di;
}

scf_dn_index_t* scf_dn_index_clone(scf_dn_index_t* di)
{
	if (!di)
		return NULL;

	scf_dn_index_t* di2 = scf_dn_index_alloc();
	if (!di2)
		return NULL;

	di2->member   = di->member;
	di2->index    = di->index;
	di2->dn       = di->dn;
	di2->dn_scale = di->dn_scale;

	return di2;
}

void scf_dn_index_free(scf_dn_index_t* di)
{
	if (di && 0 == --di->refs)
		free(di);
}

int scf_dn_index_same(const scf_dn_index_t* di0, const scf_dn_index_t* di1)
{
	if (di0->member != di1->member)
		return 0;

	if (di0->member)
		return 1;

	if (-1 == di0->index || -1 == di1->index)
		return 0;

	if (di0->index == di1->index)
		return 1;
	return 0;
}

int scf_dn_index_like(const scf_dn_index_t* di0, const scf_dn_index_t* di1)
{
	if (di0->member != di1->member)
		return 0;

	if (di0->member)
		return 1;

	if (-1 == di0->index || -1 == di1->index)
		return 1;

	if (di0->index == di1->index)
		return 1;
	return 0;
}

int scf_dn_status_is_like(const scf_dn_status_t* ds)
{
	if (!ds->dn_indexes)
		return 0;

	scf_dn_index_t* di;
	int i;
	for (i = 0; i < ds->dn_indexes->size; i++) {

		di = ds->dn_indexes->data[i];

		if (di->member)
			continue;

		if (-1 == di->index)
			return 1;
	}
	return 0;
}

scf_dn_status_t* scf_dn_status_null()
{
	scf_dn_status_t* ds = calloc(1, sizeof(scf_dn_status_t));
	if (!ds)
		return NULL;

	ds->refs = 1;
	return ds;
}

scf_dn_status_t* scf_dn_status_alloc(scf_dag_node_t* dn)
{
	scf_dn_status_t* ds = calloc(1, sizeof(scf_dn_status_t));
	if (!ds)
		return NULL;

	ds->refs     = 1;
	ds->dag_node = dn;

	if (dn) {
		ds->active   = dn->active;
		ds->inited   = dn->inited;
		ds->updated  = dn->updated;
	}

	return ds;
}

int scf_ds_copy_dn(scf_dn_status_t* dst, scf_dn_status_t* src)
{
	scf_dn_index_t* di;
	int i;

	if (!dst || !src)
		return -1;

	dst->dag_node = src->dag_node;

	if (dst->dn_indexes) {
		scf_vector_clear(dst->dn_indexes, (void (*)(void*))scf_dn_index_free);
		scf_vector_free (dst->dn_indexes);
		dst->dn_indexes = NULL;
	}

	if (src->dn_indexes) {
		dst->dn_indexes = scf_vector_clone(src->dn_indexes);

		if (!dst->dn_indexes)
			return -ENOMEM;

		for (i = 0; i < dst->dn_indexes->size; i++) {
			di =        dst->dn_indexes->data[i];
			di->refs++;
		}
	}
	return 0;
}

int scf_ds_copy_alias(scf_dn_status_t* dst, scf_dn_status_t* src)
{
	scf_dn_index_t* di;
	int i;

	if (!dst || !src)
		return -1;

	dst->alias      = src->alias;
	dst->alias_type = src->alias_type;

	if (dst->alias_indexes) {
		scf_vector_clear(dst->alias_indexes, (void (*)(void*))scf_dn_index_free);
		scf_vector_free (dst->alias_indexes);
		dst->alias_indexes = NULL;
	}

	if (src->alias_indexes) {
		dst->alias_indexes = scf_vector_clone(src->alias_indexes);

		if (!dst->alias_indexes)
			return -ENOMEM;

		for (i = 0; i < dst->alias_indexes->size; i++) {
			di =        dst->alias_indexes->data[i];
			di->refs++;
		}
	}
	return 0;
}

scf_dn_status_t* scf_dn_status_clone(scf_dn_status_t* ds)
{
	scf_dn_status_t* ds2;
	scf_dn_index_t*  di;
	int i;

	if (!ds)
		return NULL;

	ds2 = calloc(1, sizeof(scf_dn_status_t));
	if (!ds2)
		return NULL;

	ds2->refs = 1;

	if (ds ->dn_indexes) {
		ds2->dn_indexes = scf_vector_clone(ds->dn_indexes);

		if (!ds2->dn_indexes) {
			scf_dn_status_free(ds2);
			return NULL;
		}

		for (i = 0; i < ds2->dn_indexes->size; i++) {
			di =        ds2->dn_indexes->data[i];
			di->refs++;
		}
	}

	if (ds ->alias_indexes) {
		ds2->alias_indexes = scf_vector_clone(ds->alias_indexes);

		if (!ds2->alias_indexes) {
			scf_dn_status_free(ds2);
			return NULL;
		}

		for (i = 0; i < ds2->alias_indexes->size; i++) {
			di =        ds2->alias_indexes->data[i];
			di->refs++;
		}
	}

	ds2->dag_node    = ds->dag_node;
	ds2->dereference = ds->dereference;

	ds2->alias       = ds->alias;
	ds2->alias_type  = ds->alias_type;

	ds2->active      = ds->active;
	ds2->inited      = ds->inited;
	ds2->updated     = ds->updated;

	ds2->ret_flag    = ds->ret_flag;
	ds2->ret_index   = ds->ret_index;
	return ds2;
}

scf_dn_status_t* scf_dn_status_ref(scf_dn_status_t* ds)
{
	if (ds)
		ds->refs++;
	return ds;
}

void scf_dn_status_free(scf_dn_status_t* ds)
{
	scf_dn_index_t* di;
	int i;

	if (ds) {
		assert(ds->refs > 0);

		if (--ds->refs > 0)
			return;

		if (ds->dn_indexes) {
			scf_vector_clear(ds->dn_indexes, (void (*)(void*))scf_dn_index_free);
			scf_vector_free (ds->dn_indexes);
		}

		if (ds->alias_indexes) {
			scf_vector_clear(ds->alias_indexes, (void (*)(void*))scf_dn_index_free);
			scf_vector_free (ds->alias_indexes);
		}

		free(ds);
	}
}

void scf_dn_status_print(scf_dn_status_t* ds)
{
	scf_dn_index_t* di;
	scf_variable_t* v;
	int i;

	if (ds->dag_node) {
		v = ds->dag_node->var;
		printf("dn: v_%d_%d/%s ", v->w->line, v->w->pos, v->w->text->data);

		if (ds->dn_indexes) {
			for (i = ds->dn_indexes->size - 1; i >= 0; i--) {
				di = ds->dn_indexes->data[i];

				if (di->member)
					printf("->%s ", di->member->w->text->data);
				else
					printf("[%ld] ", di->index);
			}
		}
	}

	if (ds->alias) {
		v = ds->alias->var;
		printf(" alias: v_%d_%d/%s ", v->w->line, v->w->pos, v->w->text->data);

		if (ds->alias_indexes) {
			for (i = ds->alias_indexes->size - 1; i >= 0; i--) {
				di = ds->alias_indexes->data[i];

				if (di->member)
					printf("->%s ", di->member->w->text->data);
				else
					printf("[%ld] ", di->index);
			}
		}
	}

	printf(" alias_type: %d\n", ds->alias_type);
}

scf_dag_node_t* scf_dag_node_alloc(int type, scf_variable_t* var, const scf_node_t* node)
{
	scf_dag_node_t* dn = calloc(1, sizeof(scf_dag_node_t));
	if (!dn)
		return NULL;

	dn->type = type;
	if (var)
		dn->var = scf_variable_ref(var);
	else
		dn->var = NULL;

	dn->node = (scf_node_t*)node;

#if 0
	if (SCF_OP_CALL == type) {
		scf_logw("dn: %#lx, dn->type: %d", 0xffff & (uintptr_t)dn, dn->type);
		if (var) {
			printf(", var: %#lx, var->type: %d", 0xffff & (uintptr_t)var, var->type);
			if (var->w)
				printf(", v_%d_%d/%s", var->w->line, var->w->pos, var->w->text->data);
		}
		printf("\n");
	}
#endif
	return dn;
}

int scf_dag_node_find_child(scf_dag_node_t* parent, scf_dag_node_t* child)
{
	if (parent->childs
			&& scf_vector_find(parent->childs, child))
		return 1;
	return 0;
}

int scf_dag_node_add_child(scf_dag_node_t* parent, scf_dag_node_t* child)
{
	if (!parent || !child)
		return -EINVAL;

	if (!parent->childs) {
		parent->childs = scf_vector_alloc();
		if (!parent->childs)
			return -ENOMEM;
	}

	if (!child->parents) {
		child->parents = scf_vector_alloc();
		if (!child->parents)
			return -ENOMEM;
	}

	int ret = scf_vector_add(parent->childs, child);
	if (ret < 0)
		return ret;

	ret = scf_vector_add(child->parents, parent);
	if (ret < 0) {
		scf_vector_del(parent->childs, child);
		return ret;
	}

	return 0;
}

void scf_dag_node_free(scf_dag_node_t* dn)
{
	if (dn) {
		if (dn->var)
			scf_variable_free(dn->var);

		if (dn->parents)
			scf_vector_free(dn->parents);

		if (dn->childs)
			scf_vector_free(dn->childs);

		free(dn);
		dn = NULL;
	}
}

static int _ds_cmp_indexes(const void* p0, const void* p1,
		int (*cmp)(const scf_dn_index_t*, const scf_dn_index_t*))
{
	const scf_dn_status_t* ds0 = p0;
	const scf_dn_status_t* ds1 = p1;

	if (ds0->dag_node != ds1->dag_node)
		return -1;

	if (ds0->dn_indexes) {
		if (!ds1->dn_indexes)
			return -1;

		if (ds0->dn_indexes->size != ds1->dn_indexes->size)
			return -1;

		scf_dn_index_t* di0;
		scf_dn_index_t* di1;
		int i;

		for (i = 0; i < ds0->dn_indexes->size; i++) {
			di0       = ds0->dn_indexes->data[i];
			di1       = ds1->dn_indexes->data[i];

			if (!cmp(di0, di1))
				return -1;
		}
		return 0;
	} else if (ds1->dn_indexes)
		return -1;
	return 0;
}

int scf_ds_cmp_same_indexes(const void* p0, const void* p1)
{
	return _ds_cmp_indexes(p0, p1, scf_dn_index_same);
}

int scf_ds_cmp_like_indexes(const void* p0, const void* p1)
{
	return _ds_cmp_indexes(p0, p1, scf_dn_index_like);
}

int scf_ds_cmp_alias(const void* p0, const void* p1)
{
	const scf_dn_status_t* v0 = p0;
	const scf_dn_status_t* v1 = p1;

	assert(SCF_DN_ALIAS_NULL != v0->alias_type);
	assert(SCF_DN_ALIAS_NULL != v1->alias_type);

	if (v0->alias_type != v1->alias_type)
		return -1;

	if (SCF_DN_ALIAS_ALLOC == v0->alias_type)
		return 0;

	if (v0->alias != v1->alias)
		return -1;

	switch (v0->alias_type) {

		case SCF_DN_ALIAS_VAR:
			return 0;
			break;

		case SCF_DN_ALIAS_ARRAY:
		case SCF_DN_ALIAS_MEMBER:
			if (v0->alias_indexes) {
				if (!v1->alias_indexes)
					return -1;

				if (v0->alias_indexes->size != v1->alias_indexes->size)
					return -1;

				scf_dn_index_t* di0;
				scf_dn_index_t* di1;
				int i;

				for (i = 0; i < v0->alias_indexes->size; i++) {
					di0       = v0->alias_indexes->data[i];
					di1       = v1->alias_indexes->data[i];

					if (!scf_dn_index_like(di0, di1))
						return -1;
				}
				return 0;
			} else if (v1->alias_indexes)
				return -1;
			return 0;
			break;

		default:
			break;
	};

	return -1;
}

void scf_dag_node_free_list(scf_list_t* dag_list_head)
{
	scf_dag_node_t* dn;
	scf_list_t*     l;

	for (l = scf_list_head(dag_list_head); l != scf_list_sentinel(dag_list_head); ) {
		dn = scf_list_data(l, scf_dag_node_t, list);

		l = scf_list_next(l);

		scf_list_del(&dn->list);

		scf_dag_node_free(dn);
		dn = NULL;
	}
}

static int __dn_same_call(scf_dag_node_t* dn, const scf_node_t* node, const scf_node_t* split)
{
	scf_variable_t* v0 = _scf_operand_get(node);
	scf_variable_t* v1 = dn->var;

	if (split)
		v0 = _scf_operand_get(split);

	if (v0 && v0->w && v1 && v1->w) {
		if (v0->type != v1->type) {
			scf_logd("v0: %d/%s_%#lx, split_flag: %d\n", v0->w->line, v0->w->text->data, 0xffff & (uintptr_t)v0, node->split_flag);
			scf_logd("v1: %d/%s_%#lx\n", v1->w->line, v1->w->text->data, 0xffff & (uintptr_t)v1);
		}
	}

	return v0 == v1;
}

int scf_dag_node_same(scf_dag_node_t* dn, const scf_node_t* node)
{
	int i;

	const scf_node_t* split = NULL;

	if (node->split_flag) {
		if (dn->var != _scf_operand_get(node))
			return 0;

		split = node;
		node  = node->split_parent;

		scf_logd("split type: %d, node: %#lx, var: %#lx\n", split->type,    0xffff & (uintptr_t)split,    0xffff & (uintptr_t)split->var);
		scf_logd("node  type: %d, node: %#lx, var: %#lx\n", node->type,     0xffff & (uintptr_t)node,     0xffff & (uintptr_t)node->var);
		scf_logd("dag   type: %d, node: %#lx, var: %#lx\n", dn->type, 0xffff & (uintptr_t)dn, 0xffff & (uintptr_t)dn->var);
	}

	if (dn->type != node->type)
		return 0;

	if (SCF_OP_ADDRESS_OF == node->type)
		scf_logd("type: %d, %d, node: %#lx, %#lx, ", dn->type, node->type, 0xffff & (uintptr_t)dn, 0xffff & (uintptr_t)node);

	if (scf_type_is_var(node->type)) {
		if (dn->var == node->var)
			return 1;
		else
			return 0;
	}

	if (SCF_OP_LOGIC_AND == node->type
			|| SCF_OP_LOGIC_OR == node->type
			|| SCF_OP_INC      == node->type
			|| SCF_OP_DEC      == node->type
			|| SCF_OP_INC_POST == node->type
			|| SCF_OP_DEC_POST == node->type
			|| SCF_OP_ADDRESS_OF == node->type) {
		if (dn->var == _scf_operand_get((scf_node_t*)node))
			return 1;
		return 0;
	}

	if (!dn->childs) {
		if (SCF_OP_CALL == node->type && 1 == node->nb_nodes)
			return __dn_same_call(dn, node, split);
		return 0;
	}

	if (SCF_OP_TYPE_CAST == node->type) {
		scf_dag_node_t* dn0 = dn->childs->data[0];
		scf_variable_t* vn1 = _scf_operand_get(node->nodes[1]);
		scf_node_t*     n1  = node->nodes[1];

		while (SCF_OP_EXPR == n1->type)
			n1 = n1->nodes[0];

		if (scf_dag_node_same(dn0, n1)
				&& scf_variable_same_type(dn->var, node->result))
			return 1;
		else {
			scf_logd("var: %#lx, %#lx, type: %d, %d, node: %#lx, %#lx, same: %d\n",
					0xffff & (uintptr_t)dn0->var,
					0xffff & (uintptr_t)vn1,
					dn->var->type, node->result->type,
					0xffff & (uintptr_t)dn,
					0xffff & (uintptr_t)node,
					scf_variable_same_type(dn->var, node->result));
			return 0;
		}
	} else if (SCF_OP_ARRAY_INDEX == node->type) {
		assert(3 == dn->childs->size);
		assert(2 == node->nb_nodes);
		goto cmp_childs;

	} else if (SCF_OP_ADDRESS_OF == node->type) {
		assert(1 == node->nb_nodes);

		if (SCF_OP_ARRAY_INDEX == node->nodes[0]->type) {
			assert(2 == node->nodes[0]->nb_nodes);

			if (!dn->childs || 3 != dn->childs->size)
				return 0;

			node = node->nodes[0];
			goto cmp_childs;

		} else if (SCF_OP_POINTER == node->nodes[0]->type) {
			assert(2 == node->nodes[0]->nb_nodes);

			if (!dn->childs || 2 != dn->childs->size)
				return 0;

			node = node->nodes[0];
			goto cmp_childs;
		}
	}

	if (dn->childs->size != node->nb_nodes)
		return 0;

cmp_childs:
	for (i = 0; i < node->nb_nodes; i++) {
		scf_node_t* child = node->nodes[i];

		while (SCF_OP_EXPR == child->type)
			child = child->nodes[0];

		if (0 == scf_dag_node_same(dn->childs->data[i], child))
			return 0;
	}

	for (i = 0; i < dn->childs->size; i++) {
		scf_dag_node_t*	child  = dn->childs->data[i];
		scf_dag_node_t* parent = NULL;

		assert(child->parents);

		int j;
		for (j = 0; j < child->parents->size; j++) {
			if (dn  ==  child->parents->data[j])
				break;
		}
		assert(j < child->parents->size);

		for (++j; j < child->parents->size; j++) {
			parent  = child->parents->data[j];

			if (SCF_OP_INC == parent->type
					|| SCF_OP_DEC       == parent->type
					|| SCF_OP_3AC_SETZ  == parent->type
					|| SCF_OP_3AC_SETNZ == parent->type
					|| SCF_OP_3AC_SETLT == parent->type
					|| SCF_OP_3AC_SETLE == parent->type
					|| SCF_OP_3AC_SETGT == parent->type
					|| SCF_OP_3AC_SETGE == parent->type)
				return 0;

			if (scf_type_is_assign(parent->type)) {
				if (child == parent->childs->data[0])
					return 0;
			}
		}
	}

	if (SCF_OP_CALL == dn->type)
		return __dn_same_call(dn, node, split);

	return 1;
}

scf_dag_node_t* scf_dag_find_node(scf_list_t* h, const scf_node_t* node)
{
	scf_dag_node_t* dn;
	scf_list_t*     l;
	scf_node_t*     origin = (scf_node_t*)node;

	while (SCF_OP_EXPR == origin->type)
		origin = origin->nodes[0];

	for (l = scf_list_tail(h); l != scf_list_sentinel(h); l = scf_list_prev(l)) {
		dn = scf_list_data(l, scf_dag_node_t, list);

		if (scf_dag_node_same(dn, origin))
			return dn;
	}

	return NULL;
}

int scf_dag_get_node(scf_list_t* h, const scf_node_t* node, scf_dag_node_t** pp)
{
	const scf_node_t* node2;
	scf_variable_t*   v;
	scf_dag_node_t*   dn;

	if (*pp)
		node2 = (*pp)->node;
	else
		node2 = node;

	v  = _scf_operand_get((scf_node_t*)node2);

	dn = scf_dag_find_node(h, node2);

	if (!dn) {
		dn = scf_dag_node_alloc(node2->type, v, node2);
		if (!dn)
			return -ENOMEM;

		scf_list_add_tail(h, &dn->list);

		dn->old = *pp;
	} else {
		dn->var->local_flag |= v->local_flag;
		dn->var->tmp_flag   |= v->tmp_flag;
	}

	*pp = dn;
	return 0;
}

int scf_dag_dn_same(scf_dag_node_t* dn0, scf_dag_node_t* dn1)
{
	int i;

	if (dn0->type != dn1->type)
		return 0;

	if (scf_type_is_var(dn0->type)) {
		if (dn0->var == dn1->var)
			return 1;
		else
			return 0;
	}

	if (!dn0->childs) {
		if (dn1->childs)
			return 0;

		if (dn0->var == dn1->var)
			return 1;
		scf_logd("dn0: %p, %p, dn1: %p, %p, var: %p, %p\n", dn0, dn0->childs, dn1, dn1->childs, dn0->var, dn1->var);
		return 0;
	} else if (!dn1->childs)
		return 0;

	if (dn0->childs->size != dn1->childs->size) {
		scf_logd("dn0: %p, %d, dn1: %p, %d\n", dn0, dn0->childs->size, dn1, dn1->childs->size);
		return 0;
	}

	for (i = 0; i < dn0->childs->size; i++) {
		scf_dag_node_t*	child0 = dn0->childs->data[i];
		scf_dag_node_t*	child1 = dn1->childs->data[i];

		if (2 == i && scf_type_is_assign_array_index(dn0->type))
			continue;
		if (2 == i && SCF_OP_ARRAY_INDEX == dn0->type)
			continue;

		if (0 == scf_dag_dn_same(child0, child1))
			return 0;
	}

	return 1;
}

int scf_dag_find_roots(scf_list_t* h, scf_vector_t* roots)
{
	scf_dag_node_t* dn;
	scf_list_t*     l;

	for (l = scf_list_head(h); l != scf_list_sentinel(h); l = scf_list_next(l)) {
		dn = scf_list_data(l, scf_dag_node_t, list);

		if (!dn->parents) {
			int ret = scf_vector_add(roots, dn);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int _dn_status_index(scf_vector_t* indexes, scf_dag_node_t* dn_index, int type)
{
	scf_dn_index_t* di;

	di = scf_dn_index_alloc();
	if (!di)
		return -ENOMEM;

	if (SCF_OP_ARRAY_INDEX == type) {

		if (scf_variable_const(dn_index->var))
			di->index = dn_index->var->data.i;
		else
			di->index = -1;
		di->dn = dn_index;

	} else if (SCF_OP_POINTER == type) {
		di->member = dn_index->var;
		di->dn     = dn_index;
	} else {
		scf_dn_index_free(di);
		return -1;
	}

	int ret = scf_vector_add(indexes, di);
	if (ret < 0) {
		scf_dn_index_free(di);
		return ret;
	}
	return 0;
}

int scf_dn_status_index(scf_dn_status_t* ds, scf_dag_node_t* dn_index, int type)
{
	return _dn_status_index(ds->dn_indexes, dn_index, type);
}

int scf_dn_status_alias_index(scf_dn_status_t* ds, scf_dag_node_t* dn_index, int type)
{
	return _dn_status_index(ds->alias_indexes, dn_index, type);
}

void scf_ds_vector_clear_by_ds(scf_vector_t* vec, scf_dn_status_t* ds)
{
	scf_dn_status_t* ds2;

	while (1) {
		ds2 = scf_vector_find_cmp(vec, ds, scf_ds_cmp_same_indexes);
		if (!ds2)
			break;

		assert(0 == scf_vector_del(vec, ds2));

		scf_dn_status_free(ds2);
		ds2 = NULL;
	}
}

void scf_ds_vector_clear_by_dn(scf_vector_t* vec, scf_dag_node_t* dn)
{
	scf_dn_status_t* ds;

	while (1) {
		ds = scf_vector_find_cmp(vec, dn, scf_dn_status_cmp);
		if (!ds)
			break;

		assert(0 == scf_vector_del(vec, ds));

		scf_dn_status_free(ds);
		ds = NULL;
	}
}

static int __ds_for_dn(scf_dn_status_t* ds, scf_dag_node_t* dn_base)
{
	scf_dag_node_t*  dn_index;
	scf_dag_node_t*  dn_scale;
	scf_dn_index_t*  di;

	int ret;

	if (!ds || !dn_base || !ds->dn_indexes)
		return -EINVAL;

	while (SCF_OP_DEREFERENCE == dn_base->type) {

		di = scf_dn_index_alloc();
		if (!di)
			return -ENOMEM;
		di->index = 0;

		ret = scf_vector_add(ds->dn_indexes, di);
		if (ret < 0) {
			scf_dn_index_free(di);
			return -ENOMEM;
		}

		dn_base  = dn_base->childs->data[0];
	}

	while (SCF_OP_ARRAY_INDEX == dn_base->type
			|| SCF_OP_POINTER == dn_base->type) {

		dn_index = dn_base->childs->data[1];

		ret = scf_dn_status_index(ds, dn_index, dn_base->type);
		if (ret < 0)
			return ret;

		if (SCF_OP_ARRAY_INDEX == dn_base->type) {

			di           = ds->dn_indexes->data[ds->dn_indexes->size - 1];

			di->dn_scale = dn_base->childs->data[2];
			assert(di->dn_scale);
		}

		dn_base  = dn_base->childs->data[0];
	}

/*	scf_loge("dn_base->type: %d\n", dn_base->type);
	assert(scf_type_is_var(dn_base->type)
			|| SCF_OP_INC == dn_base->type || SCF_OP_INC_POST == dn_base->type
			|| SCF_OP_DEC == dn_base->type || SCF_OP_DEC_POST == dn_base->type
			|| SCF_OP_CALL == dn_base->type
			|| SCF_OP_ADDRESS_OF == dn_base->type);
*/
	ds->dag_node = dn_base;
	return 0;
}

int scf_ds_for_dn(scf_dn_status_t** pds, scf_dag_node_t* dn)
{
	scf_dn_status_t*  ds;

	ds = scf_dn_status_null();
	if (!ds)
		return -ENOMEM;

	ds->dn_indexes = scf_vector_alloc();
	if (!ds->dn_indexes) {
		free(ds);
		return -ENOMEM;
	}

	int ret = __ds_for_dn(ds, dn);
	if (ret < 0) {
		scf_dn_status_free(ds);
		return ret;
	}

	if (0 == ds->dn_indexes->size) {
		scf_vector_free(ds->dn_indexes);
		ds->dn_indexes = NULL;
	}

	*pds = ds;
	return 0;
}

int scf_ds_for_assign_member(scf_dn_status_t** pds, scf_dag_node_t* dn_base, scf_dag_node_t* dn_member)
{
	scf_dn_status_t*  ds;
	scf_dn_index_t*   di;
	int i;

	ds = scf_dn_status_null();
	if (!ds)
		return -ENOMEM;

	ds->dn_indexes = scf_vector_alloc();
	if (!ds->dn_indexes) {
		free(ds);
		return -ENOMEM;
	}

	int ret = __ds_for_dn(ds, dn_base);
	if (ret < 0) {
		scf_dn_status_free(ds);
		return ret;
	}

	ret = scf_dn_status_index(ds, dn_member, SCF_OP_POINTER);
	if (ret < 0)
		return ret;

	di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

	for (i = ds->dn_indexes->size - 2; i >= 0; i--)
		ds->dn_indexes->data[i + 1] = ds->dn_indexes->data[i];

	ds->dn_indexes->data[0] = di;

	*pds = ds;
	return 0;
}

int scf_ds_for_assign_array_member(scf_dn_status_t** pds, scf_dag_node_t* dn_base, scf_dag_node_t* dn_index, scf_dag_node_t* dn_scale)
{
	scf_dn_status_t*  ds;
	scf_dn_index_t*   di;
	int i;

	ds = scf_dn_status_null();
	if (!ds)
		return -ENOMEM;

	ds->dn_indexes = scf_vector_alloc();
	if (!ds->dn_indexes) {
		free(ds);
		return -ENOMEM;
	}

	int ret = __ds_for_dn(ds, dn_base);
	if (ret < 0) {
		scf_dn_status_free(ds);
		return ret;
	}

	ret = scf_dn_status_index(ds, dn_index, SCF_OP_ARRAY_INDEX);
	if (ret < 0)
		return ret;

	di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

	di->dn_scale = dn_scale;

	for (i = ds->dn_indexes->size - 2; i >= 0; i--)
		ds->dn_indexes->data[i + 1] = ds->dn_indexes->data[i];

	ds->dn_indexes->data[0] = di;

	*pds = ds;
	return 0;
}

int scf_ds_for_assign_dereference(scf_dn_status_t** pds, scf_dag_node_t* dn)
{
	scf_dn_status_t*  ds;
	scf_dag_node_t*   dn_index;
	scf_dn_index_t*   di;

	ds = scf_dn_status_null();
	if (!ds)
		return -ENOMEM;

	ds->dn_indexes = scf_vector_alloc();
	if (!ds->dn_indexes) {
		free(ds);
		return -ENOMEM;
	}

	di = scf_dn_index_alloc();
	if (!di) {
		scf_dn_status_free(ds);
		return -ENOMEM;
	}
	di->index = 0;

	int ret = scf_vector_add(ds->dn_indexes, di);
	if (ret < 0) {
		scf_dn_index_free(di);
		scf_dn_status_free(ds);
		return ret;
	}

	ret = __ds_for_dn(ds, dn);
	if (ret < 0) {
		scf_dn_status_free(ds);
		return ret;
	}

	*pds = ds;
	return 0;
}
