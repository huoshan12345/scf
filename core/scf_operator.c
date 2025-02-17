#include"scf_operator.h"
#include"scf_core_types.h"

static scf_operator_t	base_operators[] =
{
	{"(",         NULL,   SCF_OP_EXPR,          0,  1,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"(",         NULL,   SCF_OP_CALL,          1, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"[",         "i",    SCF_OP_ARRAY_INDEX,   1,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"->",        "p",    SCF_OP_POINTER,       1,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"va_start",  NULL,   SCF_OP_VA_START,      1,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"va_arg",    NULL,   SCF_OP_VA_ARG,        1,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"va_end",    NULL,   SCF_OP_VA_END,        1,  1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"container", NULL,   SCF_OP_CONTAINER,     1,  3,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"create",    NULL,   SCF_OP_CREATE,        2, -1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"(",         NULL,   SCF_OP_TYPE_CAST,     2,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"!",         "not",  SCF_OP_LOGIC_NOT,     2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"~",         "bnot", SCF_OP_BIT_NOT,       2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"-",         "neg",  SCF_OP_NEG,           2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"+",         "pos",  SCF_OP_POSITIVE,      2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"sizeof",    NULL,   SCF_OP_SIZEOF,        2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},

	{"++",        "inc",  SCF_OP_INC,           2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"--",        "dec",  SCF_OP_DEC,           2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},

	{"++",        "inc",  SCF_OP_INC_POST,      2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"--",        "dec",  SCF_OP_DEC_POST,      2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},

	{"*",         NULL,   SCF_OP_DEREFERENCE,   2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"&",         NULL,   SCF_OP_ADDRESS_OF,    2,  1,  SCF_OP_ASSOCIATIVITY_RIGHT},

	{"*",         "mul",  SCF_OP_MUL,           4,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"/",         "div",  SCF_OP_DIV,           4,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"%",         "mod",  SCF_OP_MOD,           4,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"+",         "add",  SCF_OP_ADD,           5,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"-",         "sub",  SCF_OP_SUB,           5,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"<<",         NULL,  SCF_OP_SHL,           6,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{">>",         NULL,  SCF_OP_SHR,           6,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"&",         "band", SCF_OP_BIT_AND,       7,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"|",         "bor",  SCF_OP_BIT_OR,        7,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"==",        "eq",   SCF_OP_EQ,            8,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"!=",        "ne",   SCF_OP_NE,            8,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{">",         "gt",   SCF_OP_GT,            8,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"<",         "lt",   SCF_OP_LT,            8,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{">=",        "ge",   SCF_OP_GE,            8,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"<=",        "le",   SCF_OP_LE,            8,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"&&",        NULL,   SCF_OP_LOGIC_AND,     9,  2,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"||",        NULL,   SCF_OP_LOGIC_OR,      9,  2,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"=",         "a",    SCF_OP_ASSIGN,       10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"+=",        "add_", SCF_OP_ADD_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"-=",        "sub_", SCF_OP_SUB_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"*=",        "mul_", SCF_OP_MUL_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"/=",        "div_", SCF_OP_DIV_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"%=",        "mod_", SCF_OP_MOD_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"<<=",        NULL,  SCF_OP_SHL_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{">>=",        NULL,  SCF_OP_SHR_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"&=",        "and_", SCF_OP_AND_ASSIGN,   10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},
	{"|=",        "or_",  SCF_OP_OR_ASSIGN,    10,  2,  SCF_OP_ASSOCIATIVITY_RIGHT},

	{"{}",         NULL,  SCF_OP_BLOCK,        15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"return",     NULL,  SCF_OP_RETURN,       15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"break",      NULL,  SCF_OP_BREAK,        15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"continue",   NULL,  SCF_OP_CONTINUE,     15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"goto",       NULL,  SCF_OP_GOTO,         15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"label",      NULL,  SCF_LABEL,           15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"if",         NULL,  SCF_OP_IF,           15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"while",      NULL,  SCF_OP_WHILE,        15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"do",         NULL,  SCF_OP_DO,           15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"for",        NULL,  SCF_OP_FOR,          15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"switch",     NULL,  SCF_OP_SWITCH,       15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"case",       NULL,  SCF_OP_CASE,         15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"default",    NULL,  SCF_OP_DEFAULT,      15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},

	{"vla_alloc",  NULL,  SCF_OP_VLA_ALLOC,    15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
	{"vla_free",   NULL,  SCF_OP_VLA_FREE,     15, -1,  SCF_OP_ASSOCIATIVITY_LEFT},
};

scf_operator_t* scf_find_base_operator(const char* name, const int nb_operands)
{
	int i;
	for (i = 0; i < sizeof(base_operators) / sizeof(base_operators[0]); i++) {

		scf_operator_t* op = &base_operators[i];

		if (nb_operands == op->nb_operands && !strcmp(name, op->name))
			return op;
	}

	return NULL;
}

scf_operator_t* scf_find_base_operator_by_type(const int type)
{
	int i;
	for (i = 0; i < sizeof(base_operators) / sizeof(base_operators[0]); i++) {

		scf_operator_t* op = &base_operators[i];

		if (type == op->type)
			return op;
	}

	return NULL;
}
