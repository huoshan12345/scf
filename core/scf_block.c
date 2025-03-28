#include"scf_block.h"
#include"scf_scope.h"

scf_block_t* scf_block_alloc(scf_lex_word_t* w)
{
	scf_block_t* b = calloc(1, sizeof(scf_block_t));
	if (!b)
		return NULL;

	b->scope = scf_scope_alloc(w, "block");
	if (!b->scope) {
		free(b);
		return NULL;
	}

	if (w) {
		b->node.w = scf_lex_word_clone(w);

		if (!b->node.w) {
			scf_scope_free(b->scope);
			free(b);
			return NULL;
		}
	}

	b->node.type = SCF_OP_BLOCK;
	return b;
}

scf_block_t* scf_block_alloc_cstr(const char* name)
{
	scf_block_t* b = calloc(1, sizeof(scf_block_t));
	if (!b)
		return NULL;

	b->name = scf_string_cstr(name);
	if (!b->name) {
		free(b);
		return NULL;
	}

	b->scope = scf_scope_alloc(NULL, name);
	if (!b->scope) {
		scf_string_free(b->name);
		free(b);
		return NULL;
	}

	b->node.type = SCF_OP_BLOCK;
	return b;
}

void scf_block_free(scf_block_t* b)
{
	if (b) {
		scf_scope_free(b->scope);
		b->scope = NULL;

		if (b->name) {
			scf_string_free(b->name);
			b->name = NULL;
		}

		scf_node_free((scf_node_t*)b);
	}
}

scf_type_t*	scf_block_find_type(scf_block_t* b, const char* name)
{
	assert(b);
	while (b) {
//		printf("%s(),%d, ### b: %p, parent: %p\n", __func__, __LINE__, b, b->node.parent);
		if (SCF_OP_BLOCK == b->node.type || SCF_FUNCTION == b->node.type || b->node.type >= SCF_STRUCT) {

			if (b->scope) {
				scf_type_t* t = scf_scope_find_type(b->scope, name);
				if (t)
					return t;
			}
		}
		b = (scf_block_t*)(b->node.parent);
	}
	return NULL;
}

scf_type_t*	scf_block_find_type_type(scf_block_t* b, const int type)
{
	assert(b);
	while (b) {
		if (SCF_OP_BLOCK == b->node.type || SCF_FUNCTION == b->node.type || b->node.type >= SCF_STRUCT) {

			if (b->scope) {
				scf_type_t* t = scf_scope_find_type_type(b->scope, type);
				if (t)
					return t;
			}
		}
		b = (scf_block_t*)(b->node.parent);
	}
	return NULL;
}

scf_variable_t*	scf_block_find_variable(scf_block_t* b, const char* name)
{
	assert(b);
	while (b) {
		if (SCF_OP_BLOCK == b->node.type || SCF_FUNCTION == b->node.type || b->node.type >= SCF_STRUCT) {

			if (b->scope) {
				scf_variable_t* v = scf_scope_find_variable(b->scope, name);
				if (v)
					return v;
			}
		}
		b = (scf_block_t*)(b->node.parent);
	}
	return NULL;
}

scf_function_t*	scf_block_find_function(scf_block_t* b, const char* name)
{
	assert(b);
	while (b) {
		if (SCF_OP_BLOCK == b->node.type || SCF_FUNCTION == b->node.type || b->node.type >= SCF_STRUCT) {

			if (b->scope) {
				scf_function_t* f = scf_scope_find_function(b->scope, name);
				if (f)
					return f;
			}
		}

		b = (scf_block_t*)(b->node.parent);
	}
	return NULL;
}

scf_label_t* scf_block_find_label(scf_block_t* b, const char* name)
{
	assert(b);
	while (b) {
		if (SCF_OP_BLOCK == b->node.type || SCF_FUNCTION == b->node.type) {
			assert(b->scope);

			scf_label_t* l = scf_scope_find_label(b->scope, name);
			if (l)
				return l;
		}

		b = (scf_block_t*)(b->node.parent);
	}
	return NULL;
}
