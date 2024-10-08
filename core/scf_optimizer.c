#include"scf_optimizer.h"

extern scf_optimizer_t   scf_optimizer_inline;

extern scf_optimizer_t   scf_optimizer_dag;

extern scf_optimizer_t   scf_optimizer_call;
extern scf_optimizer_t   scf_optimizer_common_expr;

extern scf_optimizer_t   scf_optimizer_pointer_alias;
extern scf_optimizer_t   scf_optimizer_active_vars;

extern scf_optimizer_t   scf_optimizer_pointer_aliases;
extern scf_optimizer_t   scf_optimizer_loads_saves;

extern scf_optimizer_t   scf_optimizer_dominators;

extern scf_optimizer_t   scf_optimizer_auto_gc_find;
extern scf_optimizer_t   scf_optimizer_auto_gc;

extern scf_optimizer_t   scf_optimizer_basic_block;

extern scf_optimizer_t   scf_optimizer_const_teq;

extern scf_optimizer_t   scf_optimizer_loop;
extern scf_optimizer_t   scf_optimizer_group;
extern scf_optimizer_t   scf_optimizer_generate_loads_saves;

static scf_optimizer_t*  scf_optimizers[] =
{
	&scf_optimizer_inline, // global optimizer

	&scf_optimizer_dag,

	&scf_optimizer_call,
	&scf_optimizer_common_expr,

	&scf_optimizer_pointer_alias,
	&scf_optimizer_active_vars,

	&scf_optimizer_pointer_aliases,
	&scf_optimizer_loads_saves,

	&scf_optimizer_auto_gc_find, // global optimizer

	&scf_optimizer_dominators,

	&scf_optimizer_auto_gc,

	&scf_optimizer_basic_block,
	&scf_optimizer_const_teq,

	&scf_optimizer_dominators,
	&scf_optimizer_loop,
	&scf_optimizer_group,

	&scf_optimizer_generate_loads_saves,
};

int scf_optimize(scf_ast_t* ast, scf_vector_t* functions)
{
	scf_optimizer_t* opt;
	scf_function_t*  f;
	scf_bb_group_t*  bbg;

	int n = sizeof(scf_optimizers)  / sizeof(scf_optimizers[0]);
	int i;
	int j;

	for (i  = 0; i < n; i++) {
		opt = scf_optimizers[i];

		if (SCF_OPTIMIZER_GLOBAL == opt->flags) {

			int ret = opt->optimize(ast, NULL, functions);
			if (ret < 0) {
				scf_loge("optimizer: %s\n", opt->name);
				return ret;
			}
			continue;
		}

		for (j = 0; j < functions->size; j++) {
			f  =        functions->data[j];

			if (!f->node.define_flag)
				continue;

			int ret = opt->optimize(ast, f, NULL);
			if (ret < 0) {
				scf_loge("optimizer: %s\n", opt->name);
				return ret;
			}
		}
	}

#if 1
	for (i = 0; i < functions->size; i++) {
		f  =        functions->data[i];

		if (!f->node.define_flag)
			continue;

		printf("\n");
		scf_logi("------- %s() ------\n", f->node.w->text->data);

		scf_basic_block_print_list(&f->basic_block_list_head);

		for (j = 0; j < f->bb_groups->size; j++) {
			bbg       = f->bb_groups->data[j];

			switch (bbg->loop_layers) {
				case 0:
					scf_bb_group_print(bbg);
					break;
				default:
					scf_bb_loop_print(bbg);
					break;
			};
		}
	}
#endif
	return 0;
}
