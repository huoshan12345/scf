#include"scf_vm.h"

static char*  __naja_sign[]  = {"", "s"};
static char   __naja_size[]  = {'b', 'w', 'l', 'q'};
static char   __naja_float[] = {'B', 'H', 'S', 'D'};

static char* __naja_SH[] = {
	"LSL",
	"LSR",
	"ASR",
	"IMM"
};

static char* __naja_reg[] =
{
	"r0",  "r1",  "r2", "r3", "r4",
	"r5",  "r6",  "r7", "r8", "r9",
	"r10", "r11", "fp", "lr", "sp",
	"null"
};

static int __naja_add(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int SH   = (inst >> 16) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm12 = (inst >> 4) & 0xfff;
		printf("add%c     %s, %s, %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;

		printf("add%c     %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
	}

	return 0;
}

static int __naja_sub(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int SH   = (inst >> 16) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm12 = (inst >> 4) & 0xfff;

		if (0xf == rd)
			printf("cmp%c     %s, %lu\n", __naja_size[size], __naja_reg[rs0], uimm12);
		else
			printf("sub%c     %s, %s, %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;

		if (0xf == rd)
			printf("cmp%c     %s, %s %s %lu\n", __naja_size[size], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
		else
			printf("sub%c     %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
	}

	return 0;
}

static int __naja_mul(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rs2  = (inst >> 12) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int s    = (inst >> 17) & 0x1;
	int A    = (inst >> 16) & 0x1;

	if (0xf == rs2)
		printf("%smul%c     %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1]);
	else {
		if (0 == A)
			printf("%smadd%c    %s, %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs2], __naja_reg[rs0], __naja_reg[rs1]);
		else
			printf("%smsub%c    %s, %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs2], __naja_reg[rs0], __naja_reg[rs1]);
	}

	return 0;
}

static int __naja_div(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rs2  = (inst >> 12) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int s    = (inst >> 17) & 0x1;
	int A    = (inst >> 16) & 0x1;

	printf("%sdiv%c     %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1]);
	return 0;
}

static int __naja_ldr_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int s    = (inst >> 17) & 0x1;
	int s13  = (inst >>  4) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	printf("ldr%s%c  %s, [%s, %d]\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rb], s13 << size);
	return 0;
}

static int __naja_and(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int SH   = (inst >> 16) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm12 = (inst >> 4) & 0xfff;

		if (0xf == rd)
			printf("teq%c   %s, %#lx\n", __naja_size[size], __naja_reg[rs0], uimm12);
		else
			printf("and%c   %s, %s, %#lx\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;

		if (0xf == rd)
			printf("teq%c   %s, %s %s %lu\n", __naja_size[size], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
		else
			printf("and%c   %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
	}

	return 0;
}

static int __naja_str_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb   =  inst        & 0xf;
	int rs   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int s13  = (inst >>  4) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	printf("str%c    %s, [%s, %d]\n", __naja_size[size], __naja_reg[rs], __naja_reg[rb], s13 << size);
	return 0;
}

static int __naja_or(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int SH   = (inst >> 16) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm12 = (inst >> 4) & 0xfff;

		printf("or%c    %s, %s, %#lx\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;

		printf("or%c    %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
	}

	return 0;
}

static int __naja_pop(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int rd = (inst >> 21) & 0xf;
	int s  = (inst >> 17) & 0x1;

	if (s)
		printf("fpop     d%d, [%s]\n", rd, __naja_reg[rb]);
	else
		printf("pop      %s, [%s]\n", __naja_reg[rd], __naja_reg[rb]);
	return 0;
}

static int __naja_push(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int rs = (inst >> 21) & 0xf;
	int s  = (inst >> 17) & 0x1;

	if (s)
		printf("fpush    d%d, [%s]\n", rs, __naja_reg[rb]);
	else
		printf("push     %s, [%s]\n", __naja_reg[rs], __naja_reg[rb]);
	return 0;
}

static int __naja_ldr_sib(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb    =  inst        & 0xf;
	int ri    = (inst >>  4) & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int s     = (inst >> 17) & 0x1;
	int uimm6 = (inst >> 10) & 0x3f;

	printf("ldr%s%c   %s, [%s, %s LSL %d]\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rb], __naja_reg[ri], uimm6);
	return 0;
}

static int __naja_str_sib(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb    =  inst        & 0xf;
	int ri    = (inst >>  4) & 0xf;
	int rs    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int s     = (inst >> 17) & 0x1;
	int uimm6 = (inst >> 10) & 0x3f;

	if (s)
		printf("fstr.%d   d%d, [%s, %s, %d]\n", 1 << size, rs, __naja_reg[rb], __naja_reg[ri], uimm6);
	else
		printf("str%c     %s, [%s, %s, %d]\n", __naja_size[size], __naja_reg[rs], __naja_reg[rb], __naja_reg[ri], uimm6);
	return 0;
}

static int __naja_mov(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs    =  inst        & 0xf;
	int rs1   = (inst >>  4) & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int SH    = (inst >> 16) & 0x3;
	int uimm6 = (inst >> 10) & 0x3f;

	if (0xf == rs1) {
		if (0 == uimm6)
			printf("mov%c     %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
		else
			printf("mov%c     %s, %s %s %d\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs], __naja_SH[SH], uimm6);
	} else
		printf("mov%c     %s, %s %s %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs], __naja_SH[SH], __naja_reg[rs1]);
	return 0;
}

static int __naja_movx(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs    =  inst        & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int s     = (inst >> 17) & 0x1;

	if (s)
		printf("movs%cq    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
	else
		printf("movz%cq    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
	return 0;
}

static int __naja_not(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int SH   = (inst >> 16) & 0x3;
	int u16  =  inst        & 0xffff;

	switch (SH) {
		case 0:
			printf("neg%c    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
			break;
		case 1:
			printf("not%c    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
			break;
		case 2:
			if (0 == size)
				printf("mov      %s, %d\n", __naja_reg[rd], u16);
			else
				printf("movt     %s, %d LSL %d\n", __naja_reg[rd], u16, 16 << size);
			break;
		default:
			printf("mvn%c    %s, %d\n", __naja_size[size], __naja_reg[rd], u16);
			break;
	};
	return 0;
}

static int __naja_fadd(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	if (V) {
		int N = 1 << (3 + vlen - size);

		printf("vadd.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	} else
		printf("fadd.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);

	return 0;
}

static int __naja_fsub(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int cmp  = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	if (V) {
		int N = 1 << (3 + vlen - size);

		if (cmp)
			printf("vcmp.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
		else
			printf("vsub.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	} else {
		if (cmp)
			printf("fcmp.%d   d%d, d%d\n", 1 << size, rs0, rs1);
		else
			printf("fsub.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);
	}
	return 0;
}

static int __naja_fmul(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rs2  = (inst >> 12) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int A    = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	if (V) {
		int N = 1 << (3 + vlen - size);

		if (0xf == rs2)
			printf("vmul.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
		else {
			if (A)
				printf("vmsub.f%dx%d   X%d, X%d, X%d, X%d\n", 1 << size, N, rd, rs2, rs0, rs1);
			else
				printf("vmadd.f%dx%d   X%d, X%d, X%d, X%d\n", 1 << size, N, rd, rs2, rs0, rs1);
		}
	} else {
		if (0xf == rs2)
			printf("fmul.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);
		else {
			if (A)
				printf("fmsub.%d   d%d, d%d, d%d, d%d\n", 1 << size, rd, rs2, rs0, rs1);
			else
				printf("fmadd.%d   d%d, d%d, d%d, d%d\n", 1 << size, rd, rs2, rs0, rs1);
		}
	}
	return 0;
}

static int __naja_fdiv(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	if (V) {
		int N = 1 << (3 + vlen - size);

		printf("vdiv.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	} else
		printf("fdiv.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);
	return 0;
}

static int __naja_fldr_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int A    = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;
	int s13  = (inst >>  4) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	if (V) {
		int N = 1 << (3 + vlen - size);

		if (A)
			printf("vldr.f%dx%d  X%d, [%s]!\n", 1 << size, N, rd, __naja_reg[rb]);
		else
			printf("vldr.f%dx%d  X%d, [%s]\n", 1 << size, N, rd, __naja_reg[rb]);
	} else
		printf("fldr.%d  d%d, [%s, %d]\n", 1 << size, rd, __naja_reg[rb], s13 << size);
	return 0;
}

static int __naja_fstr_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int A    = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;
	int s13  = (inst >>  4) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	if (V) {
		int N = 1 << (3 + vlen - size);

		if (A)
			printf("vstr.f%dx%d  X%d, [%s]!\n", 1 << size, N, rd, __naja_reg[rb]);
		else
			printf("vstr.f%dx%d  X%d, [%s]\n", 1 << size, N, rd, __naja_reg[rb]);
	} else
		printf("fstr.%d  d%d, [%s, %d]\n", 1 << size, rd, __naja_reg[rb], s13 << size);
	return 0;
}

static int __naja_fldr_sib(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb    =  inst        & 0xf;
	int ri    = (inst >>  4) & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int uimm6 = (inst >> 10) & 0x3f;

	printf("fldr.%d   d%d, [%s, %s, %d]\n", 1 << size, rd, __naja_reg[rb], __naja_reg[ri], uimm6);
	return 0;
}

static int __naja_fmov(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	if (V) {
		int N = 1 << (3 + vlen - size);

		printf("vmov.f%dx%d  X%d, X%d\n", 1 << size, N, rd, rs);
	} else
		printf("fmov.%d      d%d, d%d\n", 1 << size, rd, rs);
	return 0;
}

static int __naja_fcvt(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs    =  inst        & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int dsize = (inst >> 18) & 0x3;
	int V     = (inst >> 17) & 0x1;
	int Id    = (inst >> 13) & 0x1;
	int sd    = (inst >> 12) & 0x1;
	int vlen  = (inst >> 10) & 0x3;
	int Is    = (inst >>  7) & 0x1;
	int ss    = (inst >>  6) & 0x1;
	int ssize = (inst >>  4) & 0x3;

	static char IS_array[2][2] = {
		'F', 'F',
		'U', 'I'
	};

	static char R_array[2] = {'d', 'r'};

	if (V) {
		int N = 1 << (3 + vlen - dsize);

		printf("vcvt%c%dto%c%dx%d  X%d, X%d\n",
				IS_array[Is][ss], 1 << ssize,
				IS_array[Id][sd], 1 << dsize,
				N, rd, rs);
	} else {
		printf("fcvt%c%dto%c%d      %c%d, %c%d\n",
				IS_array[Is][ss], 1 << ssize,
				IS_array[Id][sd], 1 << dsize,
				R_array[Id], rd, R_array[Is], rs);
	}
	return 0;
}

static int __naja_fneg(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int V    = (inst >> 17) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	if (V) {
		int N = 1 << (3 + vlen - size);

		printf("vneg.f%dx%d  X%d, X%d\n", 1 << size, N, rd, rs);
	} else
		printf("fneg.%d      d%d, d%d\n", 1 << size, rd, rs);
	return 0;
}

static int __naja_vadd(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	printf("vadd.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	return 0;
}

static int __naja_vsub(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int cmp  = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	if (cmp)
		printf("vcmp.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	else
		printf("vsub.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	return 0;
}

static int __naja_vmul(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rs2  = (inst >> 12) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int s    = (inst >> 17) & 0x1;
	int A    = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	static char  s_array[2] = {'u',    'i'};
	static char* A_array[2] = {"madd", "msub"};

	if (0xf == rs2)
		printf("vmul.%c%dx%d  X%d, X%d, X%d\n", s_array[s], 1 << size, N, rd, rs0, rs1);
	else
		printf("v%s.%c%dx%d  X%d, X%d, X%d, X%d\n", A_array[A], s_array[s], 1 << size, N, rd, rs2, rs0, rs1);
	return 0;
}

static int __naja_vdiv(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int s    = (inst >> 17) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	if (s)
		printf("vdiv.i%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	else
		printf("vdiv.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	return 0;
}

static int __naja_vand(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int teq  = (inst >> 16) & 0x1;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	if (teq)
		printf("vteq.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	else
		printf("vand.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	return 0;
}

static int __naja_vor(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rs1  = (inst >>  4) & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	printf("vor.u%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	return 0;
}

static int __naja_vmov(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs    =  inst        & 0xf;
	int rs1   = (inst >>  4) & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int SH    = (inst >> 16) & 0x3;
	int IMM   = (inst >> 15) & 0x3;
	int vlen  = (inst >> 10) & 0x3;
	int uimm6 = (inst >>  4) & 0x3f;

	int N = 1 << (3 + vlen - size);

	if (IMM) {
		if (0 == uimm6)
			printf("vmov.u%dx%d   X%d, X%d\n", 1 << size, N, rd, rs);
		else
			printf("vmov.u%dx%d   X%d, X%d %s %d\n", 1 << size, N, rd, rs, __naja_SH[SH], uimm6);
	} else
		printf("vmov.u%dx%d   X%d, X%d %s %s\n", 1 << size, N, rd, rs, __naja_SH[SH], __naja_reg[rs1]);
	return 0;
}

static int __naja_vnot(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs   =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int A    = (inst >> 16) & 0x3;
	int vlen = (inst >> 10) & 0x3;

	int N = 1 << (3 + vlen - size);

	if (A)
		printf("vnot.u%dx%d   X%d, X%d\n", 1 << size, N, rd, rs);
	else
		printf("vneg.u%dx%d   X%d, X%d\n", 1 << size, N, rd, rs);
	return 0;
}

static int __naja_vmov_imm(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int u16  =  inst        & 0xffff;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int vlen = (inst >> 16) & 0x3;
	int mvn  = (inst >> 25) & 0x1;

	int N = 1 << (3 + vlen - size);

	if (mvn)
		printf("vmvn.u%dx%d   X%d, %#x\n", 1 << size, N, rd, u16);
	else
		printf("vmov.u%dx%d   X%d, %#x\n", 1 << size, N, rd, u16);
	return 0;
}

static int __naja_jmp_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	uint64_t ip = naja->ip + (simm26 << 2);
	printf("jmp      %#lx\n", ip);
	return 0;
}

static int __naja_call_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	uint64_t ip = naja->ip + (simm26 << 2);
	printf("call     %#lx\n", ip);
	return 0;
}

static int __naja_jmp_reg(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	if (inst & 0x1) {
		int cc  = (inst >> 1) & 0xf;
		int s21 = (inst >> 5) & 0x1fffff;

		if (s21  & 0x100000)
			s21 |= 0xffe00000;

		s21 <<= 2;

		uint64_t ip = naja->ip + s21;

		if (0 == cc)
			printf("jz       %#lx\n", ip);

		else if (1 == cc)
			printf("jnz      %#lx\n", ip);

		else if (2 == cc)
			printf("jge      %#lx\n", ip);

		else if (3 == cc)
			printf("jgt      %#lx\n", ip);

		else if (4 == cc)
			printf("jle      %#lx\n", ip);

		else if (5 == cc)
			printf("jlt      %#lx\n", ip);
		else {
			scf_loge("\n");
			return -EINVAL;
		}
	} else {
		int rd = (inst >> 21) & 0xf;

		printf("jmp      *%s\n", __naja_reg[rd]);
	}
	return 0;
}

static int __naja_call_reg(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0xf;

	printf("call     *%s\n", __naja_reg[rd]);

	return 0;
}

static int __naja_setcc(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0xf;
	int cc = (inst >>  1) & 0xf;

	if (SCF_VM_Z == cc)
		printf("setz     %s\n", __naja_reg[rd]);

	else if (SCF_VM_NZ == cc)
		printf("setnz    %s\n", __naja_reg[rd]);

	else if (SCF_VM_GE == cc)
		printf("setge    %s\n", __naja_reg[rd]);

	else if (SCF_VM_GT == cc)
		printf("setgt    %s\n", __naja_reg[rd]);

	else if (SCF_VM_LT == cc)
		printf("setlt    %s\n", __naja_reg[rd]);

	else if (SCF_VM_LE == cc)
		printf("setle    %s\n", __naja_reg[rd]);
	else {
		scf_loge("inst: %#x\n", inst);
		return -EINVAL;
	}

	return 0;
}

static int __naja_adrp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rd  = (inst >> 21) & 0xf;
	int s21 =  inst & 0x1fffff;

	if (s21  & 0x100000)
		s21 |= ~0x1fffff;

	printf("adrp     %s, [rip, %d]\n", __naja_reg[rd], s21);
	return 0;
}

static int __naja_ret(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	printf("ret\n");
	return 0;
}

static int __naja_spop(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int sr = (inst >> 21) & 0xf;

	printf("spop     sr%d, [%s]\n", sr, __naja_reg[rb]);
	return 0;
}

static int __naja_spush(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int sr = (inst >> 21) & 0xf;

	printf("spush    sr%d, [%s]\n", sr, __naja_reg[rb]);
	return 0;
}

static int __naja_in(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int u16 =  inst        & 0xffff;
	int rd  = (inst >> 21) & 0xf;
	int D   = (inst >> 17) & 0xf;

	if (D)
		printf("out      %s, %#x\n", __naja_reg[rd], u16);
	else
		printf("in       %s, %#x\n", __naja_reg[rd], u16);
	return 0;
}

static int __naja_smov(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int sr =  inst        & 0xf;
	int rd = (inst >> 21) & 0xf;
	int D  = (inst >> 17) & 0xf;

	if (D)
		printf("smov     sr%d, %s\n", sr, __naja_reg[rd]);
	else
		printf("smov     %s, sr%d\n", __naja_reg[rd], sr);
	return 0;
}

static int __naja_iret(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	printf("iret\n");
	return 0;
}

static naja_opcode_pt  naja_opcodes[64] =
{
	__naja_add,      // 0
	__naja_sub,      // 1
	__naja_mul,      // 2
	__naja_div,      // 3

	__naja_ldr_disp, // 4
	__naja_and,      // 5

	__naja_str_disp, // 6
	__naja_or,       // 7

	__naja_pop,      // 8
	__naja_push,     // 9

	__naja_ldr_sib,  // 10
	__naja_str_sib,  // 11
	__naja_mov,      // 12
	__naja_movx,     // 13
	__naja_not,      // 14
	NULL,            // 15

	__naja_fadd,     // 16
	__naja_fsub,     // 17
	__naja_fmul,     // 18
	__naja_fdiv,     // 19

	__naja_fldr_disp,// 20
	NULL,            // 21

	__naja_fstr_disp,// 22
	NULL,            // 23

	NULL,            // 24
	NULL,            // 25
	__naja_fldr_sib, // 26
	NULL,            // 27
	__naja_fmov,     // 28
	__naja_fcvt,     // 29
	__naja_fneg,     // 30
	NULL,            // 31

	__naja_vadd,     // 32
	__naja_vsub,     // 33
	__naja_vmul,     // 34
	__naja_vdiv,     // 35
	NULL,            // 36
	__naja_vand,     // 37
	NULL,            // 38
	__naja_vor,      // 39
	NULL,            // 40
	NULL,            // 41
	NULL,            // 42
	NULL,            // 43
	__naja_vmov,     // 44
	NULL,            // 45
	__naja_vnot,     // 46
	__naja_vmov_imm, // 47

	__naja_jmp_disp, // 48
	__naja_call_disp,// 49
	__naja_jmp_reg,  // 50
	__naja_call_reg, // 51
	__naja_setcc,    // 52
	__naja_adrp,     // 53
	__naja_ret,      // 54
	NULL,            // 55
	__naja_spop,     // 56
	__naja_spush,    // 57
	__naja_in,       // 58
	NULL,            // 59
	__naja_smov,     // 60
	NULL,            // 61
	__naja_iret,     // 62
	NULL,            // 63
};

static int __naja_vm_run(scf_vm_t* vm, const char* path, const char* sys)
{
	scf_elf_sym_t* s;
	scf_vm_naja_t* naja = vm->priv;
	scf_vector_t*  syms = scf_vector_alloc();
	if (!syms)
		return -ENOMEM;

	int ret = scf_elf_read_syms(vm->elf, syms, ".symtab");
	if (ret < 0)
		return ret;

	int i;
	for (i = 0; i < syms->size; i++) {
		s  =        syms->data[i];

		if (ELF64_ST_TYPE(s->st_info) != STT_FUNC)
			continue;

		int64_t offset = s->st_value - vm->text->addr;

		if (offset >= vm->text->len)
			return -1;

		printf("\n%s: \n", s->name);
		int j;
		for (j = 0; j < s->st_size; j += 4) {

			uint32_t inst = *(uint32_t*)(vm->text->data + offset + j);

			naja_opcode_pt pt = naja_opcodes[(inst >> 26) & 0x3f];

			naja->ip = vm->text->addr + offset + j;

			if (!pt) {
				scf_loge("%4d, %#lx: inst: %d, %08x\n", j, naja->ip, (inst >> 26) & 0x3f, inst);
				continue;
			}

			printf("%4d, %#lx: %08x | ", j, naja->ip, inst);

			ret = pt(vm, inst);
			if (ret < 0) {
				scf_loge("\n");
				return ret;
			}
		}
	}

	return 0;
}

static int naja_vm_run(scf_vm_t* vm, const char* path, const char* sys)
{
	int ret = naja_vm_init(vm, path, sys);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	return __naja_vm_run(vm, path, sys);
}

scf_vm_ops_t  vm_ops_naja_asm =
{
	.name  = "naja_asm",
	.open  = naja_vm_open,
	.close = naja_vm_close,
	.run   = naja_vm_run,
};
