#include"scf_instruction.h"

void scf_rela_free(scf_rela_t* rela)
{
	if (rela) {
		if (rela->name)
			scf_string_free(rela->name);

		free(rela);
	}
}

void scf_instruction_free(scf_instruction_t* inst)
{
	if (inst) {
		if (inst->label)
			scf_lex_word_free(inst->label);

		if (inst->bin)
			scf_string_free(inst->bin);

		free(inst);
	}
}

void scf_inst_data_print(scf_inst_data_t* id)
{
	if (1 == id->flag) {
		if (id->index)
			printf("%d(%s, %s, %d)", id->disp, id->base->name, id->index->name, id->scale);

		else if (id->base) {
			if (id->disp < 0)
				printf("-%#x(%s)", -id->disp, id->base->name);
			else
				printf("%#x(%s)", id->disp, id->base->name);
		} else
			printf("%d(rip)", id->disp);

	} else if (id->base)
		printf("%s", id->base->name);
	else if (id->imm_size > 0)
		printf("%d", (int)id->imm);
}

void scf_instruction_print(scf_instruction_t* inst)
{
	if (inst->label)
		printf("%s: ", inst->label->text->data);

	if (inst->OpCode)
		printf("%s ", inst->OpCode->name);

	scf_inst_data_print(&inst->dst);

	if (!scf_inst_data_empty(&inst->dst) && !scf_inst_data_empty(&inst->src))
		printf(", ");
	scf_inst_data_print(&inst->src);

	if (!scf_inst_data_empty(&inst->srcs[0]))
		printf(", ");
	scf_inst_data_print(&inst->srcs[0]);

	if (!scf_inst_data_empty(&inst->srcs[1]))
		printf(", ");
	scf_inst_data_print(&inst->srcs[1]);

	printf(" |");
	for (int i = 0; i < inst->len; i++)
		printf(" %#x", inst->code[i]);

	printf("\n");
}
