#include"scf_ast.h"

scf_scope_t* scf_scope_alloc(scf_lex_word_t* w, const char* name)
{
	scf_scope_t* scope = calloc(1, sizeof(scf_scope_t));
	if (!scope)
		return NULL;

	scope->name = scf_string_cstr(name);
	if (!scope->name) {
		free(scope);
		return NULL;
	}

	scope->vars = scf_vector_alloc();
	if (!scope->vars) {
		scf_string_free(scope->name);
		free(scope);
		return NULL;
	}

	if (w) {
		scope->w = scf_lex_word_clone(w);

		if (!scope->w) {
			scf_vector_free(scope->vars);
			scf_string_free(scope->name);
			free(scope);
			return NULL;
		}
	}

	scf_list_init(&scope->list);
	scf_list_init(&scope->type_list_head);
	scf_list_init(&scope->operator_list_head);
	scf_list_init(&scope->function_list_head);
	scf_list_init(&scope->label_list_head);
	return scope;
}

void scf_scope_push_var(scf_scope_t* scope, scf_variable_t* var)
{
	assert(scope);
	assert(var);
	scf_vector_add(scope->vars, var);
}

void scf_scope_push_type(scf_scope_t* scope, scf_type_t* t)
{
	assert(scope);
	assert(t);
	scf_list_add_front(&scope->type_list_head, &t->list);
}

void scf_scope_push_operator(scf_scope_t* scope, scf_function_t* op)
{
	assert(scope);
	assert(op);
	scf_list_add_front(&scope->operator_list_head, &op->list);
}

void scf_scope_push_function(scf_scope_t* scope, scf_function_t* f)
{
	assert(scope);
	assert(f);
	scf_list_add_front(&scope->function_list_head, &f->list);
}

void scf_scope_push_label(scf_scope_t* scope, scf_label_t* l)
{
	assert(scope);
	assert(l);
	scf_list_add_front(&scope->label_list_head, &l->list);
}

void scf_scope_free(scf_scope_t* scope)
{
	if (scope) {
		scf_string_free(scope->name);
		scope->name = NULL;

		if (scope->w)
			scf_lex_word_free(scope->w);

		scf_vector_clear(scope->vars, (void (*)(void*))scf_variable_free);
		scf_vector_free(scope->vars);
		scope->vars = NULL;

		scf_list_clear(&scope->type_list_head, scf_type_t, list, scf_type_free);
		scf_list_clear(&scope->function_list_head, scf_function_t, list, scf_function_free);

		free(scope);
	}
}

scf_type_t*	scf_scope_find_type(scf_scope_t* scope, const char* name)
{
	scf_type_t* t;
	scf_list_t* l;

	for (l = scf_list_head(&scope->type_list_head); l != scf_list_sentinel(&scope->type_list_head); l = scf_list_next(l)) {
		t  = scf_list_data(l, scf_type_t, list);

		if (!strcmp(name, t->name->data)) {
			return t;
		}
	}
	return NULL;
}

scf_type_t*	scf_scope_find_type_type(scf_scope_t* scope, const int type)
{
	scf_type_t* t;
	scf_list_t* l;

	for (l = scf_list_head(&scope->type_list_head); l != scf_list_sentinel(&scope->type_list_head); l = scf_list_next(l)) {
		t  = scf_list_data(l, scf_type_t, list);

		if (type == t->type)
			return t;
	}
	return NULL;
}

scf_variable_t*	scf_scope_find_variable(scf_scope_t* scope, const char* name)
{
	scf_variable_t* v;
	int i;

	for (i = 0; i < scope->vars->size; i++) {
		v  =        scope->vars->data[i];

		if (v->w && !strcmp(name, v->w->text->data))
			return v;
	}
	return NULL;
}

scf_label_t* scf_scope_find_label(scf_scope_t* scope, const char* name)
{
	scf_label_t* label;
	scf_list_t*  l;

	for (l    = scf_list_head(&scope->label_list_head); l != scf_list_sentinel(&scope->label_list_head); l = scf_list_next(l)) {
		label = scf_list_data(l, scf_label_t, list);

		if (!strcmp(name, label->w->text->data))
			return label;
	}
	return NULL;
}

scf_function_t*	scf_scope_find_function(scf_scope_t* scope, const char* name)
{
	scf_function_t* f;
	scf_list_t*     l;

	for (l = scf_list_head(&scope->function_list_head); l != scf_list_sentinel(&scope->function_list_head); l = scf_list_next(l)) {
		f  = scf_list_data(l, scf_function_t, list);

		if (!strcmp(name, f->node.w->text->data))
			return f;
	}
	return NULL;
}

scf_function_t*	scf_scope_find_same_function(scf_scope_t* scope, scf_function_t* f0)
{
	scf_function_t* f1;
	scf_list_t*     l;

	for (l = scf_list_head(&scope->function_list_head); l != scf_list_sentinel(&scope->function_list_head); l = scf_list_next(l)) {
		f1 = scf_list_data(l, scf_function_t, list);

		if (scf_function_same(f0, f1))
			return f1;
	}
	return NULL;
}

scf_function_t*	scf_scope_find_proper_function(scf_scope_t* scope, const char* name, scf_vector_t* argv)
{
	scf_function_t* f;
	scf_list_t*     l;

	for (l = scf_list_head(&scope->function_list_head); l != scf_list_sentinel(&scope->function_list_head); l = scf_list_next(l)) {
		f  = scf_list_data(l, scf_function_t, list);

		if (strcmp(f->node.w->text->data, name))
			continue;

		if (scf_function_same_argv(f->argv, argv))
			return f;
	}
	return NULL;
}

int scf_scope_find_like_functions(scf_vector_t** pfunctions, scf_scope_t* scope, const char* name, scf_vector_t* argv)
{
	scf_function_t* f;
	scf_vector_t*   vec;
	scf_list_t*     l;

	vec = scf_vector_alloc();
	if (!vec)
		return -ENOMEM;

	for (l = scf_list_head(&scope->function_list_head); l != scf_list_sentinel(&scope->function_list_head); l = scf_list_next(l)) {
		f  = scf_list_data(l, scf_function_t, list);

		if (strcmp(f->node.w->text->data, name))
			continue;

		if (!scf_function_like_argv(f->argv, argv))
			continue;

		int ret = scf_vector_add(vec, f);
		if (ret < 0) {
			scf_vector_free(vec);
			return ret;
		}
	}

	if (0 == vec->size) {
		scf_vector_free(vec);
		return -404;
	}

	*pfunctions = vec;
	return 0;
}

int scf_scope_find_overloaded_functions(scf_vector_t** pfunctions, scf_scope_t* scope, const int op_type, scf_vector_t* argv)
{
	scf_function_t* f;
	scf_vector_t*   vec;
	scf_list_t*     l;

	vec = scf_vector_alloc();
	if (!vec)
		return -ENOMEM;

	for (l = scf_list_head(&scope->operator_list_head); l != scf_list_sentinel(&scope->operator_list_head); l = scf_list_next(l)) {
		f  = scf_list_data(l, scf_function_t, list);

		if (op_type != f->op_type)
			continue;

		if (!scf_function_like_argv(f->argv, argv))
			continue;

		int ret = scf_vector_add(vec, f);
		if (ret < 0) {
			scf_vector_free(vec);
			return ret;
		}
	}

	if (0 == vec->size) {
		scf_vector_free(vec);
		return -404;
	}

	*pfunctions = vec;
	return 0;
}
