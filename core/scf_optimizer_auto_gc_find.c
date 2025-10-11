#include"scf_optimizer.h"
#include"scf_pointer_alias.h"

#include"scf_auto_gc_find.c"

static int _bb_add_ds(scf_basic_block_t* bb, scf_dn_status_t* ds_obj)
{
	scf_dn_status_t*  ds;

	ds = scf_vector_find_cmp(bb->ds_freed, ds_obj, scf_ds_cmp_same_indexes);
	if (ds) {
		assert(0 == scf_vector_del(bb->ds_freed, ds));

		scf_dn_status_free(ds);
		ds = NULL;
	}

	ds = scf_vector_find_cmp(bb->ds_malloced, ds_obj, scf_ds_cmp_same_indexes);
	if (!ds) {
		ds = scf_vector_find_cmp(bb->ds_malloced, ds_obj, scf_ds_cmp_like_indexes);

		if (!ds) {
			ds = scf_dn_status_clone(ds_obj);
			if (!ds)
				return -ENOMEM;

			int ret = scf_vector_add(bb->ds_malloced, ds);
			if (ret < 0) {
				scf_dn_status_free(ds);
				return ret;
			}
			return 0;
		}
	}

	ds->ret_flag |= ds_obj->ret_flag;
	ds->ret_index = ds_obj->ret_index;
	return 0;
}

static int _bb_del_ds(scf_basic_block_t* bb, scf_dn_status_t* ds_obj)
{
	scf_dn_status_t*  ds;

	if (!scf_vector_find_cmp(bb->ds_freed, ds_obj, scf_ds_cmp_same_indexes)) {

		ds = scf_vector_find_cmp(bb->ds_freed, ds_obj, scf_ds_cmp_like_indexes);
		if (!ds) {
			ds = scf_dn_status_clone(ds_obj);
			if (!ds)
				return -ENOMEM;

			int ret = scf_vector_add(bb->ds_freed, ds);
			if (ret < 0) {
				scf_dn_status_free(ds);
				return ret;
			}
		}
	}

	ds = scf_vector_find_cmp(bb->ds_malloced, ds_obj, scf_ds_cmp_same_indexes);
	if (ds) {
		assert(0 == scf_vector_del(bb->ds_malloced, ds));

		scf_dn_status_free(ds);
		ds = NULL;
	}
	return 0;
}

static int __ds_append_index(scf_dn_status_t* dst, scf_dn_status_t* src)
{
	scf_dn_index_t* di;
	scf_dn_index_t* di2;
	int j;

	for (j  = 0; j < src->dn_indexes->size; j++) {
		di2 =        src->dn_indexes->data[j];

		di = scf_dn_index_alloc();
		if (!di)
			return -ENOMEM;

		di->index  = di2->index;
		di->member = di2->member;
		di->dn     = di2->dn;
#if 0
		if (di2->dn) {
			di ->dn = scf_dag_node_alloc(di2->dn->type, di2->dn->var, di2->dn->node);
			if (!di->dn) {
				scf_dn_index_free (di);
				return -ENOMEM;
			}
		}
#endif
		int ret = scf_vector_add_front(dst->dn_indexes, di);
		if (ret < 0) {
			scf_dn_index_free(di);
			return ret;
		}
	}
	return 0;
}

static int _bb_add_ds_for_call(scf_basic_block_t* bb, scf_dn_status_t* ds_obj, scf_function_t* f2, scf_variable_t* arg)
{
	scf_list_t*        l2  = scf_list_tail(&f2->basic_block_list_head);
	scf_basic_block_t* bb2 = scf_list_data(l2, scf_basic_block_t, list);

	scf_dn_status_t*   ds;
	scf_dn_status_t*   ds2;
	scf_dn_index_t*    di;
	scf_variable_t*    v;
	scf_variable_t*    v2;

	int i;
	int j;
	for (i  = 0; i < bb2->ds_malloced->size; i++) {
		ds2 =        bb2->ds_malloced->data[i];

		if (ds2->dag_node->var != arg)
			continue;

		if (!ds2->dn_indexes) {
			_bb_add_ds(bb, ds_obj);
			continue;
		}

		ds = scf_dn_status_clone(ds_obj);
		if (!ds)
			return -ENOMEM;

		if (!ds->dn_indexes) {
			ds ->dn_indexes = scf_vector_alloc();

			if (!ds->dn_indexes) {
				scf_dn_status_free(ds);
				return -ENOMEM;
			}
		}

		int ret = __ds_append_index(ds, ds2);
		if (ret < 0) {
			scf_dn_status_free(ds);
			return ret;
		}

		v  = ds ->dag_node->var;
		v2 = ds2->dag_node->var;

		int m = v ->nb_pointers + v ->nb_dimentions + (v ->type >= SCF_STRUCT);
		int n = v2->nb_pointers + v2->nb_dimentions + (v2->type >= SCF_STRUCT);

		for (j = 0; j + m < n; j++) {
			di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

			if (di->member || 0 == di->index) {
				assert(!di->dn);

				--ds->dn_indexes->size;
				scf_dn_index_free(di);
				di = NULL;
			} else{
				scf_dn_status_free(ds);
				return -ENOMEM;
			}
		}

		if (ds->dn_indexes->size <= 0) {
			scf_vector_free(ds->dn_indexes);
			ds->dn_indexes = NULL;
		}

		_bb_add_ds(bb, ds);

		scf_dn_status_free(ds);
		ds = NULL;
	}

	return 0;
}

static int __bb_add_ds_append(scf_basic_block_t* bb, scf_dn_status_t* ds_obj, scf_basic_block_t* __bb, scf_dn_status_t* __ds)
{
	scf_dn_status_t*  ds;
	scf_dn_status_t*  ds2;
	scf_dn_index_t*   di;
	scf_dn_index_t*   di2;
	int i;

	for (i  = 0; i < __bb->ds_malloced->size; i++) {
		ds2 =        __bb->ds_malloced->data[i];

		if (ds2->dag_node->var != __ds->dag_node->var)
			continue;

		if (!ds2->dn_indexes) {
			_bb_add_ds(bb, ds_obj);
			continue;
		}

		ds = scf_dn_status_clone(ds_obj);
		if (!ds)
			return -ENOMEM;

		if (!ds->dn_indexes) {
			ds ->dn_indexes = scf_vector_alloc();

			if (!ds->dn_indexes) {
				scf_dn_status_free(ds);
				return -ENOMEM;
			}
		}

		int ret = __ds_append_index(ds, ds2);
		if (ret < 0) {
			scf_dn_status_free(ds);
			return ret;
		}

		_bb_add_ds(bb, ds);

		scf_dn_status_free(ds);
		ds = NULL;
	}

	return 0;
}

static int _bb_add_ds_for_ret(scf_basic_block_t* bb, scf_dn_status_t* ds_obj, scf_function_t* f2)
{
	scf_list_t*        l2  = scf_list_tail(&f2->basic_block_list_head);
	scf_basic_block_t* bb2 = scf_list_data(l2, scf_basic_block_t, list);
	scf_dn_status_t*   ds2;

	int i;
	for (i  = 0; i < bb2->ds_malloced->size; i++) {
		ds2 =        bb2->ds_malloced->data[i];

		if (!ds2->ret_flag)
			continue;

		__bb_add_ds_append(bb, ds_obj, bb2, ds2);
	}

	return 0;
}

#define AUTO_GC_FIND_BB_SPLIT(parent, child) \
	do { \
		int ret = scf_basic_block_split(parent, &child); \
		if (ret < 0) \
			return ret; \
		\
		child->call_flag        = parent->call_flag; \
		child->dereference_flag = parent->dereference_flag; \
		\
		scf_vector_free(child->exit_dn_actives); \
		scf_vector_free(child->exit_dn_aliases); \
		scf_vector_free(child->dn_loads);   \
		scf_vector_free(child->dn_reloads); \
		\
		child->exit_dn_actives = scf_vector_clone(parent->exit_dn_actives); \
		child->exit_dn_aliases = scf_vector_clone(parent->exit_dn_aliases); \
		child->dn_loads        = scf_vector_clone(parent->dn_loads);   \
		child->dn_reloads      = scf_vector_clone(parent->dn_reloads); \
		\
		scf_list_add_front(&parent->list, &child->list); \
	} while (0)

static int _auto_gc_find_argv_out(scf_basic_block_t* cur_bb, scf_3ac_code_t* c)
{
	assert(c->srcs->size > 0);

	scf_3ac_operand_t* src = c->srcs->data[0];
	scf_function_t*    f2  = src->dag_node->var->func_ptr;
	scf_dag_node_t*    dn;
	scf_variable_t*    v0;
	scf_variable_t*    v1;
	scf_dn_status_t*   ds_obj;

	int count = 0;
	int ret;
	int i;

	for (i  = 1; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		dn  = src->dag_node;
		v0  = dn->var;

		while (dn) {
			if (SCF_OP_TYPE_CAST == dn->type)
				dn = dn->childs->data[0];

			else if (SCF_OP_EXPR == dn->type)
				dn = dn->childs->data[0];
			else
				break;
		}

		if (v0->nb_pointers + v0->nb_dimentions + (v0->type >= SCF_STRUCT) < 2)
			continue;

		if (i - 1 >= f2->argv->size)
			continue;

		v1 = f2->argv->data[i - 1];
		if (!v1->auto_gc_flag)
			continue;

		scf_logd("i: %d, f2: %s, v0: %s, v1: %s\n", i, f2->node.w->text->data, v0->w->text->data, v1->w->text->data);

		ds_obj = NULL;
		if (SCF_OP_ADDRESS_OF == dn->type)

			ret = scf_ds_for_dn(&ds_obj, dn->childs->data[0]);
		else
			ret = scf_ds_for_dn(&ds_obj, dn);
		if (ret < 0)
			return ret;

		if (ds_obj->dag_node->var->arg_flag)
			ds_obj->ret_flag = 1;

		ret = scf_vector_add_unique(cur_bb->entry_dn_actives, ds_obj->dag_node);
		if (ret < 0) {
			scf_dn_status_free(ds_obj);
			return ret;
		}

		ret = _bb_add_ds_for_call(cur_bb, ds_obj, f2, v1);

		scf_dn_status_free(ds_obj);
		ds_obj = NULL;
		if (ret < 0)
			return ret;

		count++;
	}

	return count;
}

static int _auto_gc_find_ret(scf_basic_block_t* cur_bb, scf_3ac_code_t* c)
{
	assert(c->srcs->size > 0);

	scf_3ac_operand_t* dst;
	scf_3ac_operand_t* src = c->srcs->data[0];
	scf_function_t*    f2  = src->dag_node->var->func_ptr;
	scf_variable_t*    fret;
	scf_variable_t*    v;
	scf_dn_status_t*   ds_obj;

	assert(c->dsts->size <= f2->rets->size);

	int i;
	for (i = 0; i < c->dsts->size; i++) {
		dst       = c->dsts->data[i];

		fret = f2->rets->data[i];
		v    = dst->dag_node->var;

		scf_logd("--- f2: %s(), i: %d, auto_gc_flag: %d\n", f2->node.w->text->data, i, fret->auto_gc_flag);

		if (fret->auto_gc_flag) {
			if (!scf_variable_may_malloced(v))
				return 0;

			ds_obj = scf_dn_status_alloc(dst->dag_node);
			if (!ds_obj)
				return -ENOMEM;

			_bb_add_ds(cur_bb, ds_obj);

			scf_dn_status_free(ds_obj);
			ds_obj = NULL;
		}
	}

	return 0;
}

int __auto_gc_ds_for_assign(scf_dn_status_t** ds, scf_dag_node_t** dn, scf_3ac_code_t* c)
{
	scf_3ac_operand_t* base;
	scf_3ac_operand_t* member;
	scf_3ac_operand_t* index;
	scf_3ac_operand_t* scale;
	scf_3ac_operand_t* src;
	scf_variable_t*    v;

	switch (c->op->type) {

		case SCF_OP_ASSIGN:
			base = c->dsts->data[0];
			v    = base->dag_node->var;

			if (!scf_variable_may_malloced(v))
				return 0;

			*ds = scf_dn_status_alloc(base->dag_node);
			if (!*ds)
				return -ENOMEM;

			src = c->srcs->data[0];
			*dn = src->dag_node;
			break;

		case SCF_OP_3AC_ASSIGN_ARRAY_INDEX:
			assert(4 == c->srcs->size);

			base  = c->srcs->data[0];
			v     = _scf_operand_get(base->node->parent);

			if (!v || !scf_variable_may_malloced(v))
				return 0;

			index = c->srcs->data[1];
			scale = c->srcs->data[2];
			src   = c->srcs->data[3];
			*dn   = src->dag_node;

			return scf_ds_for_assign_array_member(ds, base->dag_node, index->dag_node, scale->dag_node);
			break;

		case SCF_OP_3AC_ASSIGN_POINTER:
			assert(3 == c->srcs->size);

			member = c->srcs->data[1];
			v      = member->dag_node->var;

			if (!scf_variable_may_malloced(v))
				return 0;

			base = c->srcs->data[0];
			src  = c->srcs->data[2];
			*dn  = src->dag_node;

			return scf_ds_for_assign_member(ds, base->dag_node, member->dag_node);
			break;

		case SCF_OP_3AC_ASSIGN_DEREFERENCE:
			assert(2 == c->srcs->size);

			base = c->srcs->data[0];
			v    = _scf_operand_get(base->node->parent);

			if (!scf_variable_may_malloced(v))
				return 0;

			src = c->srcs->data[1];
			*dn = src->dag_node;

			return scf_ds_for_assign_dereference(ds, base->dag_node);
		default:
			break;
	};

	return 0;
}

static int _auto_gc_find_ref(scf_dn_status_t* ds_obj, scf_dag_node_t* dn, scf_3ac_code_t* c,
		scf_basic_block_t* bb,
		scf_basic_block_t* cur_bb,
		scf_function_t*    f)
{
	scf_dn_status_t*  ds;
	scf_dag_node_t*   pf;
	scf_function_t*   f2;
	scf_variable_t*   fret;
	scf_node_t*       parent;
	scf_node_t*       result;

	if (SCF_OP_CALL == dn->type || dn->node->split_flag) {
		int i = 0;

		if (dn->node->split_flag) {
			parent = dn->node->split_parent;

			assert(SCF_OP_CALL == parent->type || SCF_OP_NEW == parent->type);

			for (i = 0; i < parent->result_nodes->size; i++) {
				result    = parent->result_nodes->data[i];

				if (dn->node == result)
					break;
			}
		}

		pf   = dn->childs->data[0];
		f2   = pf->var->func_ptr;
		fret = f2->rets->data[i];

		scf_logd("f2: %s, rets[%d], auto_gc_flag: %d\n", f2->node.w->text->data, i, fret->auto_gc_flag);

		if (!strcmp(f2->node.w->text->data, "scf__auto_malloc")) {
			_bb_add_ds(cur_bb, ds_obj);
			return 1;
		}

		if (fret->auto_gc_flag) {
			ds = scf_dn_status_alloc(dn);
			if (!ds)
				return -ENOMEM;
			_bb_del_ds(cur_bb, ds);

			scf_dn_status_free(ds);
			ds = NULL;

			_bb_add_ds        (cur_bb, ds_obj);
			_bb_add_ds_for_ret(cur_bb, ds_obj, f2);
			return 2;
		}
	} else {
		ds  = NULL;
		int ret = scf_ds_for_dn(&ds, dn);
		if (ret < 0)
			return ret;

		ret = _bb_find_ds_alias(ds, c, bb, &f->basic_block_list_head);

		scf_dn_status_free(ds);
		ds = NULL;
		if (ret < 0)
			return ret;

		if (1 == ret) {
			_bb_add_ds(cur_bb, ds_obj);
			return 2;
		}
	}

	return 0;
}

static int _auto_gc_find_return(scf_vector_t* objs, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_basic_block_t* cur_bb, scf_function_t* f)
{
	scf_3ac_operand_t* src;
	scf_dn_status_t*   ds_obj;
	scf_dag_node_t*    dn;
	scf_variable_t*    v;

	int count = 0;
	int i;

	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		dn  = src->dag_node;
		v   = dn->var;

		if (!scf_variable_may_malloced(dn->var))
			continue;

		if (v->w)
			scf_logd("v: %s, line: %d\n", v->w->text->data, v->w->line);

		while (dn) {
			if (SCF_OP_TYPE_CAST == dn->type)
				dn = dn->childs->data[0];

			else if (SCF_OP_EXPR == dn->type)
				dn = dn->childs->data[0];
			else
				break;
		}

		ds_obj = scf_dn_status_alloc(dn);
		if (!ds_obj)
			return -ENOMEM;

		ds_obj->ret_flag  = 1;
		ds_obj->ret_index = i;

		scf_logd("i: %d, ds: %#lx, ret_index: %d\n", i, 0xffffff &(uintptr_t)ds_obj, ds_obj->ret_index);
//		scf_dn_status_print(ds_obj);

		int ret = _auto_gc_find_ref(ds_obj, dn, c, bb, cur_bb, f);
		if (ret < 0) {
			scf_dn_status_free(ds_obj);
			return ret;
		}

		count += ret > 0;

		if (ret > 1) {
			ret = scf_vector_add(objs, ds_obj);
			if (ret < 0) {
				scf_dn_status_free(ds_obj);
				return ret;
			}
		} else
			scf_dn_status_free(ds_obj);
		ds_obj = NULL;
	}

	return count;
}

static int _auto_gc_bb_find(scf_basic_block_t* bb, scf_function_t* f)
{
	scf_basic_block_t* cur_bb = bb;
	scf_basic_block_t* bb2    = NULL;
	scf_3ac_code_t*    c;
	scf_list_t*        l;

	int count = 0;
	int ret;
	int i;

	for (l = scf_list_head(&bb->code_list_head); l != scf_list_sentinel(&bb->code_list_head); ) {

		c  = scf_list_data(l, scf_3ac_code_t, list);
		l  = scf_list_next(l);

		scf_3ac_operand_t* src;
		scf_dn_status_t*   ds_obj = NULL;
		scf_dn_status_t*   ds     = NULL;
		scf_dag_node_t*    dn     = NULL;

		if (SCF_OP_ASSIGN == c->op->type
				|| SCF_OP_3AC_ASSIGN_ARRAY_INDEX == c->op->type
				|| SCF_OP_3AC_ASSIGN_POINTER     == c->op->type
				|| SCF_OP_3AC_ASSIGN_DEREFERENCE == c->op->type) {

			ret = __auto_gc_ds_for_assign(&ds_obj, &dn, c);
			if (ret < 0)
				return ret;

			if (!ds_obj)
				goto end;

			if (SCF_OP_ASSIGN != c->op->type) {
				if (ds_obj->dag_node->var->arg_flag)
					ds_obj->ret_flag = 1;
			}

		} else if (SCF_OP_RETURN == c->op->type) {

			scf_vector_t* objs = scf_vector_alloc();
			if (!objs)
				return -ENOMEM;

			ret = _auto_gc_find_return(objs, c, bb, cur_bb, f);
			if (ret < 0) {
				scf_vector_clear(objs, ( void (*)(void*) )scf_dn_status_free);
				scf_vector_free (objs);
				return ret;
			}
			count += ret;

			if (cur_bb != bb) {
				scf_list_del(&c->list);
				scf_list_add_tail(&cur_bb->code_list_head, &c->list);
			}

			if (objs->size > 0 && l != scf_list_sentinel(&bb->code_list_head)) {

				AUTO_GC_FIND_BB_SPLIT(cur_bb, bb2);
				cur_bb = bb2;

				for (i = 0; i < objs->size; i++) {
					ds_obj    = objs->data[i];

					ret = scf_vector_add_unique(cur_bb->entry_dn_actives, ds_obj->dag_node);
					if (ret < 0) {
						scf_vector_clear(objs, ( void (*)(void*) )scf_dn_status_free);
						scf_vector_free (objs);
						return ret;
					}
				}
			}

			scf_vector_clear(objs, ( void (*)(void*) )scf_dn_status_free);
			scf_vector_free (objs);
			continue;

		} else if (SCF_OP_CALL == c->op->type) {
			assert(c->srcs->size > 0);

			ret = _auto_gc_find_argv_out(cur_bb, c);
			if (ret < 0)
				return ret;
			count += ret;

			ret = _auto_gc_find_argv_in(cur_bb, c);
			if (ret < 0)
				return ret;

			ret = _auto_gc_find_ret(cur_bb, c);
			if (ret < 0)
				return ret;

			goto end;
		} else
			goto end;

		_bb_del_ds(cur_bb, ds_obj);
		count++;
ref:
		while (dn) {
			if (SCF_OP_TYPE_CAST == dn->type)
				dn = dn->childs->data[0];

			else if (SCF_OP_EXPR == dn->type)
				dn = dn->childs->data[0];
			else
				break;
		}

		ret = _auto_gc_find_ref(ds_obj, dn, c, bb, cur_bb, f);
		if (ret < 0) {
			scf_dn_status_free(ds_obj);
			return ret;
		}

		count += ret > 0;

		if (cur_bb != bb) {
			scf_list_del(&c->list);
			scf_list_add_tail(&cur_bb->code_list_head, &c->list);
		}

		if (ret > 1 && l != scf_list_sentinel(&bb->code_list_head)) {

			AUTO_GC_FIND_BB_SPLIT(cur_bb, bb2);
			cur_bb = bb2;

			ret = scf_vector_add_unique(cur_bb->entry_dn_actives, ds_obj->dag_node);
			if (ret < 0) {
				scf_dn_status_free(ds_obj);
				return ret;
			}
		}

		scf_dn_status_free(ds_obj);
		ds_obj = NULL;
		continue;
end:
		if (cur_bb != bb) {
			scf_list_del(&c->list);
			scf_list_add_tail(&cur_bb->code_list_head, &c->list);
		}
	}

	return count;
}

static int _bb_find_ds_alias_leak(scf_dn_status_t* ds_obj, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_dn_status_t* ds;
	scf_vector_t*    aliases;
	int i;

	aliases = scf_vector_alloc();
	if (!aliases)
		return -ENOMEM;

	int ret = scf_pointer_alias_ds_leak(aliases, ds_obj, c, bb, bb_list_head);
	if (ret < 0)
		goto error;

	for (i = 0; i < aliases->size; i++) {
		ds =        aliases->data[i];

		SCF_XCHG(ds->dn_indexes, ds->alias_indexes);
		SCF_XCHG(ds->dag_node,   ds->alias);

		if (!ds->dag_node)
			continue;

		ret = __bb_add_ds_append(bb, ds_obj, bb, ds);
		if (ret < 0) {
			scf_loge("\n");
			goto error;
		}
	}

	ret = 0;
error:
	scf_vector_clear(aliases, ( void (*)(void*) )scf_dn_status_free);
	scf_vector_free (aliases);
	return ret;
}

static int _auto_gc_function_find(scf_ast_t* ast, scf_function_t* f, scf_list_t* bb_list_head)
{
	if (!f || !bb_list_head)
		return -EINVAL;

	if (scf_list_empty(bb_list_head))
		return 0;

	scf_list_t*        l;
	scf_basic_block_t* bb;
	scf_dn_status_t*   ds;
	scf_3ac_code_t*    c;
	scf_variable_t*    fret;

	int total = 0;
	int count = 0;
	int ret;

	scf_logw("--- %s() ---\n", f->node.w->text->data);

	do {
		for (l = scf_list_head(bb_list_head); l != scf_list_sentinel(bb_list_head); ) {

			bb = scf_list_data(l, scf_basic_block_t, list);
			l  = scf_list_next(l);

			ret = _auto_gc_bb_find(bb, f);
			if (ret < 0) {
				scf_loge("\n");
				return ret;
			}

			total += ret;
		}

		l   = scf_list_head(bb_list_head);
		bb  = scf_list_data(l, scf_basic_block_t, list);

		ret = scf_basic_block_search_bfs(bb, _auto_gc_bb_next_find, NULL);
		if (ret < 0)
			return ret;

		total += ret;
		count  = ret;
	} while (count > 0);

	l   = scf_list_tail(bb_list_head);
	bb  = scf_list_data(l, scf_basic_block_t, list);

	l   = scf_list_tail(&bb->code_list_head);
	c   = scf_list_data(l, scf_3ac_code_t, list);

	int i;
	for (i = 0; i < bb->ds_malloced->size; i++) {
		ds =        bb->ds_malloced->data[i];

		if (!ds->ret_flag)
			continue;
#if 1
		scf_logi("ds: %#lx, ds->ret_flag: %u, ds->ret_index: %d, ds->dag_node->var->arg_flag: %u\n",
				0xffff & (uintptr_t)ds, ds->ret_flag, ds->ret_index, ds->dag_node->var->arg_flag);
		scf_dn_status_print(ds);
#endif
		if (ds->dag_node->var->arg_flag)
			ds->dag_node->var->auto_gc_flag = 1;
		else {
			assert(ds->ret_index < f->rets->size);

			fret = f->rets->data[ds->ret_index];
			fret->auto_gc_flag = 1;

			_bb_find_ds_alias_leak(ds, c, bb, bb_list_head);
		}
	}
	scf_logi("--- %s() ---\n\n", f->node.w->text->data);

	return total;
}

static int _optimize_auto_gc_find(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!ast || !functions)
		return -EINVAL;

	if (functions->size <= 0)
		return 0;

	scf_vector_t* fqueue = scf_vector_alloc();
	if (!fqueue)
		return -ENOMEM;

	int ret = _bfs_sort_function(fqueue, functions);
	if (ret < 0) {
		scf_vector_free(fqueue);
		return ret;
	}

	int total0 = 0;
	int total1 = 0;

	do {
		total0 = total1;
		total1 = 0;

		int i;
		for (i = 0; i < fqueue->size; i++) {
			f  =        fqueue->data[i];

			if (!f->node.define_flag)
				continue;

			if (!strcmp(f->node.w->text->data, "scf__auto_malloc"))
				continue;

			ret = _auto_gc_function_find(ast, f, &f->basic_block_list_head);
			if (ret < 0) {
				scf_vector_free(fqueue);
				return ret;
			}

			total1 += ret;
		}

		scf_logi("total0: %d, total1: %d\n", total0, total1);

	} while (total0 != total1);

	scf_vector_free(fqueue);
	return 0;
}

scf_optimizer_t  scf_optimizer_auto_gc_find =
{
	.name     =  "auto_gc_find",

	.optimize =  _optimize_auto_gc_find,

	.flags    = SCF_OPTIMIZER_GLOBAL,
};
