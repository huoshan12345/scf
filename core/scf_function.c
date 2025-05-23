#include"scf_function.h"
#include"scf_scope.h"
#include"scf_type.h"
#include"scf_block.h"

scf_function_t* scf_function_alloc(scf_lex_word_t* w)
{
	assert(w);

	scf_function_t* f = calloc(1, sizeof(scf_function_t));
	if (!f)
		return NULL;

	f->node.type = SCF_FUNCTION;
	f->op_type   = -1;

	scf_list_init(&f->basic_block_list_head);
	scf_list_init(&f->dag_list_head);

	f->scope = scf_scope_alloc(w, "function");
	if (!f->scope)
		goto _scope_error;

	f->node.w = scf_lex_word_clone(w);
	if (!f->node.w)
		goto _word_error;

	f->rets = scf_vector_alloc();
	if (!f->rets)
		goto _ret_error;

	f->argv = scf_vector_alloc();
	if (!f->argv)
		goto _argv_error;

	f->callee_functions = scf_vector_alloc();
	if (!f->callee_functions)
		goto _callee_error;

	f->caller_functions = scf_vector_alloc();
	if (!f->caller_functions)
		goto _caller_error;

	f->jmps = scf_vector_alloc();
	if (!f->jmps)
		goto _jmps_error;

	f->dfs_tree = scf_vector_alloc();
	if (!f->dfs_tree)
		goto _dfs_tree_error;

	f->bb_loops = scf_vector_alloc();
	if (!f->bb_loops)
		goto _loop_error;

	f->bb_groups = scf_vector_alloc();
	if (!f->bb_groups)
		goto _group_error;

	f->text_relas = scf_vector_alloc();
	if (!f->text_relas)
		goto _text_rela_error;

	f->data_relas = scf_vector_alloc();
	if (!f->data_relas)
		goto _data_rela_error;

	return f;

_data_rela_error:
	scf_vector_free(f->text_relas);
_text_rela_error:
	scf_vector_free(f->bb_groups);
_group_error:
	scf_vector_free(f->bb_loops);
_loop_error:
	scf_vector_free(f->dfs_tree);
_dfs_tree_error:
	scf_vector_free(f->jmps);
_jmps_error:
	scf_vector_free(f->caller_functions);
_caller_error:
	scf_vector_free(f->callee_functions);
_callee_error:
	scf_vector_free(f->argv);
_argv_error:
	scf_vector_free(f->rets);
_ret_error:
	scf_lex_word_free(f->node.w);
_word_error:
	scf_scope_free(f->scope);
_scope_error:
	free(f);
	return NULL;
}

void scf_function_free(scf_function_t* f)
{
	if (f) {
		scf_scope_free(f->scope);
		f->scope = NULL;

		if (f->signature) {
			scf_string_free(f->signature);
			f->signature = NULL;
		}

		if (f->rets) {
			scf_vector_clear(f->rets, ( void (*)(void*) ) scf_variable_free);
			scf_vector_free (f->rets);
		}

		if (f->argv) {
			scf_vector_clear(f->argv, ( void (*)(void*) ) scf_variable_free);
			scf_vector_free (f->argv);
			f->argv = NULL;
		}

		if (f->callee_functions)
			scf_vector_free(f->callee_functions);

		if (f->caller_functions)
			scf_vector_free(f->caller_functions);

		if (f->jmps) {
			scf_vector_free(f->jmps);
			f->jmps = NULL;
		}

		if (f->text_relas) {
			scf_vector_free(f->text_relas);
			f->text_relas = NULL;
		}

		if (f->data_relas) {
			scf_vector_free(f->data_relas);
			f->data_relas = NULL;
		}

		scf_node_free((scf_node_t*)f);
	}
}

int scf_function_same(scf_function_t* f0, scf_function_t* f1)
{
	if (strcmp(f0->node.w->text->data, f1->node.w->text->data))
		return 0;

	return scf_function_same_type(f0, f1);
}

int scf_function_same_argv(scf_vector_t* argv0, scf_vector_t* argv1)
{
	if (argv0) {
		if (!argv1)
			return 0;

		if (argv0->size != argv1->size)
			return 0;

		int i;
		for (i = 0; i < argv0->size; i++) {
			scf_variable_t* v0 = argv0->data[i];
			scf_variable_t* v1 = argv1->data[i];

			if (!scf_variable_type_like(v0, v1))
				return 0;
		}
	} else {
		if (argv1)
			return 0;
	}

	return 1;
}

int scf_function_like_argv(scf_vector_t* argv0, scf_vector_t* argv1)
{
	if (argv0) {
		if (!argv1)
			return 0;

		if (argv0->size != argv1->size)
			return 0;

		int i;
		for (i = 0; i < argv0->size; i++) {
			scf_variable_t* v0 = argv0->data[i];
			scf_variable_t* v1 = argv1->data[i];

			if (scf_variable_type_like(v0, v1))
				continue;

			if (scf_variable_is_struct_pointer(v0) || scf_variable_is_struct_pointer(v1))
				return 0;
		}
	} else {
		if (argv1)
			return 0;
	}

	return 1;
}

int scf_function_same_type(scf_function_t* f0, scf_function_t* f1)
{
	if (f0->rets) {
		if (!f1->rets)
			return 0;

		if (f0->rets->size != f1->rets->size)
			return 0;

		int i;
		for (i = 0; i < f0->rets->size; i++) {
			if (!scf_variable_type_like(f0->rets->data[i], f1->rets->data[i]))
				return 0;
		}
	} else if (f1->rets)
		return 0;

	return scf_function_same_argv(f0->argv, f1->argv);
}

int scf_function_signature(scf_function_t* f)
{
	scf_string_t* s;
	scf_type_t*   t = (scf_type_t*)f->node.parent;

	int ret;
	int i;

	s = scf_string_alloc();
	if (!s)
		return -ENOMEM;

	if (t->node.type >= SCF_STRUCT) {
		assert(t->node.class_flag);

		ret = scf_string_cat(s, t->name);
		if (ret < 0)
			goto error;

		ret = scf_string_cat_cstr(s, "_");
		if (ret < 0)
			goto error;
	}

	if (f->op_type >= 0) {
		scf_operator_t* op = scf_find_base_operator_by_type(f->op_type);

		if (!op->signature)
			goto error;

		ret = scf_string_cat_cstr(s, op->signature);
	} else
		ret = scf_string_cat(s, f->node.w->text);

	if (ret < 0)
		goto error;
	scf_logd("f signature: %s\n", s->data);

	if (t->node.type < SCF_STRUCT) {
		if (f->signature)
			scf_string_free(f->signature);

		f->signature = s;
		return 0;
	}

	if (f->argv) {
		for (i = 0; i < f->argv->size; i++) {
			scf_variable_t* v   = f->argv->data[i];
			scf_type_t*     t_v = scf_block_find_type_type((scf_block_t*)t, v->type);

			ret = scf_string_cat_cstr(s, "_");
			if (ret < 0)
				goto error;

			const char* abbrev = scf_type_find_abbrev(t_v->name->data);
			if (abbrev)
				ret = scf_string_cat_cstr(s, abbrev);
			else
				ret = scf_string_cat(s, t_v->name);
			if (ret < 0)
				goto error;

			if (v->nb_pointers > 0) {
				char buf[64];
				snprintf(buf, sizeof(buf) - 1, "%d", v->nb_pointers);

				ret = scf_string_cat_cstr(s, buf);
				if (ret < 0)
					goto error;
			}
		}
	}

	scf_logd("f signature: %s\n", s->data);

	if (f->signature)
		scf_string_free(f->signature);
	f->signature = s;
	return 0;

error:
	scf_string_free(s);
	return -1;
}
