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

void scf_instruction_print(scf_instruction_t* inst)
{
	if (inst->label)
		printf("%s: ", inst->label->text->data);

	if (inst->OpCode)
		printf("%s ", inst->OpCode->name);

	if (1 == inst->src.flag) {
		if (inst->src.index)
			printf("%d(%s, %s, %d), ", inst->src.disp, inst->src.base->name,
					inst->src.index->name, inst->src.scale);

		else if (inst->src.base) {
			if (inst->src.disp < 0)
				printf("-%#x(%s), ", -inst->src.disp, inst->src.base->name);
			else
				printf("%#x(%s), ", inst->src.disp, inst->src.base->name);
		} else
			printf("%d(rip), ", inst->dst.disp);

	} else if (inst->src.base)
		printf("%s, ", inst->src.base->name);

	else if (inst->src.imm_size > 0)
		printf("%d, ", (int)inst->src.imm);

	if (1 == inst->dst.flag) {
		if (inst->dst.index)
			printf("%d(%s, %s, %d)", inst->dst.disp, inst->dst.base->name,
					inst->dst.index->name, inst->dst.scale);

		else if (inst->dst.base) {
			if (inst->dst.disp < 0)
				printf("-%#x(%s)", -inst->dst.disp, inst->dst.base->name);
			else
				printf("%#x(%s)", inst->dst.disp, inst->dst.base->name);
		} else
			printf("%d(rip)", inst->dst.disp);

	} else if (inst->dst.base)
		printf("%s", inst->dst.base->name);

	else if (inst->dst.imm_size > 0)
		printf("%d", (int)inst->dst.imm);

	int i;
	for (i = 0; i < inst->len; i++)
		printf(" %#x", inst->code[i]);
	printf("\n");
}
