#include"scf_optimizer.h"
#include"scf_pointer_alias.h"

#include"scf_auto_gc_3ac.c"

int __auto_gc_ds_for_assign(scf_dn_status_t** ds, scf_dag_node_t** dn, scf_3ac_code_t* c);

static int _find_ds_malloced(scf_basic_block_t* bb, void* data)
{
	scf_dn_status_t* ds = data;

	if (!scf_vector_find_cmp(bb->ds_malloced, ds, scf_ds_cmp_same_indexes))
		return 0;

	if (scf_vector_find_cmp(bb->ds_freed, ds, scf_ds_cmp_same_indexes)) {
		scf_loge("error free dn: \n");
		return -1;
	}

	if (scf_vector_find(bb->dn_updateds, ds->dag_node))
		return 1;
	return 0;
}

static int __find_dn_active(scf_vector_t* dn_vec, scf_dag_node_t* dn)
{
	scf_dn_status_t* ds = NULL;
	scf_dag_node_t*  dn2;
	int i;

	for (i = 0; i < dn_vec->size; i++) {
		dn2       = dn_vec->data[i];

		if (dn2 == dn)
			return 1;

		int ret = scf_ds_for_dn(&ds, dn2);
		if (ret < 0)
			return ret;

		if (ds->dag_node == dn) {
			scf_dn_status_free(ds);
			return 1;
		}

		scf_dn_status_free(ds);
		ds = NULL;
	}

	return 0;
}

static int _find_dn_active(scf_basic_block_t* bb, void* data)
{
	scf_dag_node_t*  dn = data;

	int ret = __find_dn_active(bb->dn_loads, dn);
	if (0 == ret) {
		ret = __find_dn_active(bb->dn_reloads, dn);

		if (0 == ret)
			ret = __find_dn_active(bb->entry_dn_actives, dn);
	}

	scf_logd("bb: %p, dn: %s, 0\n", bb, dn->var->w->text->data);
	return ret;
}

static int _bb_find_ds_malloced(scf_basic_block_t* root, scf_list_t* bb_list_head, scf_dn_status_t* ds, scf_vector_t* results)
{
	scf_basic_block_visit_flag(bb_list_head, 0);

	return scf_basic_block_search_dfs_prev(root, _find_ds_malloced, ds, results);
}

static int _bb_find_dn_active(scf_basic_block_t* root, scf_list_t* bb_list_head, scf_dag_node_t* dn, scf_vector_t* results)
{
	scf_basic_block_visit_flag(bb_list_head, 0);

	return scf_basic_block_search_dfs_prev(root, _find_dn_active, dn, results);
}

static int _bb_prev_add_active(scf_basic_block_t* bb, void* data, scf_vector_t* queue)
{
	scf_basic_block_t* bb_prev;
	scf_dag_node_t*    dn = data;

	int count = 0;
	int ret;
	int j;

	for (j = 0; j < bb->prevs->size; j++) {
		bb_prev   = bb->prevs->data[j];

		if (!scf_vector_find(bb_prev->exit_dn_aliases, dn)) {

			ret = scf_vector_add_unique(bb_prev->exit_dn_actives, dn);
			if (ret < 0)
				return ret;
		}

		if (scf_vector_find(bb_prev->dn_updateds, dn)) {

			if (scf_vector_find(bb_prev->exit_dn_aliases, dn)
					|| scf_type_is_operator(dn->type))

				ret = scf_vector_add_unique(bb_prev->dn_resaves, dn);
			else
				ret = scf_vector_add_unique(bb_prev->dn_saves, dn);

			if (ret < 0)
				return ret;
		}

		++count;

		ret = scf_vector_add(queue, bb_prev);
		if (ret < 0)
			return ret;
	}
	return count;
}

static int _bb_add_active(scf_basic_block_t* bb, scf_dag_node_t* dn)
{
	int ret = scf_vector_add_unique(bb->entry_dn_actives, dn);
	if (ret < 0)
		return ret;

	if (scf_type_is_operator(dn->type))
		ret = scf_vector_add(bb->dn_reloads, dn);
//	else
//		ret = scf_vector_add(bb->dn_loads, dn);

	return ret;
}

static int _bb_add_free_arry(scf_ast_t* ast, scf_function_t* f, scf_basic_block_t* bb, scf_dag_node_t* dn_array)
{
	scf_basic_block_t* bb1 = NULL;

	int ret = scf_basic_block_split(bb, &bb1);
	if (ret < 0)
		return ret;

	scf_list_add_front(&bb->list, &bb1->list);

	if (bb->end_flag) {

		scf_basic_block_mov_code(bb1, scf_list_head(&bb->code_list_head), bb);

		bb1->ret_flag  = bb->ret_flag;
		bb1->end_flag  = 1;
		bb ->end_flag  = 0;
		bb ->call_flag = 1;

		bb1 = bb;
	} else {
		bb1->call_flag = 1;
	}

	ret = _bb_add_gc_code_free_array(&f->dag_list_head, ast, bb1, dn_array);
	if (ret < 0)
		return ret;

	ret = _bb_add_active(bb1, dn_array);
	if (ret < 0)
		return ret;

	return scf_basic_block_search_bfs(bb1, _bb_prev_add_active, dn_array);
}

static int _bb_add_memset_array(scf_ast_t* ast, scf_function_t* f, scf_dag_node_t* dn_array)
{
	scf_basic_block_t* bb  = NULL;
	scf_basic_block_t* bb1 = NULL;
	scf_list_t*        l;

	l  = scf_list_head(&f->basic_block_list_head);
	bb = scf_list_data(l, scf_basic_block_t, list);

	int ret = scf_basic_block_split(bb, &bb1);
	if (ret < 0)
		return ret;

	scf_list_add_front(&bb->list, &bb1->list);

	scf_basic_block_mov_code(bb1, scf_list_head(&bb->code_list_head), bb);

	bb1->call_flag = 1;
	bb1->ret_flag  = bb->ret_flag;
	bb1->end_flag  = bb->end_flag;

	bb1 = bb;

	ret = _bb_add_gc_code_memset_array(&f->dag_list_head, ast, bb1, dn_array);
	if (ret < 0)
		return ret;

	ret = _bb_add_active(bb1, dn_array);
	if (ret < 0)
		return ret;

	return scf_basic_block_search_bfs(bb1, _bb_prev_add_active, dn_array);
}

static int _bb_split_prev_add_free(scf_ast_t* ast, scf_function_t* f, scf_basic_block_t* bb, scf_dn_status_t* ds, scf_vector_t* bb_split_prevs)
{
	scf_basic_block_t* bb1;
	scf_basic_block_t* bb2;
	scf_basic_block_t* bb3;
	scf_dag_node_t*    dn           = ds->dag_node;
	scf_list_t*        bb_list_head = &f->basic_block_list_head;

	bb1 = scf_basic_block_alloc();
	if (!bb1) {
		scf_vector_free(bb_split_prevs);
		return -ENOMEM;
	}

	scf_vector_free(bb1->prevs);
	bb1->prevs     = bb_split_prevs;
	bb_split_prevs = NULL;

	bb1->ds_auto_gc = scf_dn_status_clone(ds);
	if (!bb1->ds_auto_gc) {
		scf_basic_block_free(bb1);
		return -ENOMEM;
	}

	int ret = scf_vector_add(bb1->nexts, bb);
	if (ret < 0) {
		scf_basic_block_free(bb1);
		return ret;
	}

	bb1->call_flag      = 1;
	bb1->auto_free_flag = 1;

	scf_list_add_tail(&bb->list, &bb1->list);

	scf_3ac_operand_t* dst;
	scf_3ac_code_t*    c;
	scf_list_t*        l;
	int j;
	int k;

	for (j  = 0; j < bb1->prevs->size; j++) {
		bb2 =        bb1->prevs->data[j];

		assert(0 == scf_vector_del(bb->prevs, bb2));

		for (k = 0; k < bb2->nexts->size; k++) {

			if (bb2->nexts->data[k] == bb)
				bb2->nexts->data[k] =  bb1;
		}
	}

	for (j  = 0; j < bb->prevs->size; j++) {
		bb2 =        bb->prevs->data[j];

		for (l  = scf_list_next(&bb2->list); l != scf_list_sentinel(bb_list_head);
			 l  = scf_list_next(l)) {

			bb3 = scf_list_data(l, scf_basic_block_t, list);

			if (bb3->jcc_flag)
				continue;

			if (bb3->jmp_flag)
				break;

			if (bb3 == bb1) {
				bb3 = scf_basic_block_jcc(bb, f, SCF_OP_GOTO);
				if (!bb3)
					return -ENOMEM;

				scf_list_add_tail(&bb1->list, &bb3->list);
			}
			break;
		}
	}
	scf_vector_add(bb->prevs, bb1);

	for (j  = 0; j < bb1->prevs->size; j++) {
		bb2 =        bb1->prevs->data[j];

		scf_dn_status_t* ds2;
		scf_list_t*      l2;

		for (l  = scf_list_next(&bb2->list); l != &bb1->list && l != scf_list_sentinel(bb_list_head);
			 l  = scf_list_next(l)) {

			bb3 = scf_list_data(l, scf_basic_block_t, list);

			if (!bb3->jmp_flag)
				break;

			l2  = scf_list_head(&bb3->code_list_head);
			c   = scf_list_data(l2, scf_3ac_code_t, list);
			dst = c->dsts->data[0];

			if (dst->bb == bb)
				dst->bb = bb1;

			assert(scf_list_next(l2) == scf_list_sentinel(&bb3->code_list_head));
		}

		for (k  = 0; k < bb2->ds_malloced->size; k++) {
			ds2 =        bb2->ds_malloced->data[k];

			if (0 == scf_ds_cmp_same_indexes(ds2, ds))
				continue;

			if (scf_vector_find_cmp(bb2->ds_freed, ds2, scf_ds_cmp_same_indexes))
				continue;

			if (scf_vector_find_cmp(bb1->ds_malloced, ds2, scf_ds_cmp_same_indexes))
				continue;

			scf_dn_status_t* ds3 = scf_dn_status_clone(ds2);
			if (!ds3)
				return -ENOMEM;

			ret = scf_vector_add(bb1->ds_malloced, ds3);
			if (ret < 0)
				return ret;
		}
	}

	ret = _bb_add_gc_code_freep(&f->dag_list_head, ast, bb1, ds);
	if (ret < 0)
		return ret;

	ret = _bb_add_active(bb1, dn);
	if (ret < 0)
		return ret;

	return scf_basic_block_search_bfs(bb1, _bb_prev_add_active, dn);
}

static int _bb_split_prevs(scf_basic_block_t* bb, scf_dn_status_t* ds, scf_vector_t* bb_split_prevs)
{
	scf_basic_block_t* bb_prev;
	int i;

	for (i = 0; i < bb->prevs->size; i++) {
		bb_prev   = bb->prevs->data[i];

		if (!scf_vector_find_cmp(bb_prev->ds_malloced, ds, scf_ds_cmp_like_indexes))
			continue;

		if (scf_vector_find_cmp(bb_prev->ds_freed, ds, scf_ds_cmp_same_indexes))
			continue;

		int ret = scf_vector_add(bb_split_prevs, bb_prev);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int _bb_add_last_free(scf_dn_status_t* ds, scf_basic_block_t* bb, scf_ast_t* ast, scf_function_t* f)
{
	scf_vector_t* vec = scf_vector_alloc();
	if (!vec)
		return -ENOMEM;

	scf_basic_block_t* bb2;
	int j;
	int dfo = 0;

	int ret = _bb_find_ds_malloced(bb, &f->basic_block_list_head, ds, vec);
	if (ret < 0) {
		scf_vector_free(vec);
		return ret;
	}

#define AUTO_GC_FIND_MAX_DFO() \
	do { \
		for (j  = 0; j < vec->size; j++) { \
			bb2 =        vec->data[j]; \
			\
			if (bb2->dfo > dfo) \
			dfo = bb2->dfo; \
		} \
	} while (0)
	AUTO_GC_FIND_MAX_DFO();
	vec->size = 0;

	ret = _bb_find_dn_active(bb, &f->basic_block_list_head, ds->dag_node, vec);
	if (ret < 0) {
		scf_vector_free(vec);
		return ret;
	}
	AUTO_GC_FIND_MAX_DFO();
	vec->size = 0;

	for (j = 0; j < bb->dominators->size; j++) {
		bb2       = bb->dominators->data[j];

		if (bb2->dfo > dfo)
			break;
	}

	ret = _bb_split_prevs(bb2, ds, vec);
	if (ret < 0) {
		scf_vector_free(vec);
		return ret;
	}

	return _bb_split_prev_add_free(ast, f, bb2, ds, vec);
}

static int _auto_gc_last_free(scf_ast_t* ast, scf_function_t* f)
{
	scf_list_t*        l = scf_list_tail(&f->basic_block_list_head);
	scf_basic_block_t* bb;
	scf_dn_status_t*   ds;
	scf_dag_node_t*    dn;
	scf_variable_t*    v;
	scf_vector_t*      local_arrays;

	bb = scf_list_data(l, scf_basic_block_t, list);

	scf_logd("bb: %p, bb->ds_malloced->size: %d\n", bb, bb->ds_malloced->size);

	local_arrays = scf_vector_alloc();
	if (!local_arrays)
		return -ENOMEM;

	int ret;
	int i;

	for (i = 0; i < bb->ds_malloced->size; i++) {
		ds =        bb->ds_malloced->data[i];

		dn = ds->dag_node;
		v  = dn->var;

		scf_logw("%s(), last free: v_%d_%d/%s, ds->ret_flag: %u\n",
				f->node.w->text->data, v->w->line, v->w->pos, v->w->text->data, ds->ret_flag);
		scf_dn_status_print(ds);

		if (ds->ret_flag)
			continue;

		if (ds->dn_indexes) {
			int j;
			scf_dn_index_t* di;

			for (j = ds->dn_indexes->size - 1; j >= 0; j--) {
				di = ds->dn_indexes->data[j];

				if (di->member) {
					assert(di->member->member_flag && v->type >= SCF_STRUCT);
					break;
				}

				if (-1 == di->index) {
					if (v->nb_dimentions > 0 && v->local_flag) {

						ret = scf_vector_add_unique(local_arrays, dn);
						if (ret < 0)
							goto error;
					}
					break;
				}
			}

			if (j >= 0)
				continue;
		}

		ret = _bb_add_last_free(ds, bb, ast, f);
		if (ret < 0)
			goto error;
	}

	for (i = 0; i < local_arrays->size; i++) {
		dn =        local_arrays->data[i];

		ret = _bb_add_memset_array(ast, f, dn);
		if (ret < 0)
			goto error;

		ret = _bb_add_free_arry(ast, f, bb, dn);
		if (ret < 0)
			goto error;
	}

	ret = 0;
error:
	scf_vector_free(local_arrays);
	return ret;
}

#define AUTO_GC_BB_SPLIT(parent, child) \
	do { \
		int ret = scf_basic_block_split(parent, &child); \
		if (ret < 0) \
			return ret; \
		\
		child->call_flag        = parent->call_flag; \
		child->dereference_flag = parent->dereference_flag; \
		\
		SCF_XCHG(parent->ds_malloced, child->ds_malloced); \
		\
		scf_vector_free(child->exit_dn_actives); \
		scf_vector_free(child->exit_dn_aliases); \
		\
		child->exit_dn_actives = scf_vector_clone(parent->exit_dn_actives); \
		child->exit_dn_aliases = scf_vector_clone(parent->exit_dn_aliases); \
		\
		scf_list_add_front(&parent->list, &child->list); \
	} while (0)

static int _auto_gc_bb_split(scf_basic_block_t* parent, scf_basic_block_t** pchild)
{
	scf_basic_block_t* child = NULL;

	AUTO_GC_BB_SPLIT(parent, child);

	*pchild = child;
	return 0;
}

static int _auto_gc_bb_ref(scf_dn_status_t* ds_obj, scf_vector_t* ds_malloced, scf_ast_t* ast, scf_function_t* f, scf_basic_block_t** pbb)
{
	scf_basic_block_t* cur_bb = *pbb;
	scf_basic_block_t* bb1    = NULL;
	scf_dag_node_t*    dn     = ds_obj->dag_node;

	AUTO_GC_BB_SPLIT(cur_bb, bb1);

	bb1->ds_auto_gc = scf_dn_status_clone(ds_obj);
	if (!bb1->ds_auto_gc)
		return -ENOMEM;

	int ret = _bb_add_gc_code_ref(&f->dag_list_head, ast, bb1, ds_obj);
	if (ret < 0)
		return ret;

	ret = _bb_add_active(bb1, dn);
	if (ret < 0)
		return ret;

	ret = scf_basic_block_search_bfs(bb1, _bb_prev_add_active, dn);
	if (ret < 0)
		return ret;

	if (!scf_vector_find_cmp(ds_malloced, ds_obj, scf_ds_cmp_like_indexes)) {
		ret = scf_vector_add(ds_malloced, ds_obj);
		if (ret < 0)
			return ret;

		scf_dn_status_ref(ds_obj);
	}

	bb1->call_flag  = 1;
	bb1->dereference_flag = 0;
	bb1->auto_ref_flag    = 1;

	*pbb = bb1;
	return 0;
}

static int _bb_split_add_free(scf_ast_t* ast, scf_function_t* f, scf_basic_block_t** pbb, scf_dn_status_t* ds)
{
	scf_basic_block_t* cur_bb = *pbb;
	scf_basic_block_t* bb1    = NULL;
	scf_basic_block_t* bb2    = NULL;
	scf_dag_node_t*    dn           = ds->dag_node;
	scf_list_t*        bb_list_head = &f->basic_block_list_head;

	AUTO_GC_BB_SPLIT(cur_bb, bb1);
	AUTO_GC_BB_SPLIT(bb1,    bb2);

	bb1->ds_auto_gc = scf_dn_status_clone(ds);
	if (!bb1->ds_auto_gc)
		return -ENOMEM;

	bb1->call_flag  = 1;
	bb1->dereference_flag = 0;
	bb1->auto_free_flag   = 1;

	int ret = _bb_add_gc_code_freep(&f->dag_list_head, ast, bb1, ds);
	if (ret < 0)
		return ret;

	ret = _bb_add_active(bb1, dn);
	if (ret < 0)
		return ret;

	ret = scf_basic_block_search_bfs(bb1, _bb_prev_add_active, dn);
	if (ret < 0)
		return ret;

	*pbb = bb2;
	return 0;
}

static int _bb_prevs_malloced(scf_basic_block_t* bb, scf_vector_t* ds_malloced)
{
	scf_basic_block_t*  bb_prev;
	scf_dn_status_t*    ds;
	scf_dn_status_t*    ds2;
	int i;
	int j;

	for (i = 0; i < bb->prevs->size; i++) {
		bb_prev   = bb->prevs->data[i];

		for (j = 0; j < bb_prev->ds_malloced->size; j++) {
			ds =        bb_prev->ds_malloced->data[j];

			if (scf_vector_find_cmp(bb_prev->ds_freed, ds, scf_ds_cmp_same_indexes))
				continue;

			if (scf_vector_find_cmp(ds_malloced, ds, scf_ds_cmp_like_indexes))
				continue;

			ds2 = scf_dn_status_clone(ds);
			if (!ds2)
				return -ENOMEM;

			int ret = scf_vector_add(ds_malloced, ds2);
			if (ret < 0) {
				scf_dn_status_free(ds2);
				return ret;
			}
		}
	}
	return 0;
}

int __bb_find_ds_alias(scf_vector_t* aliases, scf_dn_status_t* ds_obj, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head);

static int _bb_need_ref(scf_dag_node_t* dn, scf_vector_t* ds_malloced, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_dn_status_t* ds = NULL;
	scf_vector_t*    aliases;
	int i;

	int ret = scf_ds_for_dn(&ds, dn);
	if (ret < 0)
		return ret;

	if (scf_vector_find_cmp(ds_malloced, ds, scf_ds_cmp_like_indexes)) {
		scf_dn_status_free(ds);
		return 1;
	}

	aliases = scf_vector_alloc();
	if (!aliases) {
		scf_dn_status_free(ds);
		return -ENOMEM;
	}

	ret = __bb_find_ds_alias(aliases, ds, c, bb, bb_list_head);

	scf_dn_status_free(ds);
	ds = NULL;
	if (ret < 0)
		goto error;

	ret = 0;
	for (i = 0; i < aliases->size; i++) {
		ds =        aliases->data[i];

		if (!ds->dag_node)
			continue;

		if (scf_vector_find_cmp(ds_malloced, ds, scf_ds_cmp_like_indexes)) {
			ret = 1;
			break;
		}
	}

error:
	scf_vector_clear(aliases, ( void (*)(void*) )scf_dn_status_free);
	scf_vector_free (aliases);
	return ret;
}

static int _bb_split_prevs_need_free(scf_dn_status_t* ds_obj, scf_vector_t* ds_malloced, scf_vector_t* bb_split_prevs, scf_basic_block_t* bb)
{
	if (scf_vector_find_cmp(ds_malloced, ds_obj, scf_ds_cmp_like_indexes)) {

		int ret = _bb_split_prevs(bb, ds_obj, bb_split_prevs);
		if (ret < 0)
			return ret;

		return 1;
	}

	return 0;
}

static int _auto_gc_bb_free(scf_dn_status_t* ds_obj, scf_vector_t* ds_malloced, scf_vector_t* ds_assigned, scf_ast_t* ast,
		scf_function_t*     f,
		scf_basic_block_t*  bb,
		scf_basic_block_t** cur_bb)
{
	scf_vector_t* bb_split_prevs = scf_vector_alloc();

	if (!bb_split_prevs)
		return -ENOMEM;

	int ret = _bb_split_prevs_need_free(ds_obj, ds_malloced, bb_split_prevs, bb);
	if (ret < 0) {
		scf_vector_free(bb_split_prevs);
		return ret;
	}

	if (ret) {
		if (!scf_vector_find_cmp(ds_assigned, ds_obj, scf_ds_cmp_like_indexes)
				&& bb_split_prevs->size > 0
				&& bb_split_prevs->size < bb->prevs->size) {

			return _bb_split_prev_add_free(ast, f, bb, ds_obj, bb_split_prevs);
		}

		ret = _bb_split_add_free(ast, f, cur_bb, ds_obj);
	}

	scf_vector_free(bb_split_prevs);
	bb_split_prevs = NULL;
	return ret;
}

static int _auto_gc_retval(scf_dn_status_t* ds_obj, scf_dag_node_t* dn, scf_vector_t* ds_malloced, scf_3ac_code_t* c, scf_function_t* f)
{
	scf_dag_node_t*  pf;
	scf_function_t*  f2;
	scf_variable_t*  fret;
	scf_node_t*      parent;
	scf_node_t*      result;

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

	if (!strcmp(f2->node.w->text->data, "scf__auto_malloc") || fret->auto_gc_flag) {

		assert(SCF_OP_RETURN != c->op->type);
//		fret = f->rets->data[i];
//		fret->auto_gc_flag = 1;

		if (!scf_vector_find_cmp(ds_malloced, ds_obj, scf_ds_cmp_like_indexes)) {

			int ret = scf_vector_add(ds_malloced, ds_obj);
			if (ret < 0)
				return ret;

			scf_dn_status_ref(ds_obj);
		}
	}

	return 0;
}

static int _optimize_auto_gc_bb(scf_ast_t* ast, scf_function_t* f, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_basic_block_t*  cur_bb = bb;
	scf_basic_block_t*  bb2;
	scf_3ac_code_t*     c;
	scf_vector_t*       ds_malloced;
	scf_vector_t*       ds_assigned;
	scf_list_t*         l;

	ds_malloced = scf_vector_alloc();
	if (!ds_malloced)
		return -ENOMEM;

	ds_assigned = scf_vector_alloc();
	if (!ds_assigned) {
		scf_vector_free(ds_malloced);
		return -ENOMEM;
	}

	// at first, the malloced vars, are ones malloced in previous blocks

	int ret = _bb_prevs_malloced(bb, ds_malloced);
	if (ret < 0)
		goto error;

	for (l = scf_list_head(&bb->code_list_head); l != scf_list_sentinel(&bb->code_list_head); ) {

		c  = scf_list_data(l, scf_3ac_code_t, list);
		l  = scf_list_next(l);

		scf_dn_status_t*  ds_obj = NULL;
		scf_dn_status_t*  ds     = NULL;
		scf_dag_node_t*   dn     = NULL;

		if (SCF_OP_ASSIGN == c->op->type
				|| SCF_OP_3AC_ASSIGN_ARRAY_INDEX == c->op->type
				|| SCF_OP_3AC_ASSIGN_POINTER == c->op->type
				|| SCF_OP_3AC_ASSIGN_DEREFERENCE == c->op->type) {

			ret = __auto_gc_ds_for_assign(&ds_obj, &dn, c);
			if (ret < 0)
				goto error;

			if (!ds_obj)
				goto end;

			if (SCF_OP_ASSIGN != c->op->type) {
				if (ds_obj->dag_node->var->arg_flag)
					ds_obj->ret_flag = 1;
			}
		} else
			goto end;

		ret = _auto_gc_bb_free(ds_obj, ds_malloced, ds_assigned, ast, f, bb, &cur_bb);
		if (ret < 0) {
			scf_dn_status_free(ds_obj);
			goto error;
		}

		ret = scf_vector_add(ds_assigned, ds_obj);
		if (ret < 0) {
			scf_dn_status_free(ds_obj);
			goto error;
		}
ref:
		if (!dn->var->local_flag && cur_bb != bb)
			dn ->var->tmp_flag = 1;

		while (dn) {
			if (SCF_OP_TYPE_CAST == dn->type)
				dn = dn->childs->data[0];

			else if (SCF_OP_EXPR == dn->type)
				dn = dn->childs->data[0];
			else
				break;
		}

		if (SCF_OP_CALL == dn->type || dn->node->split_flag) {

			ret = _auto_gc_retval(ds_obj, dn, ds_malloced, c, f);
			if (ret < 0)
				goto error;

		} else {
			ret = _bb_need_ref(dn, ds_malloced, c, bb, bb_list_head);
			if (ret < 0)
				goto error;

			if (ret > 0) {
				assert(SCF_OP_RETURN != c->op->type);
//				scf_variable_t* fret = f->rets->data[0];
//				fret->auto_gc_flag = 1;

				if (cur_bb != bb) {
					scf_list_del(&c->list);
					scf_list_add_tail(&cur_bb->code_list_head, &c->list);
				}

				ret = _auto_gc_bb_ref(ds_obj, ds_malloced, ast, f, &cur_bb);
				if (ret < 0)
					goto error;

				if (l != scf_list_sentinel(&bb->code_list_head)) {
					bb2 = NULL;
					ret = _auto_gc_bb_split(cur_bb, &bb2);
					if (ret < 0)
						goto error;

					cur_bb = bb2;
				}
				continue;
			}
		}
end:
		if (cur_bb != bb) {
			scf_list_del(&c->list);
			scf_list_add_tail(&cur_bb->code_list_head, &c->list);
		}
	}

	ret = 0;
error:
	scf_vector_clear(ds_assigned, ( void (*)(void*) )scf_dn_status_free);
	scf_vector_clear(ds_malloced, ( void (*)(void*) )scf_dn_status_free);
	scf_vector_free (ds_assigned);
	scf_vector_free (ds_malloced);
	return ret;
}

static int _auto_gc_prev_dn_actives(scf_list_t* current, scf_list_t* sentinel)
{
	scf_basic_block_t* bb;
	scf_dag_node_t*    dn;
	scf_vector_t*      dn_actives;
	scf_list_t*        l;

	int ret;
	int i;

	dn_actives = scf_vector_alloc();
	if (!dn_actives)
		return -ENOMEM;

	for (l = current; l != sentinel; l = scf_list_prev(l)) {
		bb = scf_list_data(l, scf_basic_block_t, list);

		ret = scf_basic_block_active_vars(bb);
		if (ret < 0)
			goto error;

		for (i = 0; i < dn_actives->size; i++) {
			dn =        dn_actives->data[i];

			ret = scf_vector_add_unique(bb->exit_dn_actives, dn);
			if (ret < 0)
				goto error;
		}

		for (i = 0; i < bb->entry_dn_actives->size; i++) {
			dn =        bb->entry_dn_actives->data[i];

			ret = scf_vector_add_unique(dn_actives, dn);
			if (ret < 0)
				goto error;
		}
	}

	ret = 0;
error:
	scf_vector_free(dn_actives);
	return ret;
}

static void _auto_gc_delete(scf_basic_block_t* bb, scf_basic_block_t* bb2)
{
	scf_basic_block_t* bb3;
	scf_basic_block_t* bb4;

	assert(1 == bb2->prevs->size);
	assert(1 == bb ->nexts->size);

	bb3 = bb2->prevs->data[0];
	bb4 = bb ->nexts->data[0];

	assert(1 == bb3->nexts->size);
	assert(1 == bb4->prevs->size);

	bb3->nexts->data[0] = bb4;
	bb4->prevs->data[0] = bb3;

	scf_list_del(&bb->list);
	scf_list_del(&bb2->list);

	scf_basic_block_free(bb);
	scf_basic_block_free(bb2);
}

static int _optimize_auto_gc(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!ast || !f)
		return -EINVAL;

	if (!strcmp(f->node.w->text->data, "scf__auto_malloc"))
		return 0;

	scf_list_t*        bb_list_head = &f->basic_block_list_head;
	scf_list_t*        l;
	scf_basic_block_t* bb;
	scf_basic_block_t* bb2;

	int ret = _auto_gc_last_free(ast, f);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	for (l = scf_list_head(bb_list_head); l != scf_list_sentinel(bb_list_head); ) {

		scf_list_t*  start = l;
		scf_list_t*  l2;

		bb = scf_list_data(l, scf_basic_block_t, list);

		for (l  = scf_list_next(l); l != scf_list_sentinel(bb_list_head); l = scf_list_next(l)) {
			bb2 = scf_list_data(l, scf_basic_block_t, list);

			if (!bb2->auto_ref_flag && !bb2->auto_free_flag)
				break;
		}

		ret = _optimize_auto_gc_bb(ast, f, bb, bb_list_head);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

		ret = _auto_gc_prev_dn_actives(scf_list_prev(l), scf_list_prev(start));
		if (ret < 0)
			return ret;

		for (l2 = scf_list_prev(l); l2 != scf_list_prev(start); ) {
			bb  = scf_list_data(l2, scf_basic_block_t, list);
			l2  = scf_list_prev(l2);

			if (l2 != start && scf_list_prev(l) != &bb->list) {
				bb2 = scf_list_data(l2, scf_basic_block_t, list);

				if (bb->auto_free_flag
						&& bb2->auto_ref_flag
						&& 0 == scf_ds_cmp_same_indexes(bb->ds_auto_gc, bb2->ds_auto_gc)) {

					l2 = scf_list_prev(l2);

					_auto_gc_delete(bb, bb2);
					continue;
				}
			}

			ret = scf_basic_block_loads_saves(bb, bb_list_head);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

scf_optimizer_t  scf_optimizer_auto_gc =
{
	.name     =  "auto_gc",

	.optimize =  _optimize_auto_gc,

	.flags    = SCF_OPTIMIZER_LOCAL,
};
