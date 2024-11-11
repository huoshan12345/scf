#include"scf_dfa.h"
#include"scf_parse.h"

static int __reshape_index(dfa_index_t** out, scf_variable_t* array, dfa_index_t* index, int n)
{
	assert(array->nb_dimentions > 0);
	assert(n > 0);

	dfa_index_t* p = calloc(array->nb_dimentions, sizeof(dfa_index_t));
	if (!p)
		return -ENOMEM;

	intptr_t i = index[n - 1].i;

	scf_logw("reshape 'init exprs' from %d-dimention to %d-dimention, origin last index: %ld\n",
			n, array->nb_dimentions, i);

	int j;
	for (j = array->nb_dimentions - 1; j >= 0; j--) {

		if (array->dimentions[j] <= 0) {
			scf_logw("array's %d-dimention size not set, file: %s, line: %d\n", j, array->w->file->data, array->w->line);

			free(p);
			return -1;
		}

		p[j].i = i % array->dimentions[j];
		i      = i / array->dimentions[j];
	}

	for (j = 0; j < array->nb_dimentions; j++)
		scf_logi("\033[32m dim: %d, size: %d, index: %ld\033[0m\n", j, array->dimentions[j], p[j].i);

	*out = p;
	return 0;
}

static int __array_member_init(scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* array, dfa_index_t* index, int n, scf_node_t** pnode)
{
	if (!pnode)
		return -1;

	scf_type_t* t    = scf_block_find_type_type(ast->current_block, SCF_VAR_INT);
	scf_node_t* root = *pnode;

	if (!root)
		root = scf_node_alloc(NULL, array->type, array);

	if (n < array->nb_dimentions) {
		if (n <= 0) {
			scf_loge("number of indexes less than needed, array '%s', file: %s, line: %d\n",
					array->w->text->data, w->file->data, w->line);
			return -1;
		}

		int ret = __reshape_index(&index, array, index, n);
		if (ret < 0)
			return ret;
	}

	int i;
	for (i = 0; i < array->nb_dimentions; i++) {

		intptr_t k = index[i].i;

		if (k >= array->dimentions[i]) {
			scf_loge("index [%ld] out of size [%d], in dim: %d, file: %s, line: %d\n",
					k, array->dimentions[i], i, w->file->data, w->line);

			if (n < array->nb_dimentions) {
				free(index);
				index = NULL;
			}
			return -1;
		}

		scf_variable_t* v_index     = scf_variable_alloc(NULL, t);
		v_index->const_flag         = 1;
		v_index->const_literal_flag = 1;
		v_index->data.i64           = k;

		scf_node_t* node_index = scf_node_alloc(NULL, v_index->type,  v_index);
		scf_node_t* node_op    = scf_node_alloc(w,    SCF_OP_ARRAY_INDEX, NULL);

		scf_node_add_child(node_op, root);
		scf_node_add_child(node_op, node_index);
		root = node_op;
	}

	if (n < array->nb_dimentions) {
		free(index);
		index = NULL;
	}

	*pnode = root;
	return array->nb_dimentions;
}

int scf_struct_member_init(scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* _struct, dfa_index_t* index, int n, scf_node_t** pnode)
{
	if (!pnode)
		return -1;

	scf_variable_t* v    = NULL;
	scf_type_t*     t    = scf_block_find_type_type(ast->current_block, _struct->type);
	scf_node_t*     root = *pnode;

	if (!root)
		root = scf_node_alloc(NULL, _struct->type,  _struct);

	int j = 0;
	while (j < n) {

		if (!t->scope) {
			scf_loge("\n");
			return -1;
		}

		int k;

		if (index[j].w) {
			for (k = 0; k < t->scope->vars->size; k++) {
				v  =        t->scope->vars->data[k];

				if (v->w && !strcmp(index[j].w->text->data, v->w->text->data))
					break;
			}
		} else
			k = index[j].i;

		if (k >= t->scope->vars->size) {
			scf_loge("\n");
			return -1;
		}

		v = t->scope->vars->data[k];

		scf_node_t* node_op = scf_node_alloc(w,    SCF_OP_POINTER, NULL);
		scf_node_t* node_v  = scf_node_alloc(NULL, v->type,        v);

		scf_node_add_child(node_op, root);
		scf_node_add_child(node_op, node_v);
		root = node_op;

		scf_logi("j: %d, k: %d, v: '%s'\n", j, k, v->w->text->data);
		j++;

		if (v->nb_dimentions > 0) {

			int ret = __array_member_init(ast, w, v, index + j, n - j, &root);
			if (ret < 0)
				return -1;

			j += ret;
			scf_logi("struct var member: %s->%s[]\n", _struct->w->text->data, v->w->text->data);
		}

		if (v->type < SCF_STRUCT || v->nb_pointers > 0) {
			// if 'v' is a base type var or a pointer, and of course 'v' isn't an array,
			// we can't get the member of v !!
			// the index must be the last one, and its expr is to init v !
			if (j < n - 1) {
				scf_loge("number of indexes more than needed, struct member: %s->%s, file: %s, line: %d\n",
						_struct->w->text->data, v->w->text->data, w->file->data, w->line);
				return -1;
			}

			scf_logi("struct var member: %s->%s\n", _struct->w->text->data, v->w->text->data);

			*pnode = root;
			return n;
		}

		// 'v' is not a base type var or a pointer, it's a struct
		// first, find the type in this struct scope, then find in global
		scf_type_t* type_v = NULL;

		while (t) {
			type_v = scf_scope_find_type_type(t->scope, v->type);
			if (type_v)
				break;

			// only can use types in this scope, or parent scope
			// can't use types in children scope
			t = t->parent;
		}

		if (!type_v) {
			type_v = scf_block_find_type_type(ast->current_block, v->type);

			if (!type_v) {
				scf_loge("\n");
				return -1;
			}
		}

		t = type_v;
	}

	scf_loge("number of indexes less than needed, struct member: %s->%s, file: %s, line: %d\n",
			_struct->w->text->data, v->w->text->data, w->file->data, w->line);
	return -1;
}

int scf_array_member_init(scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* array, dfa_index_t* index, int n, scf_node_t** pnode)
{
	scf_node_t* root = NULL;

	int ret = __array_member_init(ast, w, array, index, n, &root);
	if (ret < 0)
		return ret;

	if (array->type < SCF_STRUCT || array->nb_pointers > 0) {

		if (ret < n - 1) {
			scf_loge("\n");
			return -1;
		}

		*pnode = root;
		return n;
	}

	ret = scf_struct_member_init(ast, w, array, index + ret, n - ret, &root);
	if (ret < 0)
		return ret;

	*pnode = root;
	return n;
}

int scf_array_init(scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* v, scf_vector_t* init_exprs)
{
	dfa_init_expr_t* ie;

	int unset     = 0;
	int unset_dim = -1;
	int capacity  = 1;
	int i;
	int j;

	for (i = 0; i < v->nb_dimentions; i++) {
		assert(v->dimentions);

		scf_logi("dim[%d]: %d\n", i, v->dimentions[i]);

		if (v->dimentions[i] < 0) {

			if (unset > 0) {
				scf_loge("array '%s' should only unset 1-dimention size, file: %s, line: %d\n",
						v->w->text->data, w->file->data, w->line);
				return -1;
			}

			unset++;
			unset_dim = i;
		} else
			capacity *= v->dimentions[i];
	}

	if (unset) {
		int unset_max = -1;

		for (i = 0; i < init_exprs->size; i++) {
			ie =        init_exprs->data[i];

			if (unset_dim < ie->n) {
				if (unset_max < ie->index[unset_dim].i)
					unset_max = ie->index[unset_dim].i;
			}
		}

		if (-1 == unset_max) {
			unset_max = init_exprs->size / capacity;

			v->dimentions[unset_dim] = unset_max;

			scf_logw("don't set %d-dimention size of array '%s', use '%d' as calculated, file: %s, line: %d\n",
					unset_dim, v->w->text->data, unset_max, w->file->data, w->line);
		} else
			v->dimentions[unset_dim] = unset_max + 1;
	}

	for (i = 0; i < init_exprs->size; i++) {
		ie =        init_exprs->data[i];

		if (ie->n < v->nb_dimentions) {
			int n = ie->n;

			void* p = realloc(ie, sizeof(dfa_init_expr_t) + sizeof(dfa_index_t) * v->nb_dimentions);
			if (!p)
				return -ENOMEM;
			init_exprs->data[i] = p;

			ie    = p;
			ie->n = v->nb_dimentions;

			intptr_t index = ie->index[n - 1].i;

			scf_logw("reshape 'init exprs' from %d-dimention to %d-dimention, origin last index: %ld\n", n, v->nb_dimentions, index);

			for (j = v->nb_dimentions - 1; j >= 0; j--) {

				ie->index[j].i = index % v->dimentions[j];
				index          = index / v->dimentions[j];
			}
		}

		for (j = 0; j < v->nb_dimentions; j++)
			scf_logi("\033[32mi: %d, dim: %d, size: %d, index: %ld\033[0m\n", i, j, v->dimentions[j], ie->index[j].i);
	}

	for (i = 0; i < init_exprs->size; i++) {
		ie =        init_exprs->data[i];

		scf_logi("#### data init, i: %d, init expr: %p\n", i, ie->expr);

		scf_expr_t* e;
		scf_node_t* assign;
		scf_node_t* node = NULL;

		if (scf_array_member_init(ast, w, v, ie->index, ie->n, &node) < 0) {
			scf_loge("\n");
			return -1;
		}

		e      = scf_expr_alloc();
		assign = scf_node_alloc(w, SCF_OP_ASSIGN, NULL);

		scf_node_add_child(assign, node);
		scf_node_add_child(assign, ie->expr);
		scf_expr_add_node(e, assign);

		ie->expr = e;
		printf("\n");
	}

	return 0;
}

int scf_struct_init(scf_ast_t* ast, scf_lex_word_t* w, scf_variable_t* var, scf_vector_t* init_exprs)
{
	dfa_init_expr_t* ie;

	int i;
	for (i = 0; i < init_exprs->size; i++) {
		ie =        init_exprs->data[i];

		scf_logi("#### struct init, i: %d, init_expr->expr: %p\n", i, ie->expr);

		scf_expr_t* e;
		scf_node_t* assign;
		scf_node_t* node = NULL;

		if (scf_struct_member_init(ast, w, var, ie->index, ie->n, &node) < 0)
			return -1;

		e      = scf_expr_alloc();
		assign = scf_node_alloc(w, SCF_OP_ASSIGN, NULL);

		scf_node_add_child(assign, node);
		scf_node_add_child(assign, ie->expr);
		scf_expr_add_node(e, assign);

		ie->expr = e;
		printf("\n");
	}

	return 0;
}
