#include"scf_x64.h"

scf_register_t	x64_registers[] = {

	{0, 1, "al",     X64_COLOR(0, 0, 0x1),  NULL, 0},
	{0, 2, "ax",     X64_COLOR(0, 0, 0x3),  NULL, 0},
	{0, 4, "eax",    X64_COLOR(0, 0, 0xf),  NULL, 0},
	{0, 8, "rax",    X64_COLOR(0, 0, 0xff), NULL, 0},

	{1, 1, "cl",     X64_COLOR(0, 1, 0x1),  NULL, 0},
	{1, 2, "cx",     X64_COLOR(0, 1, 0x3),  NULL, 0},
	{1, 4, "ecx",    X64_COLOR(0, 1, 0xf),  NULL, 0},
	{1, 8, "rcx",    X64_COLOR(0, 1, 0xff), NULL, 0},

	{2, 1, "dl",     X64_COLOR(0, 2, 0x1),  NULL, 0},
	{2, 2, "dx",     X64_COLOR(0, 2, 0x3),  NULL, 0},
	{2, 4, "edx",    X64_COLOR(0, 2, 0xf),  NULL, 0},
	{2, 8, "rdx",    X64_COLOR(0, 2, 0xff), NULL, 0},

	{3, 1, "bl",     X64_COLOR(0, 3, 0x1),  NULL, 0},
	{3, 2, "bx",     X64_COLOR(0, 3, 0x3),  NULL, 0},
	{3, 4, "ebx",    X64_COLOR(0, 3, 0xf),  NULL, 0},
	{3, 8, "rbx",    X64_COLOR(0, 3, 0xff), NULL, 0},

	{4, 2, "sp",     X64_COLOR(0, 4, 0x3),  NULL, 0},
	{4, 4, "esp",    X64_COLOR(0, 4, 0xf),  NULL, 0},
	{4, 8, "rsp",    X64_COLOR(0, 4, 0xff), NULL, 0},

	{5, 2, "bp",     X64_COLOR(0, 5, 0x3),  NULL, 0},
	{5, 4, "ebp",    X64_COLOR(0, 5, 0xf),  NULL, 0},
	{5, 8, "rbp",    X64_COLOR(0, 5, 0xff), NULL, 0},

	{6, 1, "sil",    X64_COLOR(0, 6, 0x1),  NULL, 0},
	{6, 2, "si",     X64_COLOR(0, 6, 0x3),  NULL, 0},
	{6, 4, "esi",    X64_COLOR(0, 6, 0xf),  NULL, 0},
	{6, 8, "rsi",    X64_COLOR(0, 6, 0xff), NULL, 0},

	{7, 1, "dil",    X64_COLOR(0, 7, 0x1),  NULL, 0},
	{7, 2, "di",     X64_COLOR(0, 7, 0x3),  NULL, 0},
	{7, 4, "edi",    X64_COLOR(0, 7, 0xf),  NULL, 0},
	{7, 8, "rdi",    X64_COLOR(0, 7, 0xff), NULL, 0},

	{8, 1, "r8b",    X64_COLOR(0, 8,  0x1),  NULL, 0},
	{8, 2, "r8w",    X64_COLOR(0, 8,  0x3),  NULL, 0},
	{8, 4, "r8d",    X64_COLOR(0, 8,  0xf),  NULL, 0},
	{8, 8, "r8",     X64_COLOR(0, 8,  0xff), NULL, 0},

	{9, 1, "r9b",    X64_COLOR(0, 9,  0x1),  NULL, 0},
	{9, 2, "r9w",    X64_COLOR(0, 9,  0x3),  NULL, 0},
	{9, 4, "r9d",    X64_COLOR(0, 9,  0xf),  NULL, 0},
	{9, 8, "r9",     X64_COLOR(0, 9,  0xff), NULL, 0},

	{10, 1, "r10b",  X64_COLOR(0, 10, 0x1),  NULL, 0},
	{10, 2, "r10w",  X64_COLOR(0, 10, 0x3),  NULL, 0},
	{10, 4, "r10d",  X64_COLOR(0, 10, 0xf),  NULL, 0},
	{10, 8, "r10",   X64_COLOR(0, 10, 0xff), NULL, 0},

	{11, 1, "r11b",  X64_COLOR(0, 11, 0x1),  NULL, 0},
	{11, 2, "r11w",  X64_COLOR(0, 11, 0x3),  NULL, 0},
	{11, 4, "r11d",  X64_COLOR(0, 11, 0xf),  NULL, 0},
	{11, 8, "r11",   X64_COLOR(0, 11, 0xff), NULL, 0},

	{12, 1, "r12b",  X64_COLOR(0, 12, 0x1),  NULL, 0},
	{12, 2, "r12w",  X64_COLOR(0, 12, 0x3),  NULL, 0},
	{12, 4, "r12d",  X64_COLOR(0, 12, 0xf),  NULL, 0},
	{12, 8, "r12",   X64_COLOR(0, 12, 0xff), NULL, 0},

	{13, 1, "r13b",  X64_COLOR(0, 13, 0x1),  NULL, 0},
	{13, 2, "r13w",  X64_COLOR(0, 13, 0x3),  NULL, 0},
	{13, 4, "r13d",  X64_COLOR(0, 13, 0xf),  NULL, 0},
	{13, 8, "r13",   X64_COLOR(0, 13, 0xff), NULL, 0},

	{14, 1, "r14b",  X64_COLOR(0, 14, 0x1),  NULL, 0},
	{14, 2, "r14w",  X64_COLOR(0, 14, 0x3),  NULL, 0},
	{14, 4, "r14d",  X64_COLOR(0, 14, 0xf),  NULL, 0},
	{14, 8, "r14",   X64_COLOR(0, 14, 0xff), NULL, 0},

	{15, 1, "r15b",  X64_COLOR(0, 15, 0x1),  NULL, 0},
	{15, 2, "r15w",  X64_COLOR(0, 15, 0x3),  NULL, 0},
	{15, 4, "r15d",  X64_COLOR(0, 15, 0xf),  NULL, 0},
	{15, 8, "r15",   X64_COLOR(0, 15, 0xff), NULL, 0},

	{4, 1, "ah",     X64_COLOR(0, 0, 0x2),  NULL, 0},
	{5, 1, "ch",     X64_COLOR(0, 1, 0x2),  NULL, 0},
	{6, 1, "dh",     X64_COLOR(0, 2, 0x2),  NULL, 0},
	{7, 1, "bh",     X64_COLOR(0, 3, 0x2),  NULL, 0},

	{0, 4, "mm0",    X64_COLOR(1, 0, 0xf),  NULL, 0},
	{0, 8, "xmm0",   X64_COLOR(1, 0, 0xff), NULL, 0},

	{1, 4, "mm1",    X64_COLOR(1, 1, 0xf),  NULL, 0},
	{1, 8, "xmm1",   X64_COLOR(1, 1, 0xff), NULL, 0},

	{2, 4, "mm2",    X64_COLOR(1, 2, 0xf),  NULL, 0},
	{2, 8, "xmm2",   X64_COLOR(1, 2, 0xff), NULL, 0},

	{3, 4, "mm3",    X64_COLOR(1, 3, 0xf),  NULL, 0},
	{3, 8, "xmm3",   X64_COLOR(1, 3, 0xff), NULL, 0},

	{4, 4, "mm4",    X64_COLOR(1, 4, 0xf),  NULL, 0},
	{4, 8, "xmm4",   X64_COLOR(1, 4, 0xff), NULL, 0},

	{5, 4, "mm5",    X64_COLOR(1, 5, 0xf),  NULL, 0},
	{5, 8, "xmm5",   X64_COLOR(1, 5, 0xff), NULL, 0},

	{6, 4, "mm6",    X64_COLOR(1, 6, 0xf),  NULL, 0},
	{6, 8, "xmm6",   X64_COLOR(1, 6, 0xff), NULL, 0},

	{7, 4, "mm7",    X64_COLOR(1, 7, 0xf),  NULL, 0},
	{7, 8, "xmm7",   X64_COLOR(1, 7, 0xff), NULL, 0},


	{0xf, 8, "rip",  X64_COLOR(0, 7, 0xff), NULL, 0},
};

scf_register_t* x64_find_register(const char* name)
{
	int i;
	for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {

		scf_register_t*	r = &(x64_registers[i]);

		if (!strcmp(r->name, name))
			return r;
	}
	return NULL;
}

scf_register_t* x64_find_register_type_id_bytes(uint32_t type, uint32_t id, int bytes)
{
	int i;
	for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {

		scf_register_t*	r = &(x64_registers[i]);

		if (X64_COLOR_TYPE(r->color) == type && r->id == id && r->bytes == bytes)
			return r;
	}
	return NULL;
}

scf_register_t* x64_find_register_color(intptr_t color)
{
	int i;
	for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {

		scf_register_t*	r = &(x64_registers[i]);

		if (r->color == color)
			return r;
	}
	return NULL;
}

scf_register_t* x64_find_register_color_bytes(intptr_t color, int bytes)
{
	int i;
	for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {

		scf_register_t*	r = &(x64_registers[i]);

		if (X64_COLOR_CONFLICT(r->color, color) && r->bytes == bytes)
			return r;
	}
	return NULL;
}
