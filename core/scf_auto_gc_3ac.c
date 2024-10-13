#include"scf_optimizer.h"
#include"scf_pointer_alias.h"

static scf_3ac_operand_t* _auto_gc_operand_alloc_pf(scf_list_t* dag_list_head, scf_ast_t* ast, scf_function_t* f)
{
	scf_3ac_operand_t* src;
	scf_dag_node_t*    dn;
	scf_variable_t*    v;
	scf_type_t*        t = NULL;

	int ret = scf_ast_find_type_type(&t, ast, SCF_FUNCTION_PTR);
	assert(0 == ret);
	assert(t);

	if (f)
		v = SCF_VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	else
		v = SCF_VAR_ALLOC_BY_TYPE(NULL, t, 1, 1, NULL);

	if (!v)
		return NULL;
	v->const_literal_flag = 1;

	dn = scf_dag_node_alloc(v->type, v, (scf_node_t*)f);

	scf_variable_free(v);
	v = NULL;
	if (!dn)
		return NULL;

	src = scf_3ac_operand_alloc();
	if (!src) {
		scf_dag_node_free(dn);
		return NULL;
	}

	src->node     = (scf_node_t*)f;
	src->dag_node = dn;

	scf_list_add_tail(dag_list_head, &dn->list);
	return src;
}

static scf_3ac_code_t* _auto_gc_code_ref(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn)
{
	scf_3ac_operand_t* src;
	scf_3ac_code_t*    c;
	scf_function_t*    f = NULL;
	scf_variable_t*    v;
	scf_type_t*        t;

	int ret = scf_ast_find_function(&f, ast, "scf__auto_ref");
	if (!f)
		return NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->op = scf_3ac_find_operator(SCF_OP_CALL);

#define AUTO_GC_CODE_ADD_FUNCPTR() \
	do { \
		src = _auto_gc_operand_alloc_pf(dag_list_head, ast, f); \
		if (!src) { \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
		\
		if (scf_vector_add(c->srcs, src) < 0) { \
			scf_3ac_operand_free(src); \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
	} while (0)

	AUTO_GC_CODE_ADD_FUNCPTR();

#define AUTO_GC_CODE_ADD_DN() \
	do { \
		src = scf_3ac_operand_alloc(); \
		if (!src) { \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
		src->node     = dn->node; \
		src->dag_node = dn; \
		\
		if (scf_vector_add(c->srcs, src) < 0) { \
			scf_3ac_operand_free(src); \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
	} while (0)

	AUTO_GC_CODE_ADD_DN();

	return c;
}

static scf_3ac_code_t* _auto_gc_code_memset_array(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_array)
{
	scf_3ac_operand_t* src;
	scf_3ac_code_t*    c;
	scf_dag_node_t*    dn;
	scf_function_t*    f;
	scf_variable_t*    v;
	scf_type_t*        t = NULL;

	int ret = scf_ast_find_type_type(&t, ast, SCF_VAR_INTPTR);
	assert(0 == ret);
	assert(t);

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_3AC_MEMSET);

	dn = dn_array;
	AUTO_GC_CODE_ADD_DN();

#define AUTO_GC_CODE_ADD_VAR() \
	do { \
		dn = scf_dag_node_alloc(v->type, v, NULL); \
		\
		scf_variable_free(v); \
		v = NULL; \
		\
		if (!dn) { \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
		src = scf_3ac_operand_alloc(); \
		if (!src) { \
			scf_dag_node_free(dn); \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
		\
		if (scf_vector_add(c->srcs, src) < 0) { \
			scf_3ac_operand_free(src); \
			scf_dag_node_free(dn); \
			scf_3ac_code_free(c); \
			return NULL; \
		} \
		\
		src->node     = dn->node; \
		src->dag_node = dn; \
		scf_list_add_tail(dag_list_head, &dn->list); \
	} while (0)

	v = SCF_VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}
	v->data.i64 = 0;
	v->const_literal_flag = 1;
	AUTO_GC_CODE_ADD_VAR();

	v = SCF_VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}
	v->data.i64 = scf_variable_size(dn_array->var);
	v->const_literal_flag = 1;
	AUTO_GC_CODE_ADD_VAR();

	return c;
}

static scf_3ac_code_t* _auto_gc_code_free_array(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_array, int capacity, int nb_pointers)
{
	scf_3ac_operand_t* src;
	scf_3ac_code_t*    c;
	scf_dag_node_t*    dn;
	scf_function_t*    f = NULL;
	scf_variable_t*    v;
	scf_type_t*        t;

	int ret = scf_ast_find_function(&f, ast, "scf__auto_free_array");
	if (!f)
		return NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_CALL);

	AUTO_GC_CODE_ADD_FUNCPTR();

	dn = dn_array;
	AUTO_GC_CODE_ADD_DN();

	t = scf_block_find_type_type(ast->current_block, SCF_VAR_INTPTR);
	v = SCF_VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}
	v->data.i64 = capacity;
	v->const_literal_flag = 1;
	AUTO_GC_CODE_ADD_VAR();

	v = SCF_VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}
	v->data.i64 = nb_pointers;
	v->const_literal_flag = 1;
	AUTO_GC_CODE_ADD_VAR();

	if (dn_array->var->type >= SCF_STRUCT) {
		t   = NULL;
		ret = scf_ast_find_type_type(&t, ast, dn_array->var->type);
		assert(0 == ret);
		assert(t);

		f = scf_scope_find_function(t->scope, "__release");
	} else
		f = NULL;
	AUTO_GC_CODE_ADD_FUNCPTR();

	return c;
}

static scf_3ac_code_t* _auto_gc_code_freep_array(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_array, int nb_pointers)
{
	scf_3ac_operand_t* src;
	scf_3ac_code_t*    c;
	scf_dag_node_t*    dn;
	scf_function_t*    f = NULL;
	scf_variable_t*    v;
	scf_type_t*        t;

	int ret = scf_ast_find_function(&f, ast, "scf__auto_freep_array");
	if (!f)
		return NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_CALL);

	AUTO_GC_CODE_ADD_FUNCPTR();

	dn = dn_array;
	AUTO_GC_CODE_ADD_DN();

	t   = NULL;
	ret = scf_ast_find_type_type(&t, ast, SCF_VAR_INTPTR);
	assert(0 == ret);
	assert(t);
	v = SCF_VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}
	v->data.i64 = nb_pointers;
	v->const_literal_flag = 1;

	char buf[128];
	snprintf(buf, sizeof(buf) - 1, "%d", nb_pointers);

	if (scf_string_cat_cstr(v->w->text, buf) < 0) {
		scf_variable_free(v);
		scf_3ac_code_free(c);
		return NULL;
	}
	AUTO_GC_CODE_ADD_VAR();

	if (dn_array->var->type >= SCF_STRUCT) {
		t   = NULL;
		ret = scf_ast_find_type_type(&t, ast, dn_array->var->type);
		assert(0 == ret);
		assert(t);

		f = scf_scope_find_function(t->scope, "__release");
	} else
		f = NULL;
	AUTO_GC_CODE_ADD_FUNCPTR();

	return c;
}

static scf_3ac_code_t* _code_alloc_address(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn)
{
	scf_3ac_operand_t*  src;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_variable_t*     v;
	scf_lex_word_t*     w;
	scf_string_t*       s;
	scf_type_t*         t = NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->dsts = scf_vector_alloc();
	if (!c->dsts) {
		scf_3ac_code_free(c);
		return NULL;
	}

	src = scf_3ac_operand_alloc();
	if (!src) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->srcs, src) < 0) {
		scf_3ac_operand_free(src);
		scf_3ac_code_free(c);
		return NULL;
	}
	src->node     = dn->node;
	src->dag_node = dn;

	dst = scf_3ac_operand_alloc();
	if (!dst) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->dsts, dst) < 0) {
		scf_3ac_operand_free(dst);
		scf_3ac_code_free(c);
		return NULL;
	}

	c->op = scf_3ac_find_operator(SCF_OP_ADDRESS_OF);

	w = scf_lex_word_alloc(dn->var->w->file, 0, 0, SCF_LEX_WORD_ID);
	if (!w) {
		scf_3ac_code_free(c);
		return NULL;
	}

	w->text = scf_string_cstr("&");
	if (!w->text) {
		scf_lex_word_free(w);
		scf_3ac_code_free(c);
		return NULL;
	}

	int ret = scf_ast_find_type_type(&t, ast, dn->var->type);
	assert(0 == ret);
	assert(t);

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 0, dn->var->nb_pointers + 1, NULL);
	scf_lex_word_free(w);
	w = NULL;
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node = scf_dag_node_alloc(v->type, v, NULL);
	scf_variable_free(v);
	v = NULL;
	if (!dst->dag_node) {
		scf_3ac_code_free(c);
		return NULL;
	}

	scf_list_add_tail(dag_list_head, &dst->dag_node->list);
	return c;
}

static scf_3ac_code_t* _code_alloc_dereference(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn)
{
	scf_3ac_operand_t*  src;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_variable_t*     v;
	scf_lex_word_t*     w;
	scf_string_t*       s;
	scf_type_t*         t = NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->dsts = scf_vector_alloc();
	if (!c->dsts) {
		scf_3ac_code_free(c);
		return NULL;
	}

	src = scf_3ac_operand_alloc();
	if (!src) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->srcs, src) < 0) {
		scf_3ac_operand_free(src);
		scf_3ac_code_free(c);
		return NULL;
	}
	src->node     = dn->node;
	src->dag_node = dn;

	dst = scf_3ac_operand_alloc();
	if (!dst) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->dsts, dst) < 0) {
		scf_3ac_operand_free(dst);
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_DEREFERENCE);

	w = scf_lex_word_alloc(dn->var->w->file, 0, 0, SCF_LEX_WORD_ID);
	if (!w) {
		scf_3ac_code_free(c);
		return NULL;
	}

	w->text = scf_string_cstr("*");
	if (!w->text) {
		scf_lex_word_free(w);
		scf_3ac_code_free(c);
		return NULL;
	}

	int ret = scf_ast_find_type_type(&t, ast, dn->var->type);
	assert(0 == ret);
	assert(t);

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 0, dn->var->nb_pointers - 1, NULL);
	scf_lex_word_free(w);
	w = NULL;
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node = scf_dag_node_alloc(v->type, v, NULL);
	scf_variable_free(v);
	v = NULL;
	if (!dst->dag_node) {
		scf_3ac_code_free(c);
		return NULL;
	}

	scf_list_add_tail(dag_list_head, &dst->dag_node->list);
	return c;
}

static scf_3ac_code_t* _code_alloc_member(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_base, scf_dag_node_t* dn_member)
{
	scf_3ac_operand_t*  base;
	scf_3ac_operand_t*  member;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_variable_t*     v;
	scf_lex_word_t*     w;
	scf_string_t*       s;
	scf_type_t*         t = NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->dsts = scf_vector_alloc();
	if (!c->dsts) {
		scf_3ac_code_free(c);
		return NULL;
	}

	base = scf_3ac_operand_alloc();
	if (!base) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, base) < 0) {
		scf_3ac_operand_free(base);
		scf_3ac_code_free(c);
		return NULL;
	}
	base->node     = dn_base->node;
	base->dag_node = dn_base;

	member = scf_3ac_operand_alloc();
	if (!member) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, member) < 0) {
		scf_3ac_operand_free(member);
		scf_3ac_code_free(c);
		return NULL;
	}
	member->node     = dn_member->node;
	member->dag_node = dn_member;

	dst = scf_3ac_operand_alloc();
	if (!dst) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->dsts, dst) < 0) {
		scf_3ac_operand_free(dst);
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_POINTER);

	w = scf_lex_word_alloc(dn_base->var->w->file, 0, 0, SCF_LEX_WORD_ID);
	if (!w) {
		scf_3ac_code_free(c);
		return NULL;
	}

	w->text = scf_string_cstr("->");
	if (!w->text) {
		scf_lex_word_free(w);
		scf_3ac_code_free(c);
		return NULL;
	}

	int ret = scf_ast_find_type_type(&t, ast, dn_member->var->type);
	assert(0 == ret);
	assert(t);

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 0, dn_member->var->nb_pointers, NULL);
	scf_lex_word_free(w);
	w = NULL;
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node = scf_dag_node_alloc(v->type, v, NULL);
	scf_variable_free(v);
	v = NULL;
	if (!dst->dag_node) {
		scf_3ac_code_free(c);
		return NULL;
	}

	scf_list_add_tail(dag_list_head, &dst->dag_node->list);
	return c;
}

static scf_3ac_code_t* _code_alloc_member_address(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_base, scf_dag_node_t* dn_member)
{
	scf_3ac_operand_t*  base;
	scf_3ac_operand_t*  member;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_variable_t*     v;
	scf_lex_word_t*     w;
	scf_string_t*       s;
	scf_type_t*         t = NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->dsts = scf_vector_alloc();
	if (!c->dsts) {
		scf_3ac_code_free(c);
		return NULL;
	}

	base = scf_3ac_operand_alloc();
	if (!base) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, base) < 0) {
		scf_3ac_operand_free(base);
		scf_3ac_code_free(c);
		return NULL;
	}
	base->node     = dn_base->node;
	base->dag_node = dn_base;

	member = scf_3ac_operand_alloc();
	if (!member) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, member) < 0) {
		scf_3ac_operand_free(member);
		scf_3ac_code_free(c);
		return NULL;
	}
	member->node     = dn_member->node;
	member->dag_node = dn_member;

	dst = scf_3ac_operand_alloc();
	if (!dst) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->dsts, dst) < 0) {
		scf_3ac_operand_free(dst);
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_3AC_ADDRESS_OF_POINTER);

	w = scf_lex_word_alloc(dn_base->var->w->file, 0, 0, SCF_LEX_WORD_ID);
	if (!w) {
		scf_3ac_code_free(c);
		return NULL;
	}

	w->text = scf_string_cstr("&->");
	if (!w->text) {
		scf_lex_word_free(w);
		scf_3ac_code_free(c);
		return NULL;
	}

	int ret = scf_ast_find_type_type(&t, ast, dn_member->var->type);
	assert(0 == ret);
	assert(t);

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 0, dn_member->var->nb_pointers + 1, NULL);
	scf_lex_word_free(w);
	w = NULL;
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node = scf_dag_node_alloc(v->type, v, NULL);
	scf_variable_free(v);
	v = NULL;
	if (!dst->dag_node) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

	scf_list_add_tail(dag_list_head, &dst->dag_node->list);
	return c;
}

static scf_3ac_code_t* _code_alloc_array_member_address(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_base, scf_dag_node_t* dn_index, scf_dag_node_t* dn_scale)
{
	scf_3ac_operand_t*  base;
	scf_3ac_operand_t*  index;
	scf_3ac_operand_t*  scale;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_variable_t*     v;
	scf_lex_word_t*     w;
	scf_string_t*       s;
	scf_type_t*         t = NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->dsts = scf_vector_alloc();
	if (!c->dsts) {
		scf_3ac_code_free(c);
		return NULL;
	}

	base = scf_3ac_operand_alloc();
	if (!base) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, base) < 0) {
		scf_3ac_operand_free(base);
		scf_3ac_code_free(c);
		return NULL;
	}
	base->node     = dn_base->node;
	base->dag_node = dn_base;

	index = scf_3ac_operand_alloc();
	if (!index) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, index) < 0) {
		scf_3ac_operand_free(index);
		scf_3ac_code_free(c);
		return NULL;
	}
	index->node     = dn_index->node;
	index->dag_node = dn_index;

	scale = scf_3ac_operand_alloc();
	if (!scale) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, scale) < 0) {
		scf_3ac_operand_free(scale);
		scf_3ac_code_free(c);
		return NULL;
	}
	scale->node      = dn_scale->node;
	scale->dag_node  = dn_scale;

	dst = scf_3ac_operand_alloc();
	if (!dst) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->dsts, dst) < 0) {
		scf_3ac_operand_free(dst);
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_3AC_ADDRESS_OF_ARRAY_INDEX);

	w = scf_lex_word_alloc(dn_base->var->w->file, 0, 0, SCF_LEX_WORD_ID);
	if (!w) {
		scf_3ac_code_free(c);
		return NULL;
	}

	w->text = scf_string_cstr("&[]");
	if (!w->text) {
		scf_lex_word_free(w);
		scf_3ac_code_free(c);
		return NULL;
	}

	int ret = scf_ast_find_type_type(&t, ast, dn_base->var->type);
	assert(0 == ret);
	assert(t);

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 0, dn_base->var->nb_pointers, NULL);
	scf_lex_word_free(w);
	w = NULL;
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node = scf_dag_node_alloc(v->type, v, NULL);
	scf_variable_free(v);
	v = NULL;
	if (!dst->dag_node) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

	scf_list_add_tail(dag_list_head, &dst->dag_node->list);
	return c;
}

static scf_3ac_code_t* _code_alloc_array_member(scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_base, scf_dag_node_t* dn_index, scf_dag_node_t* dn_scale)
{
	scf_3ac_operand_t*  base;
	scf_3ac_operand_t*  index;
	scf_3ac_operand_t*  scale;
	scf_3ac_operand_t*  dst;
	scf_3ac_code_t*     c;
	scf_variable_t*     v;
	scf_lex_word_t*     w;
	scf_string_t*       s;
	scf_type_t*         t = NULL;

	c = scf_3ac_code_alloc();
	if (!c)
		return NULL;

	c->srcs = scf_vector_alloc();
	if (!c->srcs) {
		scf_3ac_code_free(c);
		return NULL;
	}

	c->dsts = scf_vector_alloc();
	if (!c->dsts) {
		scf_3ac_code_free(c);
		return NULL;
	}

	base = scf_3ac_operand_alloc();
	if (!base) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, base) < 0) {
		scf_3ac_operand_free(base);
		scf_3ac_code_free(c);
		return NULL;
	}
	base->node     = dn_base->node;
	base->dag_node = dn_base;

	index = scf_3ac_operand_alloc();
	if (!index) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, index) < 0) {
		scf_3ac_operand_free(index);
		scf_3ac_code_free(c);
		return NULL;
	}
	index->node     = dn_index->node;
	index->dag_node = dn_index;

	scale = scf_3ac_operand_alloc();
	if (!scale) {
		scf_3ac_code_free(c);
		return NULL;
	}
	if (scf_vector_add(c->srcs, scale) < 0) {
		scf_3ac_operand_free(scale);
		scf_3ac_code_free(c);
		return NULL;
	}
	scale->node      = dn_scale->node;
	scale->dag_node  = dn_scale;

	dst = scf_3ac_operand_alloc();
	if (!dst) {
		scf_3ac_code_free(c);
		return NULL;
	}

	if (scf_vector_add(c->dsts, dst) < 0) {
		scf_3ac_operand_free(dst);
		scf_3ac_code_free(c);
		return NULL;
	}
	c->op = scf_3ac_find_operator(SCF_OP_ARRAY_INDEX);

	w = scf_lex_word_alloc(dn_base->var->w->file, 0, 0, SCF_LEX_WORD_ID);
	if (!w) {
		scf_3ac_code_free(c);
		return NULL;
	}

	w->text = scf_string_cstr("[]");
	if (!w->text) {
		scf_lex_word_free(w);
		scf_3ac_code_free(c);
		return NULL;
	}

	int ret = scf_ast_find_type_type(&t, ast, dn_base->var->type);
	assert(0 == ret);
	assert(t);

	v = SCF_VAR_ALLOC_BY_TYPE(w, t, 0, dn_base->var->nb_pointers, NULL);
	scf_lex_word_free(w);
	w = NULL;
	if (!v) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node = scf_dag_node_alloc(v->type, v, NULL);
	scf_variable_free(v);
	v = NULL;
	if (!dst->dag_node) {
		scf_3ac_code_free(c);
		return NULL;
	}

	dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

	scf_list_add_tail(dag_list_head, &dst->dag_node->list);
	return c;
}

static int _auto_gc_code_list_ref(scf_list_t* h, scf_list_t* dag_list_head, scf_ast_t* ast, scf_dn_status_t* ds)
{
	scf_dag_node_t*    dn = ds->dag_node;
	scf_3ac_code_t*    c;
	scf_3ac_operand_t* dst;

	if (ds->dn_indexes) {

		int i;
		for (i = ds->dn_indexes->size - 1; i >= 0; i--) {

			scf_dn_index_t* di = ds->dn_indexes->data[i];

			if (di->member) {
				assert(di->dn);

				c = _code_alloc_member(dag_list_head, ast, dn, di->dn);

			} else {
				assert(di->index >= 0 || -1 == di->index);
				assert(di->dn_scale);

				c = _code_alloc_array_member(dag_list_head, ast, dn, di->dn, di->dn_scale);
			}

			scf_list_add_tail(h, &c->list);

			dst = c->dsts->data[0];
			dn  = dst->dag_node;
		}
	}

	c = _auto_gc_code_ref(dag_list_head, ast, dn);

	scf_list_add_tail(h, &c->list);
	return 0;
}

static int _auto_gc_code_list_freep(scf_list_t* h, scf_list_t* dag_list_head, scf_ast_t* ast, scf_dn_status_t* ds)
{
	scf_dag_node_t*    dn = ds->dag_node;
	scf_3ac_code_t*    c;
	scf_3ac_operand_t* dst;

	if (ds->dn_indexes) {

		scf_dn_index_t* di;
		int i;

		for (i = ds->dn_indexes->size - 1; i >= 1; i--) {
			di = ds->dn_indexes->data[i];

			if (di->member) {
				assert(di->dn);

				c = _code_alloc_member(dag_list_head, ast, dn, di->dn);

			} else {
				assert(di->index >= 0);

				assert(0 == di->index);

				c = _code_alloc_dereference(dag_list_head, ast, dn);
			}

			scf_list_add_tail(h, &c->list);

			dst = c->dsts->data[0];
			dn  = dst->dag_node;
		}

		di = ds->dn_indexes->data[0];

		if (di->member) {
			assert(di->dn);

			c = _code_alloc_member_address(dag_list_head, ast, dn, di->dn);

		} else {
			assert(di->index >= 0 || -1 == di->index);
			assert(di->dn_scale);

			c = _code_alloc_array_member_address(dag_list_head, ast, dn, di->dn, di->dn_scale);
		}

		scf_list_add_tail(h, &c->list);

		dst = c->dsts->data[0];
		dn  = dst->dag_node;

	} else {
		c = _code_alloc_address(dag_list_head, ast, dn);

		scf_list_add_tail(h, &c->list);

		dst = c->dsts->data[0];
		dn  = dst->dag_node;
	}

	int nb_pointers = scf_variable_nb_pointers(dn->var);

	assert(nb_pointers >= 2);

	c = _auto_gc_code_freep_array(dag_list_head, ast, dn, nb_pointers - 1);

	scf_list_add_tail(h, &c->list);
	return 0;
}

static int _auto_gc_code_list_free_array(scf_list_t* h, scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_array)
{
	scf_3ac_code_t* c;

	assert(dn_array->var->nb_dimentions > 0);
	assert(dn_array->var->capacity      > 0);

	c = _auto_gc_code_free_array(dag_list_head, ast, dn_array, dn_array->var->capacity, dn_array->var->nb_pointers);

	scf_list_add_tail(h, &c->list);
	return 0;
}

static int _auto_gc_code_list_memset_array(scf_list_t* h, scf_list_t* dag_list_head, scf_ast_t* ast, scf_dag_node_t* dn_array)
{
	scf_3ac_code_t*    c;

	assert(dn_array->var->capacity > 0);

	c = _auto_gc_code_memset_array(dag_list_head, ast, dn_array);

	scf_list_add_tail(h, &c->list);
	return 0;
}

static int _bb_add_gc_code_ref(scf_list_t* dag_list_head, scf_ast_t* ast, scf_basic_block_t* bb, scf_dn_status_t* ds)
{
	scf_list_t     h;
	scf_list_init(&h);

	if (scf_vector_add_unique(bb->entry_dn_actives, ds->dag_node) < 0)
		return -ENOMEM;

	int ret = _auto_gc_code_list_ref(&h, dag_list_head, ast, ds);
	if (ret < 0)
		return ret;

	scf_basic_block_add_code(bb, &h);
	return 0;
}

static int _bb_add_gc_code_freep(scf_list_t* dag_list_head, scf_ast_t* ast, scf_basic_block_t* bb, scf_dn_status_t* ds)
{
	scf_list_t     h;
	scf_list_init(&h);

	if (scf_vector_add_unique(bb->entry_dn_actives, ds->dag_node) < 0)
		return -ENOMEM;

	int ret = _auto_gc_code_list_freep(&h, dag_list_head, ast, ds);
	if (ret < 0)
		return ret;

	scf_basic_block_add_code(bb, &h);
	return 0;
}

static int _bb_add_gc_code_memset_array(scf_list_t* dag_list_head, scf_ast_t* ast, scf_basic_block_t* bb, scf_dag_node_t* dn_array)
{
	scf_list_t     h;
	scf_list_init(&h);

	if (scf_vector_add_unique(bb->entry_dn_actives, dn_array) < 0)
		return -ENOMEM;

	int ret = _auto_gc_code_list_memset_array(&h, dag_list_head, ast, dn_array);
	if (ret < 0)
		return ret;

	scf_basic_block_add_code(bb, &h);
	return 0;
}

static int _bb_add_gc_code_free_array(scf_list_t* dag_list_head, scf_ast_t* ast, scf_basic_block_t* bb, scf_dag_node_t* dn_array)
{
	scf_list_t     h;
	scf_list_init(&h);

	if (scf_vector_add_unique(bb->entry_dn_actives, dn_array) < 0)
		return -ENOMEM;

	int ret = _auto_gc_code_list_free_array(&h, dag_list_head, ast, dn_array);
	if (ret < 0)
		return ret;

	scf_basic_block_add_code(bb, &h);
	return 0;
}
