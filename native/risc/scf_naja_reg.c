#include"scf_risc.h"

#define SCF_RISC_REG_FP   12
#define SCF_RISC_REG_LR   13
#define SCF_RISC_REG_SP   14
#define SCF_RISC_REG_NULL 15

scf_register_t	naja_registers[] =
{
	{0, 1, "r0b",   RISC_COLOR(0, 0, 0x1),  NULL, 0, 0},
	{0, 2, "r0w",   RISC_COLOR(0, 0, 0x3),  NULL, 0, 0},
	{0, 4, "r0d",   RISC_COLOR(0, 0, 0xf),  NULL, 0, 0},
	{0, 8, "r0",    RISC_COLOR(0, 0, 0xff), NULL, 0, 0},

	{1, 1, "r1b",   RISC_COLOR(0, 1, 0x1),  NULL, 0, 0},
	{1, 2, "r1w",   RISC_COLOR(0, 1, 0x3),  NULL, 0, 0},
	{1, 4, "r1d",   RISC_COLOR(0, 1, 0xf),  NULL, 0, 0},
	{1, 8, "r1",    RISC_COLOR(0, 1, 0xff), NULL, 0, 0},

	{2, 1, "r2b",   RISC_COLOR(0, 2, 0x1),  NULL, 0, 0},
	{2, 2, "r2w",   RISC_COLOR(0, 2, 0x3),  NULL, 0, 0},
	{2, 4, "r2d",   RISC_COLOR(0, 2, 0xf),  NULL, 0, 0},
	{2, 8, "r2",    RISC_COLOR(0, 2, 0xff), NULL, 0, 0},

	{3, 1, "r3b",   RISC_COLOR(0, 3, 0x1),  NULL, 0, 0},
	{3, 2, "r3w",   RISC_COLOR(0, 3, 0x3),  NULL, 0, 0},
	{3, 4, "r3d",   RISC_COLOR(0, 3, 0xf),  NULL, 0, 0},
	{3, 8, "r3",    RISC_COLOR(0, 3, 0xff), NULL, 0, 0},

	{4, 1, "r4b",   RISC_COLOR(0, 4, 0x1),  NULL, 0, 0},
	{4, 2, "r4w",   RISC_COLOR(0, 4, 0x3),  NULL, 0, 0},
	{4, 4, "r4d",   RISC_COLOR(0, 4, 0xf),  NULL, 0, 0},
	{4, 8, "r4",    RISC_COLOR(0, 4, 0xff), NULL, 0, 0},

	{5, 1, "r5b",   RISC_COLOR(0, 5, 0x1),  NULL, 0, 0},
	{5, 2, "r5w",   RISC_COLOR(0, 5, 0x3),  NULL, 0, 0},
	{5, 4, "r5d",   RISC_COLOR(0, 5, 0xf),  NULL, 0, 0},
	{5, 8, "r5",    RISC_COLOR(0, 5, 0xff), NULL, 0, 0},

	{6, 1, "r6b",   RISC_COLOR(0, 6, 0x1),  NULL, 0, 0},
	{6, 2, "r6w",   RISC_COLOR(0, 6, 0x3),  NULL, 0, 0},
	{6, 4, "r6d",   RISC_COLOR(0, 6, 0xf),  NULL, 0, 0},
	{6, 8, "r6",    RISC_COLOR(0, 6, 0xff), NULL, 0, 0},

	{7, 1, "r7b",   RISC_COLOR(0, 7, 0x1),  NULL, 0, 0},
	{7, 2, "r7w",   RISC_COLOR(0, 7, 0x3),  NULL, 0, 0},
	{7, 4, "r7d",   RISC_COLOR(0, 7, 0xf),  NULL, 0, 0},
	{7, 8, "r7",    RISC_COLOR(0, 7, 0xff), NULL, 0, 0},

	{8, 1, "r8b",   RISC_COLOR(0, 8, 0x1),  NULL, 0, 0},
	{8, 2, "r8w",   RISC_COLOR(0, 8, 0x3),  NULL, 0, 0},
	{8, 4, "r8d",   RISC_COLOR(0, 8, 0xf),  NULL, 0, 0},
	{8, 8, "r8",    RISC_COLOR(0, 8, 0xff), NULL, 0, 0},

	{9, 1, "r9b",   RISC_COLOR(0, 9, 0x1),  NULL, 0, 0},
	{9, 2, "r9w",   RISC_COLOR(0, 9, 0x3),  NULL, 0, 0},
	{9, 4, "r9d",   RISC_COLOR(0, 9, 0xf),  NULL, 0, 0},
	{9, 8, "r9",    RISC_COLOR(0, 9, 0xff), NULL, 0, 0},

// not use r10, r11
	{10, 1, "r10b", RISC_COLOR(0, 10, 0x1),  NULL, 0, 0},
	{10, 2, "r10w", RISC_COLOR(0, 10, 0x3),  NULL, 0, 0},
	{10, 4, "r10d", RISC_COLOR(0, 10, 0xf),  NULL, 0, 0},
	{10, 8, "r10",  RISC_COLOR(0, 10, 0xff), NULL, 0, 0},

	{11, 1, "r11b", RISC_COLOR(0, 11, 0x1),  NULL, 0, 0},
	{11, 2, "r11w", RISC_COLOR(0, 11, 0x3),  NULL, 0, 0},
	{11, 4, "r11d", RISC_COLOR(0, 11, 0xf),  NULL, 0, 0},
	{11, 8, "r11",  RISC_COLOR(0, 11, 0xff), NULL, 0, 0},

	{12, 1, "r12b", RISC_COLOR(0, 12, 0x1),  NULL, 0, 0},
	{12, 2, "r12w", RISC_COLOR(0, 12, 0x3),  NULL, 0, 0},
	{12, 4, "r12d", RISC_COLOR(0, 12, 0xf),  NULL, 0, 0},
	{12, 8, "fp",   RISC_COLOR(0, 12, 0xff), NULL, 0, 0},

	{13, 1, "r13b", RISC_COLOR(0, 13, 0x1),  NULL, 0, 0},
	{13, 2, "r13w", RISC_COLOR(0, 13, 0x3),  NULL, 0, 0},
	{13, 4, "r13d", RISC_COLOR(0, 13, 0xf),  NULL, 0, 0},
	{13, 8, "lr",   RISC_COLOR(0, 13, 0xff), NULL, 0, 0},

	{14, 8, "sp",   RISC_COLOR(0, 14, 0xff), NULL, 0, 0},
//	{15, 8, "null", RISC_COLOR(0, 15, 0xff), NULL, 0, 0},


	{0, 1, "b0",    RISC_COLOR(1, 0, 0x1),  NULL, 0, 0},
	{0, 2, "h0",    RISC_COLOR(1, 0, 0x3),  NULL, 0, 0},
	{0, 4, "s0",    RISC_COLOR(1, 0, 0xf),  NULL, 0, 0},
	{0, 8, "d0",    RISC_COLOR(1, 0, 0xff), NULL, 0, 0},

	{1, 1, "b1",    RISC_COLOR(1, 1, 0x1),  NULL, 0, 0},
	{1, 2, "h1",    RISC_COLOR(1, 1, 0x3),  NULL, 0, 0},
	{1, 4, "s1",    RISC_COLOR(1, 1, 0xf),  NULL, 0, 0},
	{1, 8, "d1",    RISC_COLOR(1, 1, 0xff), NULL, 0, 0},

	{2, 1, "b2",    RISC_COLOR(1, 2, 0x1),  NULL, 0, 0},
	{2, 2, "h2",    RISC_COLOR(1, 2, 0x3),  NULL, 0, 0},
	{2, 4, "s2",    RISC_COLOR(1, 2, 0xf),  NULL, 0, 0},
	{2, 8, "d2",    RISC_COLOR(1, 2, 0xff), NULL, 0, 0},

	{3, 1, "b3",    RISC_COLOR(1, 3, 0x1),  NULL, 0, 0},
	{3, 2, "h3",    RISC_COLOR(1, 3, 0x3),  NULL, 0, 0},
	{3, 4, "s3",    RISC_COLOR(1, 3, 0xf),  NULL, 0, 0},
	{3, 8, "d3",    RISC_COLOR(1, 3, 0xff), NULL, 0, 0},

	{4, 1, "b4",    RISC_COLOR(1, 4, 0x1),  NULL, 0, 0},
	{4, 2, "h4",    RISC_COLOR(1, 4, 0x3),  NULL, 0, 0},
	{4, 4, "s4",    RISC_COLOR(1, 4, 0xf),  NULL, 0, 0},
	{4, 8, "d4",    RISC_COLOR(1, 4, 0xff), NULL, 0, 0},

	{5, 1, "b5",    RISC_COLOR(1, 5, 0x1),  NULL, 0, 0},
	{5, 2, "h5",    RISC_COLOR(1, 5, 0x3),  NULL, 0, 0},
	{5, 4, "s5",    RISC_COLOR(1, 5, 0xf),  NULL, 0, 0},
	{5, 8, "d5",    RISC_COLOR(1, 5, 0xff), NULL, 0, 0},

	{6, 1, "b6",    RISC_COLOR(1, 6, 0x1),  NULL, 0, 0},
	{6, 2, "h6",    RISC_COLOR(1, 6, 0x3),  NULL, 0, 0},
	{6, 4, "s6",    RISC_COLOR(1, 6, 0xf),  NULL, 0, 0},
	{6, 8, "d6",    RISC_COLOR(1, 6, 0xff), NULL, 0, 0},

	{7, 1, "b7",    RISC_COLOR(1, 7, 0x1),  NULL, 0, 0},
	{7, 2, "h7",    RISC_COLOR(1, 7, 0x3),  NULL, 0, 0},
	{7, 4, "s7",    RISC_COLOR(1, 7, 0xf),  NULL, 0, 0},
	{7, 8, "d7",    RISC_COLOR(1, 7, 0xff), NULL, 0, 0},

	{8, 1, "b8",    RISC_COLOR(1, 8, 0x1),  NULL, 0, 0},
	{8, 2, "h8",    RISC_COLOR(1, 8, 0x3),  NULL, 0, 0},
	{8, 4, "s8",    RISC_COLOR(1, 8, 0xf),  NULL, 0, 0},
	{8, 8, "d8",    RISC_COLOR(1, 8, 0xff), NULL, 0, 0},

	{9, 1, "b9",    RISC_COLOR(1, 9, 0x1),  NULL, 0, 0},
	{9, 2, "h9",    RISC_COLOR(1, 9, 0x3),  NULL, 0, 0},
	{9, 4, "s9",    RISC_COLOR(1, 9, 0xf),  NULL, 0, 0},
	{9, 8, "d9",    RISC_COLOR(1, 9, 0xff), NULL, 0, 0},

	{10, 1, "b10",    RISC_COLOR(1, 10, 0x1),  NULL, 0, 0},
	{10, 2, "h10",    RISC_COLOR(1, 10, 0x3),  NULL, 0, 0},
	{10, 4, "s10",    RISC_COLOR(1, 10, 0xf),  NULL, 0, 0},
	{10, 8, "d10",    RISC_COLOR(1, 10, 0xff), NULL, 0, 0},

	{11, 1, "b11",    RISC_COLOR(1, 11, 0x1),  NULL, 0, 0},
	{11, 2, "h11",    RISC_COLOR(1, 11, 0x3),  NULL, 0, 0},
	{11, 4, "s11",    RISC_COLOR(1, 11, 0xf),  NULL, 0, 0},
	{11, 8, "d11",    RISC_COLOR(1, 11, 0xff), NULL, 0, 0},

	{12, 1, "b12",    RISC_COLOR(1, 12, 0x1),  NULL, 0, 0},
	{12, 2, "h12",    RISC_COLOR(1, 12, 0x3),  NULL, 0, 0},
	{12, 4, "s12",    RISC_COLOR(1, 12, 0xf),  NULL, 0, 0},
	{12, 8, "d12",    RISC_COLOR(1, 12, 0xff), NULL, 0, 0},

	{13, 1, "b13",    RISC_COLOR(1, 13, 0x1),  NULL, 0, 0},
	{13, 2, "h13",    RISC_COLOR(1, 13, 0x3),  NULL, 0, 0},
	{13, 4, "s13",    RISC_COLOR(1, 13, 0xf),  NULL, 0, 0},
	{13, 8, "d13",    RISC_COLOR(1, 13, 0xff), NULL, 0, 0},

	{14, 1, "b14",    RISC_COLOR(1, 14, 0x1),  NULL, 0, 0},
	{14, 2, "h14",    RISC_COLOR(1, 14, 0x3),  NULL, 0, 0},
	{14, 4, "s14",    RISC_COLOR(1, 14, 0xf),  NULL, 0, 0},
	{14, 8, "d14",    RISC_COLOR(1, 14, 0xff), NULL, 0, 0},

	{15, 1, "b15",    RISC_COLOR(1, 15, 0x1),  NULL, 0, 0},
	{15, 2, "h15",    RISC_COLOR(1, 15, 0x3),  NULL, 0, 0},
	{15, 4, "s15",    RISC_COLOR(1, 15, 0xf),  NULL, 0, 0},
	{15, 8, "d15",    RISC_COLOR(1, 15, 0xff), NULL, 0, 0},
};

static uint32_t naja_abi_regs[] =
{
	SCF_RISC_REG_X0,
	SCF_RISC_REG_X1,
	SCF_RISC_REG_X2,
	SCF_RISC_REG_X3,
	SCF_RISC_REG_X4,
	SCF_RISC_REG_X5,
};

static uint32_t naja_abi_float_regs[] =
{
	SCF_RISC_REG_D0,
	SCF_RISC_REG_D1,
	SCF_RISC_REG_D2,
	SCF_RISC_REG_D3,
	SCF_RISC_REG_D4,
	SCF_RISC_REG_D5,
	SCF_RISC_REG_D6,
	SCF_RISC_REG_D7,
};

static uint32_t naja_abi_ret_regs[] =
{
	SCF_RISC_REG_X0,
	SCF_RISC_REG_X1,
	SCF_RISC_REG_X2,
	SCF_RISC_REG_X3,
};

static uint32_t naja_abi_caller_saves[] =
{
	SCF_RISC_REG_X0,
	SCF_RISC_REG_X1,
	SCF_RISC_REG_X2,
	SCF_RISC_REG_X3,
	SCF_RISC_REG_X4,
	SCF_RISC_REG_X5,
};

static uint32_t naja_abi_callee_saves[] =
{
	SCF_RISC_REG_X6,
	SCF_RISC_REG_X7,
	SCF_RISC_REG_X8,
	SCF_RISC_REG_X9,
	SCF_RISC_REG_FP,
	SCF_RISC_REG_LR,
};

static int naja_color_conflict(intptr_t c0, intptr_t c1)
{
	intptr_t id0 = c0 >> 16;
	intptr_t id1 = c1 >> 16;

	return id0 == id1 && (c0 & c1 & 0xffff);
}

static int naja_variable_size(scf_variable_t* v)
{
	if (v->nb_dimentions > 0)
		return 8;

	if (v->type >= SCF_STRUCT && 0 == v->nb_pointers)
		return 8;

	return v->size;
}

scf_register_t* naja_find_register(const char* name)
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (!strcmp(r->name, name))
			return r;
	}
	return NULL;
}

scf_register_t* naja_find_register_type_id_bytes(uint32_t type, uint32_t id, int bytes)
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (RISC_COLOR_TYPE(r->color) == type && r->id == id && r->bytes == bytes)
			return r;
	}
	return NULL;
}

scf_register_t* naja_find_register_color(intptr_t color)
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (r->color == color)
			return r;
	}
	return NULL;
}

scf_register_t* naja_find_register_color_bytes(intptr_t color, int bytes)
{
	int i;
	for (i = 0; i < sizeof(naja_registers) / sizeof(naja_registers[0]); i++) {

		scf_register_t*	r = &(naja_registers[i]);

		if (naja_color_conflict(r->color, color) && r->bytes == bytes)
			return r;
	}
	return NULL;
}
