#include"scf_operator_handler.h"

scf_operator_handler_t* scf_operator_handler_alloc(int type, scf_operator_handler_pt func)
{
	scf_operator_handler_t* h = calloc(1, sizeof(scf_operator_handler_t));
	assert(h);

	scf_list_init(&h->list);

	h->type			= type;

	h->func			= func;
	return h;
}

void scf_operator_handler_free(scf_operator_handler_t* h)
{
	if (h) {
		free(h);
		h = NULL;
	}
}

