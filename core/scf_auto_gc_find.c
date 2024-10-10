#include"scf_optimizer.h"
#include"scf_pointer_alias.h"

static int _bb_find_ds(scf_basic_block_t* bb, scf_dn_status_t* ds_obj)
{
	if (scf_vector_find_cmp(bb->ds_freed, ds_obj, scf_ds_cmp_same_indexes))
		return 0;

	if (scf_vector_find_cmp(bb->ds_malloced, ds_obj, scf_ds_cmp_same_indexes))
		return 1;
	return 0;
}

static int __ds_append_index_n(scf_dn_status_t* dst, scf_dn_status_t* src, int n)
{
	scf_dn_index_t* di;
	int j;

	assert(n <= src->dn_indexes->size);

	for (j = 0; j < n; j++) {
		di = src->dn_indexes->data[j];

		int ret = scf_vector_add_front(dst->dn_indexes, di);
		if (ret < 0)
			return ret;
		di->refs++;
	}

	return 0;
}

int __bb_find_ds_alias(scf_vector_t* aliases, scf_dn_status_t* ds_obj, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_vector_t*    tmp;
	scf_dn_status_t* ds2;
	scf_dn_status_t* ds;
	scf_dn_index_t*  di;
	scf_3ac_code_t*  c2 = scf_list_data(scf_list_prev(&c->list), scf_3ac_code_t, list);

	int ndi = 0;
	int ret = 0;
	int j;

	tmp = scf_vector_alloc();
	if (!tmp)
		return -ENOMEM;

	ds = scf_dn_status_clone(ds_obj);
	if (!ds) {
		scf_vector_free(tmp);
		return -ENOMEM;
	}

	while (1) {
		ret = scf_pointer_alias_ds(tmp, ds, c2, bb, bb_list_head);
		if (ret < 0) {
			if (SCF_POINTER_NOT_INIT == ret)
				break;
			goto error;
		}

		for (j = 0; j < tmp->size; j++) {
			ds2       = tmp->data[j];

			SCF_XCHG(ds2->dn_indexes, ds2->alias_indexes);
			SCF_XCHG(ds2->dag_node,   ds2->alias);

			if (ds_obj->dn_indexes) {

				if (!ds2->dn_indexes) {
					ds2 ->dn_indexes = scf_vector_alloc();
					if (!ds2->dn_indexes) {
						ret = -ENOMEM;
						goto error;
					}
				}

				ret = __ds_append_index_n(ds2, ds_obj, ndi);
				if (ret < 0)
					goto error;
			}

			ret = scf_vector_add(aliases, ds2);
			if (ret < 0)
				goto error;

			scf_dn_status_ref(ds2);
			ds2 = NULL;
		}

		scf_vector_clear(tmp, ( void (*)(void*) )scf_dn_status_free);

		if (ds->dn_indexes) {
			assert(ds->dn_indexes->size > 0);
			di =   ds->dn_indexes->data[0];

			assert(0 == scf_vector_del(ds->dn_indexes, di));

			scf_dn_index_free(di);
			di = NULL;

			ndi++;

			if (0 == ds->dn_indexes->size) {
				scf_vector_free(ds->dn_indexes);
				ds->dn_indexes = NULL;
			}
		} else
			break;
	}

	ret = 0;
error:
	scf_dn_status_free(ds);
	scf_vector_clear(tmp, ( void (*)(void*) )scf_dn_status_free);
	scf_vector_free(tmp);
	return ret;
}

static int _bb_find_ds_alias(scf_dn_status_t* ds_obj, scf_3ac_code_t* c, scf_basic_block_t* bb, scf_list_t* bb_list_head)
{
	scf_dn_status_t* ds_obj2;
	scf_dn_status_t* ds;
	scf_dn_index_t*  di;
	scf_vector_t*    aliases;
	int i;

	aliases = scf_vector_alloc();
	if (!aliases)
		return -ENOMEM;

	int ret = __bb_find_ds_alias(aliases, ds_obj, c, bb, bb_list_head);
	if (ret < 0)
		return ret;

	int need = 0;
	for (i = 0; i < aliases->size; i++) {
		ds =        aliases->data[i];

		scf_logd("ds: %#lx, ds->refs: %d\n", 0xffff & (uintptr_t)ds, ds->refs);
		if (!ds->dag_node)
			continue;

		if (scf_vector_find_cmp(bb->ds_malloced, ds, scf_ds_cmp_same_indexes)
		&& !scf_vector_find_cmp(bb->ds_freed,    ds, scf_ds_cmp_same_indexes)) {
			need = 1;
			break;
		}
	}

	if (scf_vector_find_cmp(bb->ds_malloced, ds_obj, scf_ds_cmp_same_indexes)
	&& !scf_vector_find_cmp(bb->ds_freed,    ds_obj, scf_ds_cmp_same_indexes)) {
		need = 1;
	}

	ret = need;
error:
	scf_vector_clear(aliases, ( void (*)(void*) ) scf_dn_status_free);
	scf_vector_free (aliases);
	return ret;
}

static int _auto_gc_find_argv_in(scf_basic_block_t* cur_bb, scf_3ac_code_t* c)
{
	scf_3ac_operand_t* src;
	scf_dag_node_t*    dn;
	scf_variable_t*    v;

	int i;
	for (i  = 1; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		dn  = src->dag_node;

		while (dn) {
			if (SCF_OP_TYPE_CAST == dn->type)
				dn = dn->childs->data[0];

			else if (SCF_OP_EXPR == dn->type)
				dn = dn->childs->data[0];

			else if (SCF_OP_POINTER == dn->type)
				dn = dn->childs->data[0];
			else
				break;
		}

		v = dn->var;

		if (v->nb_pointers + v->nb_dimentions + (v->type >= SCF_STRUCT) < 2)
			continue;

		scf_logd("v: %s\n", v->w->text->data);

		if (scf_vector_add_unique(cur_bb->dn_reloads, dn) < 0)
			return -ENOMEM;
	}

	return 0;
}

static int _auto_gc_bb_next_find(scf_basic_block_t* bb, void* data, scf_vector_t* queue)
{
	scf_basic_block_t* next_bb;
	scf_dn_status_t*   ds;
	scf_dn_status_t*   ds2;

	int count = 0;
	int ret;
	int j;

	for (j = 0; j < bb->nexts->size; j++) {
		next_bb   = bb->nexts->data[j];

		int k;
		for (k = 0; k < bb->ds_malloced->size; k++) {
			ds =        bb->ds_malloced->data[k];

			if (scf_vector_find_cmp(bb->ds_freed, ds, scf_ds_cmp_same_indexes))
				continue;

			if (scf_vector_find_cmp(next_bb->ds_freed, ds, scf_ds_cmp_same_indexes))
				continue;

			ds2 = scf_vector_find_cmp(next_bb->ds_malloced, ds, scf_ds_cmp_like_indexes);
			if (ds2) {
				uint32_t tmp = ds2->ret;

				ds2->ret |= ds->ret;

				if (tmp != ds2->ret)
					count++;
				continue;
			}

			ds2 = scf_dn_status_clone(ds);
			if (!ds2)
				return -ENOMEM;

			ret = scf_vector_add(next_bb->ds_malloced, ds2);
			if (ret < 0) {
				scf_dn_status_free(ds2);
				return ret;
			}
			++count;
		}

		ret = scf_vector_add(queue, next_bb);
		if (ret < 0)
			return ret;
	}
	return count;
}

static int _bfs_sort_function(scf_vector_t* fqueue, scf_vector_t* functions)
{
	scf_function_t* fmalloc = NULL;
	scf_function_t* f;
	scf_function_t* f2;
	int i;
	int j;

	for (i = 0; i < functions->size; i++) {
		f  =        functions->data[i];

		f->visited_flag = 0;

		if (!fmalloc && !strcmp(f->node.w->text->data, "scf__auto_malloc"))
			fmalloc = f;
	}

	if (!fmalloc)
		return 0;

	int ret = scf_vector_add(fqueue, fmalloc);
	if (ret < 0)
		return ret;

	for (i = 0; i < fqueue->size; i++) {
		f  =        fqueue->data[i];

		if (f->visited_flag)
			continue;

		scf_logd("f: %p, %s\n", f, f->node.w->text->data);

		f->visited_flag = 1;

		for (j = 0; j < f->caller_functions->size; j++) {
			f2 =        f->caller_functions->data[j];

			if (f2->visited_flag)
				continue;

			ret = scf_vector_add(fqueue, f2);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}
