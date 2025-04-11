#include"scf_optimizer.h"

static int __bb_dfs_tree(scf_basic_block_t* root, scf_vector_t* edges, int* total)
{
	scf_basic_block_t* bb;
	scf_bb_edge_t*     edge;

	int ret;
	int i;

	assert(!root->jmp_flag);

	root->visit_flag = 1;

	for (i = 0; i < root->prevs->size; ++i) {
		bb =        root->prevs->data[i];

		if (bb->visit_flag)
			continue;

		edge = malloc(sizeof(scf_bb_edge_t));
		if (!edge)
			return -ENOMEM;

		edge->start = root;
		edge->end   = bb;

		ret = scf_vector_add(edges, edge);
		if ( ret < 0)
			return ret;

		ret = __bb_dfs_tree(bb, edges, total);
		if ( ret < 0)
			return ret;
	}

	root->dfo = --*total;
	return 0;
}

static int _bb_dfs_tree(scf_list_t* bb_list_head, scf_vector_t* tree)
{
	if (!bb_list_head)
		return -EINVAL;

	if (scf_list_empty(bb_list_head))
		return 0;

	scf_basic_block_t* bb;
	scf_list_t*        l;

	int total = 0;

	for (l = scf_list_head(bb_list_head); l != scf_list_sentinel(bb_list_head); l = scf_list_next(l)) {
		bb = scf_list_data(l, scf_basic_block_t, list);

		bb->visit_flag = 0;

		if (!bb->jmp_flag)
			++total;
	}
	assert(&bb->list == scf_list_tail(bb_list_head));

	return __bb_dfs_tree(bb, tree, &total);
}

static int _bb_cmp_dfo(const void* p0, const void* p1)
{
	scf_basic_block_t* bb0 = *(scf_basic_block_t**)p0;
	scf_basic_block_t* bb1 = *(scf_basic_block_t**)p1;

	if (bb0->dfo < bb1->dfo)
		return -1;
	if (bb0->dfo > bb1->dfo)
		return 1;
	return 0;
}

static int _bb_intersection(scf_vector_t* dst, scf_vector_t* src)
{
	int k0 = 0;
	int k1 = 0;

	while (k0 < dst->size && k1 < src->size) {

		scf_basic_block_t* bb0 = dst->data[k0];
		scf_basic_block_t* bb1 = src->data[k1];

		if (bb0->dfo < bb1->dfo) {

			int ret = scf_vector_del(dst, bb0);
			if (ret < 0)
				return ret;
			continue;
		}

		if (bb0->dfo > bb1->dfo) {
			++k1;
			continue;
		}

		++k0;
		++k1;
	}

	dst->size = k0;
	return 0;
}

static int __find_reverse_dominators(scf_list_t* bb_list_head)
{
	if (!bb_list_head)
		return -EINVAL;

	if (scf_list_empty(bb_list_head))
		return 0;

	scf_list_t*        l;
	scf_basic_block_t* bb;
	scf_vector_t*      all;

	int i;
	int j;
	int ret;
	int changed;

	all = scf_vector_alloc();
	if (!all)
		return -ENOMEM;

	for (l = scf_list_tail(bb_list_head); l != scf_list_sentinel(bb_list_head); l = scf_list_prev(l)) {

		bb = scf_list_data(l, scf_basic_block_t, list);
		if (bb->jmp_flag)
			continue;

		ret = scf_vector_add(all, bb);
		if (ret < 0)
			goto error;
	}

	scf_vector_qsort(all, _bb_cmp_dfo);

	for (i = 0; i < all->size; i++) {
		bb =        all->data[i];

		scf_vector_qsort(bb->prevs, _bb_cmp_dfo);
		scf_vector_qsort(bb->nexts, _bb_cmp_dfo);

		if (0 == bb->dfo) {

			if (!bb->dominators) {
				bb->dominators = scf_vector_alloc();
				if (!bb->dominators) {
					ret = -ENOMEM;
					goto error;
				}
			} else
				scf_vector_clear(bb->dominators, NULL);

			ret = scf_vector_add(bb->dominators, bb);
			if (ret < 0)
				goto error;

			scf_logd("bb: %p_%d, dom size: %d\n", bb, bb->dfo, bb->dominators->size);
			continue;
		}

		if (bb->dominators)
			scf_vector_free(bb->dominators);

		bb->dominators = scf_vector_clone(all);
		if (!bb->dominators) {
			ret = -ENOMEM;
			goto error;
		}

		scf_logd("bb: %p_%d, dom size: %d\n", bb, bb->dfo, bb->dominators->size);
	}

	do {
		changed = 0;

		for (i = 1; i < all->size; i++) {
			bb =        all->data[i];

			scf_vector_t* dominators = NULL;

			for (j = 0; j < bb->nexts->size; j++) {
				scf_basic_block_t* next = bb->nexts->data[j];

				if (!dominators) {
					dominators = scf_vector_clone(next->dominators);
					if (!dominators) {
						ret = -ENOMEM;
						goto error;
					}
					continue;
				}

				ret = _bb_intersection(dominators, next->dominators);
				if (ret < 0) {
					scf_vector_free(dominators);
					goto error;
				}
			}

			scf_logd("bb: %p, dominators: %p, bb->nexts->size: %d\n",
					bb, dominators, bb->nexts->size);

			scf_basic_block_t* dom  = NULL;
			scf_basic_block_t* dom1 = bb;

			for (j = 0; j < dominators->size; j++) {
				dom       = dominators->data[j];

				if (bb->dfo == dom->dfo)
					break;

				if (bb->dfo < dom->dfo)
					break;
			}
			if (bb->dfo < dom->dfo) {

				for (; j < dominators->size; j++) {
					dom                 = dominators->data[j];
					dominators->data[j] = dom1;
					dom1                = dom;
				}
			}
			if (j == dominators->size) {
				ret = scf_vector_add(dominators, dom1);
				if (ret < 0) {
					scf_vector_free(dominators);
					goto error;
				}
			}

			if (dominators->size != bb->dominators->size)
				++changed;
			else {
				int k0 = 0;
				int k1 = 0;

				while (k0 < dominators->size && k1 < bb->dominators->size) {

					scf_basic_block_t* dom0 =     dominators->data[k0];
					scf_basic_block_t* dom1 = bb->dominators->data[k1];

					if (dom0->dfo < dom1->dfo) {
						++changed;
						break;
					} else if (dom0->dfo > dom1->dfo) {
						++changed;
						break;
					} else {
						++k0;
						++k1;
					}
				}
			}

			scf_vector_free(bb->dominators);
			bb->dominators = dominators;
			dominators     = NULL;
		}
	} while (changed > 0);
#if 0
	for (i = 0; i < all->size; i++) {
		bb =        all->data[i];

		int j;
		for (j = 0; j < bb->dominators->size; j++) {

			scf_basic_block_t* dom = bb->dominators->data[j];
			scf_logi("bb: %p_%d, dom: %p_%d\n", bb, bb->dfo, dom, dom->dfo);
		}
		printf("\n");
	}
#endif
	ret = 0;
error:
	scf_vector_free(all);
	return ret;
}

static int _optimize_dominators_reverse(scf_ast_t* ast, scf_function_t* f, scf_vector_t* functions)
{
	if (!f)
		return -EINVAL;

	scf_bb_edge_t*  edge;
	scf_vector_t*   tree;
	scf_list_t*     bb_list_head = &f->basic_block_list_head;

	int ret;
	int i;

	if (scf_list_empty(bb_list_head))
		return 0;

	tree = scf_vector_alloc();
	if (!tree)
		return -ENOMEM;

	ret = _bb_dfs_tree(bb_list_head, tree);
	if (ret < 0)
		return ret;

	for (i = 0; i < tree->size; i++) {
		edge      = tree->data[i];
		scf_logd("bb_%p_%d --> bb_%p_%d\n", edge->start, edge->start->dfo, edge->end, edge->end->dfo);
	}

	scf_vector_clear(tree, free);
	scf_vector_free(tree);
	tree = NULL;

	ret = __find_reverse_dominators(bb_list_head);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

//	scf_basic_block_print_list(bb_list_head);
	return 0;
}

scf_optimizer_t  scf_optimizer_dominators_reverse =
{
	.name     =  "dominators_reverse",

	.optimize =  _optimize_dominators_reverse,
};
