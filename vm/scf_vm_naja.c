#include"scf_vm.h"

static const char* somaps[][3] =
{
	{"x64", "/lib/ld-linux-aarch64.so.1",  "/lib64/ld-linux-x86-64.so.2"},
	{"x64", "libc.so.6",                   "/lib/x86_64-linux-gnu/libc.so.6"},
};

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

typedef int (*dyn_func_pt)(uint64_t r0,
		uint64_t r1,
		uint64_t r2,
		uint64_t r3,
		uint64_t r4,
		uint64_t r5,
		double   d0,
		double   d1,
		double   d2,
		double   d3,
		double   d4,
		double   d5,
		double   d6,
		double   d7);

int naja_vm_open(scf_vm_t* vm)
{
	if (!vm)
		return -EINVAL;

	scf_vm_naja_t* naja = calloc(1, sizeof(scf_vm_naja_t));
	if (!naja)
		return -ENOMEM;

	vm->priv = naja;
	return 0;
}

int naja_vm_close(scf_vm_t* vm)
{
	if (vm) {
		if (vm->priv) {
			free(vm->priv);
			vm->priv = NULL;
		}
	}

	return 0;
}

static int naja_vm_dynamic_link(scf_vm_t* vm)
{
	scf_vm_naja_t* naja = vm->priv;
	dyn_func_pt    f    = NULL;

	int64_t  sp = naja->regs[NAJA_REG_SP];

	uint64_t lr  = *(uint64_t*)(naja->stack - (sp +  8));
	uint64_t r10 = *(uint64_t*)(naja->stack - (sp + 16));

	scf_logw("sp: %ld, r10: %#lx, lr: %#lx, vm->jmprel_size: %ld\n", sp, r10, lr, vm->jmprel_size);

	if (r10  > (uint64_t)vm->data->data) {
		r10 -= (uint64_t)vm->data->data;
		r10 +=           vm->data->addr;
	}

	scf_logw("r10: %#lx, text: %p, rodata: %p, data: %p\n", r10, vm->text->data, vm->rodata->data, vm->data->data);

	int i;
	for (i = 0; i < vm->jmprel_size / sizeof(Elf64_Rela); i++) {

		if (r10  == vm->jmprel[i].r_offset) {

			int   j     = ELF64_R_SYM(vm->jmprel[i].r_info);
			char* fname = vm->dynstr + vm->dynsym[j].st_name;

			scf_logw("j: %d, %s\n", j, fname);

			int k;
			for (k = 0; k < vm->sofiles->size; k++) {

				f = dlsym(vm->sofiles->data[k], fname);
				if (f)
					break;
			}

			if (f) {
				int64_t offset = vm->jmprel[i].r_offset - vm->data->addr;

				if (offset < 0 || offset > vm->data->len) {
					scf_loge("\n");
					return -1;
				}

				scf_logw("f: %p\n", f);

				*(void**)(vm->data->data + offset) = f;

				naja->regs[0] = f(naja->regs[0],
						naja->regs[1],
						naja->regs[2],
						naja->regs[3],
						naja->regs[4],
						naja->regs[5],
						naja->fvec[0].d[0],
						naja->fvec[1].d[0],
						naja->fvec[2].d[0],
						naja->fvec[3].d[0],
						naja->fvec[4].d[0],
						naja->fvec[5].d[0],
						naja->fvec[6].d[0],
						naja->fvec[7].d[0]);

				naja->regs[NAJA_REG_SP] += 16;
				return 0;
			}
			break;
		}
	}

	return -1;
}

int naja_vm_init(scf_vm_t* vm, const char* path, const char* sys)
{
	if (!vm || !path)
		return -EINVAL;

	if (vm->elf)
		scf_vm_clear(vm);

	if (vm->priv)
		memset(vm->priv, 0, sizeof(scf_vm_naja_t));
	else {
		vm->priv = calloc(1, sizeof(scf_vm_naja_t));
		if (!vm->priv)
			return -ENOMEM;
	}

	if (vm->phdrs)
		scf_vector_clear(vm->phdrs, (void (*)(void*) )free);
	else {
		vm->phdrs = scf_vector_alloc();
		if (!vm->phdrs)
			return -ENOMEM;
	}

	if (vm->sofiles)
		scf_vector_clear(vm->phdrs, (void (*)(void*) )dlclose);
	else {
		vm->sofiles = scf_vector_alloc();
		if (!vm->sofiles)
			return -ENOMEM;
	}

	int ret = scf_elf_open(&vm->elf, "naja", path, "rb");
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	ret = scf_elf_read_phdrs(vm->elf, vm->phdrs);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	scf_elf_phdr_t* ph;
	int i;
	for (i = 0; i < vm->phdrs->size; i++) {
		ph =        vm->phdrs->data[i];

		if (PT_LOAD ==  ph->ph.p_type) {

			ph->addr = (ph->ph.p_vaddr + ph->ph.p_memsz) & ~(ph->ph.p_align - 1);
			ph->len  = (ph->ph.p_vaddr + ph->ph.p_memsz) - ph->addr;

			ph->data = calloc(1, ph->len);
			if (!ph->data)
				return -ENOMEM;

			fseek(vm->elf->fp, 0, SEEK_SET);

			ret = fread(ph->data, ph->len, 1, vm->elf->fp);
			if (1 != ret) {
				scf_loge("\n");
				return -1;
			}

			scf_logi("i: %d, ph->p_offset: %#lx, ph->p_filesz: %#lx\n", i, ph->ph.p_offset, ph->ph.p_filesz);

			scf_logi("i: %d, ph->addr: %#lx, ph->len: %#lx, %#lx, ph->flags: %#x\n", i, ph->addr, ph->len, ph->ph.p_memsz, ph->ph.p_flags);

			if ((PF_X | PF_R) == ph->ph.p_flags)
				vm->text   =  ph;

			else if ((PF_W | PF_R) == ph->ph.p_flags)
				vm->data   = ph;

			else if (PF_R == ph->ph.p_flags)
				vm->rodata =  ph;
			else {
				scf_loge("\n");
				return -1;
			}

		} else if (PT_DYNAMIC == ph->ph.p_type) {
			ph->addr = ph->ph.p_vaddr;
			ph->len  = ph->ph.p_memsz;

			vm->dynamic = ph;

			scf_logi("ph->addr: %#lx, ph->len: %#lx, %#lx, ph->p_offset: %#lx\n", ph->addr, ph->len, ph->ph.p_memsz, ph->ph.p_offset);
		}
	}

	scf_logi("\n\n");

	if (vm->dynamic) {
		Elf64_Dyn* d = (Elf64_Dyn*)(vm->data->data + vm->dynamic->ph.p_offset);

		vm->jmprel = NULL;
		for (i = 0; i < vm->dynamic->ph.p_filesz / sizeof(Elf64_Dyn); i++) {

			switch (d[i].d_tag) {

				case DT_STRTAB:
					scf_logi("dynstr: %#lx\n", d[i].d_un.d_ptr);
					vm->dynstr = d[i].d_un.d_ptr - vm->text->addr + vm->text->data;
					break;

				case DT_SYMTAB:
					scf_logi("dynsym: %#lx\n", d[i].d_un.d_ptr);
					vm->dynsym = (Elf64_Sym*)(d[i].d_un.d_ptr - vm->text->addr + vm->text->data);
					break;

				case DT_JMPREL:
					scf_logi("JMPREL: %#lx\n", d[i].d_un.d_ptr);
					vm->jmprel      = (Elf64_Rela*)(d[i].d_un.d_ptr - vm->text->addr + vm->text->data);
					vm->jmprel_addr = d[i].d_un.d_ptr;
					break;

				case DT_PLTGOT:
					scf_logi("PLTGOT: %#lx\n", d[i].d_un.d_ptr);
					vm->pltgot = (uint64_t*)(d[i].d_un.d_ptr - vm->data->addr + vm->data->data);
					break;

				default:
					break;
			};
		}

		for (i = 0; i < vm->dynamic->ph.p_filesz / sizeof(Elf64_Dyn); i++) {

			if (DT_NEEDED == d[i].d_tag) {

				uint8_t* name = d[i].d_un.d_ptr + vm->dynstr;

				int j;
				for (j = 0; j < sizeof(somaps) / sizeof(somaps[0]); j++) {

					if (!strcmp(somaps[j][0], sys)
							&& !strcmp(somaps[j][1], name)) {
						name  = somaps[j][2];
						break;
					}
				}

				scf_logi("needed: %s\n", name);

				void* so = dlopen(name, RTLD_LAZY);
				if (!so) {
					scf_loge("dlopen error, so: %s\n", name);
					return -1;
				}

				if (scf_vector_add(vm->sofiles, so) < 0) {
					dlclose(so);
					return -ENOMEM;
				}
			}
		}

		vm->pltgot[2] = (uint64_t)naja_vm_dynamic_link;
	}

	return 0;
}

static int __naja_add(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs0  =  inst        & 0xf;
	int rd   = (inst >> 21) & 0xf;
	int size = (inst >> 18) & 0x3;
	int SH   = (inst >> 16) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm12 = (inst >> 4) & 0xfff;

		naja->regs[rd]  = naja->regs[rs0] + uimm12;

		NAJA_PRINTF("add%c     %s, %s, %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;
		uint64_t v1    = naja->regs[rs1];

		NAJA_PRINTF("add%c     %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);

		switch (SH) {
			case 0:
				naja->regs[rd] = naja->regs[rs0] + (scf_zero_extend(v1, 8 << size) << uimm6);
				break;
			case 1:
				naja->regs[rd] = naja->regs[rs0] + (scf_zero_extend(v1, 8 << size) >> uimm6);
				break;
			default:
				naja->regs[rd] = naja->regs[rs0] + (((int64_t)scf_sign_extend(v1, 8 << size)) >> uimm6);
				break;
		};
	}

	naja->ip += 4;
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

		naja->regs[rd]  = naja->regs[rs0] - uimm12;

		if (0xf == rd)
			NAJA_PRINTF("cmp%c     %s, %lu\n", __naja_size[size], __naja_reg[rs0], uimm12);
		else
			NAJA_PRINTF("sub%c     %s, %s, %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;
		uint64_t v1    = naja->regs[rs1];

		if (0xf == rd)
			NAJA_PRINTF("cmp%c     %s, %s %s %lu\n", __naja_size[size], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
		else
			NAJA_PRINTF("sub%c     %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);

		switch (SH) {
			case 0:
				naja->regs[rd] = naja->regs[rs0] - (scf_zero_extend(v1, 8 << size) << uimm6);
				break;
			case 1:
				naja->regs[rd] = naja->regs[rs0] - (scf_zero_extend(v1, 8 << size) >> uimm6);
				break;
			default:
				naja->regs[rd] = naja->regs[rs0] - (((int64_t)scf_sign_extend(v1, 8 << size)) >> uimm6);
				break;
		}
	}

	if (0xf == rd) { // cmp
		int ret = scf_sign_extend(naja->regs[rd], 8 << size);

		if (0 == ret)
			naja->flags = 0x1;
		else if (ret > 0)
			naja->flags = 0x4;
		else
			naja->flags = 0x2;
	}

	naja->ip += 4;
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
		NAJA_PRINTF("%smul%c     %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1]);
	else {
		if (0 == A)
			NAJA_PRINTF("%smadd%c    %s, %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs2], __naja_reg[rs0], __naja_reg[rs1]);
		else
			NAJA_PRINTF("%smsub%c    %s, %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs2], __naja_reg[rs0], __naja_reg[rs1]);
	}

	uint64_t v0 = naja->regs[rs0];
	uint64_t v1 = naja->regs[rs1];

	if (s)
		naja->regs[rd] = (int64_t)scf_sign_extend(v0, 8 << size) * (int64_t)scf_sign_extend(v1, 8 << size);
	else
		naja->regs[rd] = v0 * v1;

	if (0xf != rs2) {
		if (0 == A)
			naja->regs[rd] += naja->regs[rs2];
		else
			naja->regs[rd]  = naja->regs[rs2] - naja->regs[rd];
	}

	naja->ip += 4;
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

	NAJA_PRINTF("%sdiv%c     %s, %s, %s\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1]);

	uint64_t v0 = naja->regs[rs0];
	uint64_t v1 = naja->regs[rs1];

	if (s)
		naja->regs[rd] = (int64_t)scf_sign_extend(v0, 8 << size) / (int64_t)scf_sign_extend(v1, 8 << size);
	else
		naja->regs[rd] = scf_zero_extend(v0, 8 << size) / scf_zero_extend(v1, 8 << size);

	naja->ip += 4;
	return 0;
}

static int __naja_mem(scf_vm_t* vm, int64_t addr, uint8_t** pdata, int64_t* poffset)
{
	scf_vm_naja_t* naja   = vm->priv;
	uint8_t*       data   = NULL;
	int64_t        offset = 0;

	if (addr >= (int64_t)vm->data->data) {
		data  = (uint8_t*)addr;

		if (addr >= (int64_t)vm->data->data + vm->data->len) {
			scf_loge("\n");
			return -1;
		}

	} else if (addr >= (int64_t)vm->rodata->data) {
		data  = (uint8_t*)addr;

		if (addr >= (int64_t)vm->rodata->data + vm->rodata->len) {
			scf_loge("\n");
			return -1;
		}

	} else if (addr >= (int64_t)vm->text->data) {
		data  = (uint8_t*)addr;

		if (addr >= (int64_t)vm->text->data + vm->text->len) {
			scf_loge("\n");
			return -1;
		}

	} else if (addr  >= 0x800000) {
		data   = vm->data->data;
		offset = addr - vm->data->addr;

		if (offset >= vm->data->len) {
			scf_loge("offset: %#lx, vm->data->len: %ld\n", offset, vm->data->len);
			return -1;
		}

	} else if (addr >= 0x600000) {
		data   = vm->rodata->data;
		offset = addr - vm->rodata->addr;

		if (offset >= vm->rodata->len) {
			scf_loge("\n");
			return -1;
		}

	} else if (addr >= 0x400000) {
		data   = vm->text->data;
		offset = addr - vm->text->addr;

		if (offset >= vm->text->len) {
			scf_loge("\n");
			return -1;
		}
	} else {
		data   = naja->stack;
		offset = addr;
	}

	*poffset = offset;
	*pdata   = data;
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

	NAJA_PRINTF("ldr%s%c  %s, [%s, %d]\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rb], s13 << size);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << size;

	if (data   == naja->stack) {
		offset = -offset;

		scf_logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset >= 0);

		if (naja->size < offset) {
			scf_loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << size;
	}

	switch (size) {
		case 0:
			if (s)
				naja->regs[rd] = *(int8_t*)(data + offset);
			else
				naja->regs[rd] = *(uint8_t*)(data + offset);
			break;

		case 1:
			if (s)
				naja->regs[rd] = *(int16_t*)(data + offset);
			else
				naja->regs[rd] = *(uint16_t*)(data + offset);
			break;

		case 2:
			if (s)
				naja->regs[rd] = *(int32_t*)(data + offset);
			else
				naja->regs[rd] = *(uint32_t*)(data + offset);
			break;

		default:
			naja->regs[rd] = *(uint64_t*)(data + offset);
			break;
	};

	naja->ip += 4;
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
			NAJA_PRINTF("teq%c   %s, %#lx\n", __naja_size[size], __naja_reg[rs0], uimm12);
		else
			NAJA_PRINTF("and%c   %s, %s, %#lx\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);

		naja->regs[rd]  = naja->regs[rs0] & uimm12;
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;
		uint64_t v1    = naja->regs[rs1];

		if (0xf == rd)
			NAJA_PRINTF("teq%c   %s, %s %s %lu\n", __naja_size[size], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);
		else
			NAJA_PRINTF("and%c   %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);

		if (0 == SH)
			naja->regs[rd]  = naja->regs[rs0] & (v1 << uimm6);
		else if (1 == SH)
			naja->regs[rd]  = naja->regs[rs0] & (v1 >> uimm6);
		else
			naja->regs[rd]  = naja->regs[rs0] & (((int64_t)scf_sign_extend(v1, 8 << size)) >> uimm6);
	}

	if (0xf == rd)
		naja->flags = !!scf_zero_extend(naja->regs[rd], 8 << size);

	naja->ip += 4;
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

	NAJA_PRINTF("str%c    %s, [%s, %d]\n", __naja_size[size], __naja_reg[rs], __naja_reg[rb], s13 << size);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << size;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			scf_loge("offset0: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << size;
	}

	switch (size) {
		case 0:
			*(uint8_t*)(data + offset) = naja->regs[rs];
			break;

		case 1:
			*(uint16_t*)(data + offset) = naja->regs[rs];
			break;

		case 2:
			*(uint32_t*)(data + offset) = naja->regs[rs];
			break;
		default:
			*(uint64_t*)(data + offset) = naja->regs[rs];
			break;
	};

	naja->ip += 4;
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

		NAJA_PRINTF("or%c    %s, %s, %#lx\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], uimm12);

		naja->regs[rd]  = naja->regs[rs0] | uimm12;
	} else {
		uint64_t uimm6 = (inst >> 10) & 0x3f;
		int      rs1   = (inst >>  4) & 0xf;
		uint64_t v1    = naja->regs[rs1];

		NAJA_PRINTF("or%c    %s, %s, %s %s %lu\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs0], __naja_reg[rs1], __naja_SH[SH], uimm6);

		if (0 == SH)
			naja->regs[rd]  = naja->regs[rs0] | (v1 << uimm6);
		else if (1 == SH)
			naja->regs[rd]  = naja->regs[rs0] | (v1 >> uimm6);
		else
			naja->regs[rd]  = naja->regs[rs0] | (((int64_t)scf_sign_extend(v1, 8 << size)) >> uimm6);
	}

	naja->ip += 4;
	return 0;
}

static int __naja_pop(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int rd = (inst >> 21) & 0xf;
	int s  = (inst >> 17) & 0x1;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	if (data   == naja->stack) {
		offset  = -offset;

		scf_logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset > 0);

		if (naja->size < offset) {
			scf_loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 8;
	}

	if (s) {
		NAJA_PRINTF("fpop     d%d, [%s]\n", rd, __naja_reg[rb]);

		naja->fvec[rd].d[0] = *(double*)(data + offset);
	} else {
		naja->regs[rd] = *(uint64_t*)(data + offset);

		NAJA_PRINTF("pop      %s, [%s], offset: %ld, value: %#lx\n", __naja_reg[rd], __naja_reg[rb], offset, naja->regs[rd]);
	}

	naja->regs[rb] += 8;

	naja->ip += 4;
	return 0;
}

static int __naja_push(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int rs = (inst >> 21) & 0xf;
	int s  = (inst >> 17) & 0x1;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	if (data   == naja->stack) {
		offset = -offset;

		scf_logd("offset0: %ld, size: %ld\n", offset, naja->size);

		assert(offset >= 0);

		offset += 8;

		if (naja->size < offset) {
			data = realloc(naja->stack, offset + STACK_INC);
			if (!data)
				return -ENOMEM;

			naja->stack = data;
			naja->size  = offset + STACK_INC;
		}

		offset -= 8;
	}

	if (s) {
		NAJA_PRINTF("fpush    d%d, [%s]\n", rs, __naja_reg[rb]);

		*(double*)(data + offset) = naja->fvec[rs].d[0];
	} else {
		*(uint64_t*)(data + offset) = naja->regs[rs];

		NAJA_PRINTF("push     %s, [%s], offset: %ld, value: %#lx\n", __naja_reg[rs], __naja_reg[rb], offset, naja->regs[rs]);
	}

	naja->regs[rb] -= 8;

	naja->ip += 4;
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

	NAJA_PRINTF("ldr%s%c   %s, [%s, %s, %d]\n", __naja_sign[s], __naja_size[size], __naja_reg[rd], __naja_reg[rb], __naja_reg[ri], uimm6);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += (naja->regs[ri] << uimm6);

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset >= 0);

		if (naja->size < offset) {
			scf_loge("\n");
			return -1;
		}

		offset -= 1 << size;
	}

	switch (size) {
		case 0:
			if (s)
				naja->regs[rd] = *(int8_t*)(data + offset);
			else
				naja->regs[rd] = *(uint8_t*)(data + offset);
			break;

		case 1:
			if (s)
				naja->regs[rd] = *(int16_t*)(data + offset);
			else
				naja->regs[rd] = *(uint16_t*)(data + offset);
			break;

		case 2:
			if (s)
				naja->regs[rd] = *(int32_t*)(data + offset);
			else
				naja->regs[rd] = *(uint32_t*)(data + offset);
			break;
		default:
			naja->regs[rd] = *(uint64_t*)(data + offset);
			break;
	};

	naja->ip += 4;
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
		NAJA_PRINTF("fstr.%d   d%d, [%s, %s, %d]\n", 1 << size, rs, __naja_reg[rb], __naja_reg[ri], uimm6);
	else
		NAJA_PRINTF("str%c     %s, [%s, %s, %d]\n", __naja_size[size], __naja_reg[rs], __naja_reg[rb], __naja_reg[ri], uimm6);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += naja->regs[ri] << uimm6;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset >= 0);

		if (naja->size < offset) {
			scf_loge("\n");
			return -1;
		}

		offset -= 1 << size;
	}

	switch (size) {
		case 0:
			*(uint8_t*)(data + offset) = naja->regs[rs];
			break;

		case 1:
			*(uint16_t*)(data + offset) = naja->regs[rs];
			break;

		case 2:
			*(uint32_t*)(data + offset) = naja->regs[rs];
			break;

		default:
			*(uint64_t*)(data + offset) = naja->regs[rs];
			break;
	};

	naja->ip += 4;
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
			NAJA_PRINTF("mov%c     %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
		else
			NAJA_PRINTF("mov%c     %s, %s %s %d\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs], __naja_SH[SH], uimm6);
	} else
		NAJA_PRINTF("mov%c     %s, %s %s %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs], __naja_SH[SH], __naja_reg[rs1]);

	uint64_t v = naja->regs[rs];

	if (0xf != rs1)
		uimm6 = naja->regs[rs1] & 0x3f;

	switch (SH) {
		case 0:
			naja->regs[rd] = v << uimm6;
			break;
		case 1:
			naja->regs[rd] = v >> uimm6;
			break;
		case 2:
			naja->regs[rd] = (int64_t)scf_sign_extend(v, 8 << size) >> uimm6;
			break;
		default:
			scf_loge("\n");
			return -1;
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_movx(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rs    =  inst        & 0xf;
	int rd    = (inst >> 21) & 0xf;
	int size  = (inst >> 18) & 0x3;
	int s     = (inst >> 17) & 0x1;

	if (s) {
		NAJA_PRINTF("movs%cq    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);

		naja->regs[rd] = scf_sign_extend(naja->regs[rs], 8 << size);
	} else {
		naja->regs[rd] = scf_zero_extend(naja->regs[rs], 8 << size);

		NAJA_PRINTF("movz%cq    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
	}

	naja->ip += 4;
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
			NAJA_PRINTF("neg%c    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);

			naja->regs[rd] = -scf_zero_extend(naja->regs[rs], 8 << size);
			break;
		case 1:
			naja->regs[rd] = ~scf_zero_extend(naja->regs[rs], 8 << size);

			NAJA_PRINTF("not%c    %s, %s\n", __naja_size[size], __naja_reg[rd], __naja_reg[rs]);
			break;
		case 2:
			if (0 == size) {
				NAJA_PRINTF("mov      %s, %d\n", __naja_reg[rd], u16);

				naja->regs[rd]  = u16;
			} else {
				naja->regs[rd] |= u16 << size;

				NAJA_PRINTF("movt     %s, %d LSL %d\n", __naja_reg[rd], u16, 16 << size);
			}
			break;
		default:
			naja->regs[rd] = ~u16;

			NAJA_PRINTF("mvn%c    %s, %d\n", __naja_size[size], __naja_reg[rd], u16);
			break;
	};

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (V) {
		N = 1 << (3 + vlen - size);

		NAJA_PRINTF("vadd.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	} else
		NAJA_PRINTF("fadd.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				naja->fvec[rd].f[i] = naja->fvec[rs0].f[i] + naja->fvec[rs1].f[i];
				break;
			case 3:
				naja->fvec[rd].d[i] = naja->fvec[rs0].d[i] + naja->fvec[rs1].d[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (V) {
		N = 1 << (3 + vlen - size);

		if (cmp)
			NAJA_PRINTF("vcmp.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
		else
			NAJA_PRINTF("vsub.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	} else {
		if (cmp)
			NAJA_PRINTF("fcmp.%d   d%d, d%d\n", 1 << size, rs0, rs1);
		else
			NAJA_PRINTF("fsub.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);
	}

	for (i = 0; i < N; i++) {
		double d;

		switch (size) {
			case 2:
				d = naja->fvec[rs0].f[i] - naja->fvec[rs1].f[i];

				if (!cmp)
					naja->fvec[rd].f[i] = d;
				break;

			case 3:
				d = naja->fvec[rs0].d[i] - naja->fvec[rs1].d[i];
				if (!cmp)
					naja->fvec[rd].d[i] = d;
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};

		if (cmp) {
			uint32_t flags;

			if (d > 0.0)
				flags = 0x4;
			else if (d < 0.0)
				flags = 0x2;
			else
				flags = 0x1;

			if (V)
				naja->fvec[rd].l[i] = flags;
			else
				naja->flags = flags;
		}
	}

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (V) {
		N = 1 << (3 + vlen - size);

		if (0xf == rs2)
			NAJA_PRINTF("vmul.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
		else {
			if (A)
				NAJA_PRINTF("vmsub.f%dx%d   X%d, X%d, X%d, X%d\n", 1 << size, N, rd, rs2, rs0, rs1);
			else
				NAJA_PRINTF("vmadd.f%dx%d   X%d, X%d, X%d, X%d\n", 1 << size, N, rd, rs2, rs0, rs1);
		}
	} else {
		if (0xf == rs2)
			NAJA_PRINTF("fmul.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);
		else {
			if (A)
				NAJA_PRINTF("fmsub.%d   d%d, d%d, d%d, d%d\n", 1 << size, rd, rs2, rs0, rs1);
			else
				NAJA_PRINTF("fmadd.%d   d%d, d%d, d%d, d%d\n", 1 << size, rd, rs2, rs0, rs1);
		}
	}

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				naja->fvec[rd].f[i] = naja->fvec[rs0].f[i] * naja->fvec[rs1].f[i];

				if (0xf != rs2) {
					if (A)
						naja->fvec[rd].f[i] = naja->fvec[rs2].f[i] - naja->fvec[rd].f[i];
					else
						naja->fvec[rd].f[i] += naja->fvec[rs2].f[i];
				}
				break;
			case 3:
				naja->fvec[rd].d[i] = naja->fvec[rs0].d[i] * naja->fvec[rs1].d[i];

				if (0xf != rs2) {
					if (A)
						naja->fvec[rd].d[i] = naja->fvec[rs2].d[i] - naja->fvec[rd].d[i];
					else
						naja->fvec[rd].d[i] += naja->fvec[rs2].d[i];
				}
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (V) {
		N = 1 << (3 + vlen - size);

		NAJA_PRINTF("vdiv.f%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	} else
		NAJA_PRINTF("fdiv.%d   d%d, d%d, d%d\n", 1 << size, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				naja->fvec[rd].f[i] = naja->fvec[rs0].f[i] / naja->fvec[rs1].f[i];
				break;
			case 3:
				naja->fvec[rd].d[i] = naja->fvec[rs0].d[i] / naja->fvec[rs1].d[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	if (V) {
		N = 1 << (3 + vlen - size);

		if (A)
			NAJA_PRINTF("vldr.f%dx%d  X%d, [%s]!\n", 1 << size, N, rd, __naja_reg[rb]);
		else
			NAJA_PRINTF("vldr.f%dx%d  X%d, [%s]\n", 1 << size, N, rd, __naja_reg[rb]);
	} else
		NAJA_PRINTF("fldr.%d  d%d, [%s, %d]\n", 1 << size, rd, __naja_reg[rb], s13 << size);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << size;

	if (data   == naja->stack) {
		offset = -offset;

		scf_logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset > 0);

		if (naja->size < offset) {
			scf_loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= N << size;
	}

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				naja->fvec[rd].f[i] = *(float*)(data + offset + (i << 2));
				break;
			case 3:
				naja->fvec[rd].d[i] = *(double*)(data + offset + (i << 3));
				break;
			default:
				scf_loge("size: %d\n", size);
				return -1;
				break;
		};
	}

	if (A)
		naja->regs[rb] += offset + (N << size);

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	if (V) {
		N = 1 << (3 + vlen - size);

		if (A)
			NAJA_PRINTF("vstr.f%dx%d  X%d, [%s]!\n", 1 << size, N, rd, __naja_reg[rb]);
		else
			NAJA_PRINTF("vstr.f%dx%d  X%d, [%s]\n", 1 << size, N, rd, __naja_reg[rb]);
	} else
		NAJA_PRINTF("fstr.%d  d%d, [%s, %d]\n", 1 << size, rd, __naja_reg[rb], s13 << size);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << size;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			scf_loge("offset0: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= N << size;
	}

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				*(float*)(data + offset + (i << 2)) = naja->fvec[rd].f[i];
				break;
			case 3:
				*(double*)(data + offset + (i << 3)) = naja->fvec[rd].d[i];
				break;
			default:
				scf_loge("size: %d\n", size);
				return -1;
				break;
		};
	}

	if (A)
		naja->regs[rb] += offset + (N << size);

	naja->ip += 4;
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

	NAJA_PRINTF("fldr.%d   d%d, [%s, %s, %d]\n", 1 << size, rd, __naja_reg[rb], __naja_reg[ri], uimm6);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += naja->regs[ri] << uimm6;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			scf_loge("\n");
			return -1;
		}

		offset -= 1 << size;
	}

	switch (size) {
		case 2:
			naja->fvec[rd].f[0] = *(float*)(data + offset);
			break;
		case 3:
			naja->fvec[rd].d[0] = *(double*)(data + offset);
			break;
		default:
			scf_loge("\n");
			return -1;
			break;
	};

	naja->ip += 4;
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
	int N    = 1;
	int i;

	if (V) {
		N = 1 << (3 + vlen - size);

		NAJA_PRINTF("vmov.f%dx%d  X%d, X%d\n", 1 << size, N, rd, rs);
	} else
		NAJA_PRINTF("fmov.%d      d%d, d%d\n", 1 << size, rd, rs);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				naja->fvec[rd].f[i] = naja->fvec[rs].f[i];
				break;
			case 3:
				naja->fvec[rd].d[i] = naja->fvec[rs].d[i];
				break;
			default:
				scf_loge("size: %d\n", size);
				return -1;
				break;
		};
	}

	naja->ip += 4;
	return 0;
}

static uint64_t __naja_to_u64(scf_vm_naja_t* naja, int i, int r, int sign, int size)
{
	uint64_t ret;

	switch (size) {
		case 0:
			ret = naja->fvec[r].b[i];
			break;
		case 1:
			ret = naja->fvec[r].w[i];
			break;
		case 2:
			ret = naja->fvec[r].l[i];
			break;
		default:
			ret = naja->fvec[r].q[i];
			break;
	};

	if (sign)
		return scf_sign_extend(ret, 8 << size);
	return scf_zero_extend(ret, 8 << size);
}

static void __naja_u64_to_int(scf_vm_naja_t* naja, int i, int r, int size, uint64_t src)
{
	switch (size) {
		case 0:
			naja->fvec[r].b[i] = src;
			break;
		case 1:
			naja->fvec[r].w[i] = src;
			break;
		case 2:
			naja->fvec[r].l[i] = src;
			break;
		default:
			naja->fvec[r].q[i] = src;
			break;
	};
}

static void __naja_u64_to_float(scf_vm_naja_t* naja, int i, int r, int size, uint64_t src)
{
	switch (size) {
		case 2:
			naja->fvec[r].f[i] = src;
			break;
		case 3:
			naja->fvec[r].d[i] = src;
			break;
		default:
			break;
	};
}

static double __naja_to_double(scf_vm_naja_t* naja, int i, int r, int size)
{
	double ret;

	switch (size) {
		case 2:
			ret = naja->fvec[r].f[i];
			break;
		case 3:
			ret = naja->fvec[r].d[i];
			break;
		default:
			ret = 0.0;
			break;
	};

	return ret;
}

static void __naja_double_to_int(scf_vm_naja_t* naja, int i, int r, int size, double src)
{
	switch (size) {
		case 0:
			naja->fvec[r].b[i] = src;
			break;
		case 1:
			naja->fvec[r].w[i] = src;
			break;
		case 2:
			naja->fvec[r].l[i] = src;
			break;
		default:
			naja->fvec[r].q[i] = src;
			break;
	};
}

static void __naja_double_to_float(scf_vm_naja_t* naja, int i, int r, int size, double src)
{
	switch (size) {
		case 2:
			naja->fvec[r].f[i] = src;
			break;
		case 3:
			naja->fvec[r].d[i] = src;
			break;
		default:
			break;
	};
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
		int i;

		NAJA_PRINTF("vcvt%c%dto%c%dx%d  X%d, X%d\n",
				IS_array[Is][ss], 1 << ssize,
				IS_array[Id][sd], 1 << dsize,
				N, rd, rs);

		for (i = 0; i < N; i++) {
			if (Is) {
				uint64_t src = __naja_to_u64(naja, i, rs, ss, ssize);
				if (Id)
					__naja_u64_to_int(naja, i, rd, dsize, src);
				else
					__naja_u64_to_float(naja, i, rd, dsize, src);

			} else {
				double src = __naja_to_double(naja, i, rs, ssize);
				if (Id)
					__naja_double_to_int(naja, i, rd, dsize, src);
				else
					__naja_double_to_float(naja, i, rd, dsize, src);
			}
		}
	} else {
		NAJA_PRINTF("fcvt%c%dto%c%d      %c%d, %c%d\n",
				IS_array[Is][ss], 1 << ssize,
				IS_array[Id][sd], 1 << dsize,
				R_array[Id], rd, R_array[Is], rs);

		if (Is) {
			uint64_t src;
			if (ss)
				src = scf_sign_extend(naja->regs[rs], 8 << ssize);
			else
				src = scf_zero_extend(naja->regs[rs], 8 << ssize);

			if (Id)
				naja->regs[rd] = src;
			else
				__naja_u64_to_float(naja, 0, rd, dsize, src);

		} else {
			double src = __naja_to_double(naja, 0, rs, ssize);

			if (Id)
				naja->regs[rd] = src;
			else
				__naja_u64_to_float(naja, 0, rd, dsize, src);
		}
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
	int N    = 1;
	int i;

	if (V) {
		N = 1 << (3 + vlen - size);

		NAJA_PRINTF("vneg.f%dx%d  X%d, X%d\n", 1 << size, N, rd, rs);
	} else
		NAJA_PRINTF("fneg.%d      d%d, d%d\n", 1 << size, rd, rs);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 2:
				naja->fvec[rd].f[i] = -naja->fvec[rs].f[i];
				break;
			case 3:
				naja->fvec[rd].d[i] = -naja->fvec[rs].d[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	NAJA_PRINTF("vadd.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				naja->fvec[rd].b[i] = naja->fvec[rs0].b[i] + naja->fvec[rs1].b[i];
				break;
			case 1:
				naja->fvec[rd].w[i] = naja->fvec[rs0].w[i] + naja->fvec[rs1].w[i];
				break;
			case 2:
				naja->fvec[rd].l[i] = naja->fvec[rs0].l[i] + naja->fvec[rs1].l[i];
				break;
			case 3:
				naja->fvec[rd].q[i] = naja->fvec[rs0].q[i] + naja->fvec[rs1].q[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	if (cmp)
		NAJA_PRINTF("vcmp.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	else
		NAJA_PRINTF("vsub.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				naja->fvec[rd].b[i] = naja->fvec[rs0].b[i] - naja->fvec[rs1].b[i];
				if (cmp) {
					int8_t i = naja->fvec[rd].b[i];
					if (0 == i)
						naja->fvec[rd].b[i] = 0x1;
					else if (i > 0)
						naja->fvec[rd].b[i] = 0x4;
					else
						naja->fvec[rd].b[i] = 0x2;
				}
				break;
			case 1:
				naja->fvec[rd].w[i] = naja->fvec[rs0].w[i] - naja->fvec[rs1].w[i];
				if (cmp) {
					int16_t i = naja->fvec[rd].w[i];
					if (0 == i)
						naja->fvec[rd].w[i] = 0x1;
					else if (i > 0)
						naja->fvec[rd].w[i] = 0x4;
					else
						naja->fvec[rd].w[i] = 0x2;
				}
				break;
			case 2:
				naja->fvec[rd].l[i] = naja->fvec[rs0].l[i] - naja->fvec[rs1].l[i];
				if (cmp) {
					int32_t i = naja->fvec[rd].l[i];
					if (0 == i)
						naja->fvec[rd].l[i] = 0x1;
					else if (i > 0)
						naja->fvec[rd].l[i] = 0x4;
					else
						naja->fvec[rd].l[i] = 0x2;
				}
				break;
			case 3:
				naja->fvec[rd].q[i] = naja->fvec[rs0].q[i] - naja->fvec[rs1].q[i];
				if (cmp) {
					int64_t i = naja->fvec[rd].q[i];
					if (0 == i)
						naja->fvec[rd].q[i] = 0x1;
					else if (i > 0)
						naja->fvec[rd].q[i] = 0x4;
					else
						naja->fvec[rd].q[i] = 0x2;
				}
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	static char  s_array[2] = {'u',    'i'};
	static char* A_array[2] = {"madd", "msub"};

	if (0xf == rs2)
		NAJA_PRINTF("vmul.%c%dx%d  X%d, X%d, X%d\n", s_array[s], 1 << size, N, rd, rs0, rs1);
	else
		NAJA_PRINTF("v%s.%c%dx%d  X%d, X%d, X%d, X%d\n", A_array[A], s_array[s], 1 << size, N, rd, rs2, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				naja->fvec[rd].b[i] = naja->fvec[rs0].b[i] * naja->fvec[rs1].b[i];
				if (0xf != rs2) {
					if (A)
						naja->fvec[rd].b[i]  = naja->fvec[rs2].b[i] - naja->fvec[rd].b[i];
					else
						naja->fvec[rd].b[i] += naja->fvec[rs2].b[i];
				}
				break;
			case 1:
				naja->fvec[rd].w[i] = naja->fvec[rs0].w[i] * naja->fvec[rs1].w[i];
				if (0xf != rs2) {
					if (A)
						naja->fvec[rd].w[i]  = naja->fvec[rs2].w[i] - naja->fvec[rd].w[i];
					else
						naja->fvec[rd].w[i] += naja->fvec[rs2].w[i];
				}
				break;
			case 2:
				naja->fvec[rd].l[i] = naja->fvec[rs0].l[i] * naja->fvec[rs1].l[i];
				if (0xf != rs2) {
					if (A)
						naja->fvec[rd].l[i]  = naja->fvec[rs2].l[i] - naja->fvec[rd].l[i];
					else
						naja->fvec[rd].l[i] += naja->fvec[rs2].l[i];
				}
				break;
			case 3:
				naja->fvec[rd].q[i] = naja->fvec[rs0].q[i] * naja->fvec[rs1].q[i];
				if (0xf != rs2) {
					if (A)
						naja->fvec[rd].q[i]  = naja->fvec[rs2].q[i] - naja->fvec[rd].q[i];
					else
						naja->fvec[rd].q[i] += naja->fvec[rs2].q[i];
				}
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	if (s)
		NAJA_PRINTF("vdiv.i%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	else
		NAJA_PRINTF("vdiv.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				naja->fvec[rd].b[i] = naja->fvec[rs0].b[i] / naja->fvec[rs1].b[i];
				break;
			case 1:
				naja->fvec[rd].w[i] = naja->fvec[rs0].w[i] / naja->fvec[rs1].w[i];
				break;
			case 2:
				naja->fvec[rd].l[i] = naja->fvec[rs0].l[i] / naja->fvec[rs1].l[i];
				break;
			case 3:
				naja->fvec[rd].q[i] = naja->fvec[rs0].q[i] / naja->fvec[rs1].q[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	if (teq)
		NAJA_PRINTF("vteq.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);
	else
		NAJA_PRINTF("vand.u%dx%d  X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				naja->fvec[rd].b[i] = naja->fvec[rs0].b[i] & naja->fvec[rs1].b[i];
				if (teq)
					naja->fvec[rd].b[i] = !!naja->fvec[rd].b[i];
				break;
			case 1:
				naja->fvec[rd].w[i] = naja->fvec[rs0].w[i] & naja->fvec[rs1].w[i];
				if (teq)
					naja->fvec[rd].w[i] = !!naja->fvec[rd].w[i];
				break;
			case 2:
				naja->fvec[rd].l[i] = naja->fvec[rs0].l[i] & naja->fvec[rs1].l[i];
				if (teq)
					naja->fvec[rd].l[i] = !!naja->fvec[rd].l[i];
				break;
			case 3:
				naja->fvec[rd].q[i] = naja->fvec[rs0].q[i] & naja->fvec[rs1].q[i];
				if (teq)
					naja->fvec[rd].q[i] = !!naja->fvec[rd].q[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	NAJA_PRINTF("vor.u%dx%d   X%d, X%d, X%d\n", 1 << size, N, rd, rs0, rs1);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				naja->fvec[rd].b[i] = naja->fvec[rs0].b[i] | naja->fvec[rs1].b[i];
				break;
			case 1:
				naja->fvec[rd].w[i] = naja->fvec[rs0].w[i] | naja->fvec[rs1].w[i];
				break;
			case 2:
				naja->fvec[rd].l[i] = naja->fvec[rs0].l[i] | naja->fvec[rs1].l[i];
				break;
			case 3:
				naja->fvec[rd].q[i] = naja->fvec[rs0].q[i] | naja->fvec[rs1].q[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	if (IMM) {
		if (0 == uimm6)
			NAJA_PRINTF("vmov.u%dx%d   X%d, X%d\n", 1 << size, N, rd, rs);
		else
			NAJA_PRINTF("vmov.u%dx%d   X%d, X%d %s %d\n", 1 << size, N, rd, rs, __naja_SH[SH], uimm6);
	} else
		NAJA_PRINTF("vmov.u%dx%d   X%d, X%d %s %s\n", 1 << size, N, rd, rs, __naja_SH[SH], __naja_reg[rs1]);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				if (!IMM)
					uimm6 = naja->fvec[rs1].b[i];

				if (0 == SH)
					naja->fvec[rd].b[i] = naja->fvec[rs].b[i] << uimm6;
				else if (1 == SH)
					naja->fvec[rd].b[i] = naja->fvec[rs].b[i] >> uimm6;
				else if (2 == SH)
					naja->fvec[rd].b[i] = (int8_t)naja->fvec[rs].b[i] >> uimm6;
				else {
					scf_loge("\n");
					return -1;
				}
				break;
			case 1:
				if (!IMM)
					uimm6 = naja->fvec[rs1].w[i];

				if (0 == SH)
					naja->fvec[rd].w[i] = naja->fvec[rs].w[i] << uimm6;
				else if (1 == SH)
					naja->fvec[rd].w[i] = naja->fvec[rs].w[i] >> uimm6;
				else if (2 == SH)
					naja->fvec[rd].w[i] = (int8_t)naja->fvec[rs].w[i] >> uimm6;
				else {
					scf_loge("\n");
					return -1;
				}
				break;
			case 2:
				if (!IMM)
					uimm6 = naja->fvec[rs1].l[i];

				if (0 == SH)
					naja->fvec[rd].l[i] = naja->fvec[rs].l[i] << uimm6;
				else if (1 == SH)
					naja->fvec[rd].l[i] = naja->fvec[rs].l[i] >> uimm6;
				else if (2 == SH)
					naja->fvec[rd].l[i] = (int8_t)naja->fvec[rs].l[i] >> uimm6;
				else {
					scf_loge("\n");
					return -1;
				}
				break;
			case 3:
				if (!IMM)
					uimm6 = naja->fvec[rs1].q[i];

				if (0 == SH)
					naja->fvec[rd].q[i] = naja->fvec[rs].q[i] << uimm6;
				else if (1 == SH)
					naja->fvec[rd].q[i] = naja->fvec[rs].q[i] >> uimm6;
				else if (2 == SH)
					naja->fvec[rd].q[i] = (int8_t)naja->fvec[rs].q[i] >> uimm6;
				else {
					scf_loge("\n");
					return -1;
				}
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	if (A)
		NAJA_PRINTF("vnot.u%dx%d   X%d, X%d\n", 1 << size, N, rd, rs);
	else
		NAJA_PRINTF("vneg.u%dx%d   X%d, X%d\n", 1 << size, N, rd, rs);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				if (A)
					naja->fvec[rd].b[i] = ~naja->fvec[rs].b[i];
				else
					naja->fvec[rd].b[i] = -naja->fvec[rs].b[i];
				break;
			case 1:
				if (A)
					naja->fvec[rd].w[i] = ~naja->fvec[rs].w[i];
				else
					naja->fvec[rd].w[i] = -naja->fvec[rs].w[i];
				break;
			case 2:
				if (A)
					naja->fvec[rd].l[i] = ~naja->fvec[rs].l[i];
				else
					naja->fvec[rd].l[i] = -naja->fvec[rs].l[i];
				break;
			case 3:
				if (A)
					naja->fvec[rd].q[i] = ~naja->fvec[rs].q[i];
				else
					naja->fvec[rd].q[i] = -naja->fvec[rs].q[i];
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

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
	int i;

	if (mvn)
		NAJA_PRINTF("vmvn.u%dx%d   X%d, %#x\n", 1 << size, N, rd, u16);
	else
		NAJA_PRINTF("vmov.u%dx%d   X%d, %#x\n", 1 << size, N, rd, u16);

	for (i = 0; i < N; i++) {
		switch (size) {
			case 0:
				if (mvn)
					naja->fvec[rd].b[i] = ~u16;
				else
					naja->fvec[rd].b[i] = u16;
				break;
			case 1:
				if (mvn)
					naja->fvec[rd].w[i] = ~u16;
				else
					naja->fvec[rd].w[i] = u16;
				break;
			case 2:
				if (mvn)
					naja->fvec[rd].l[i] = ~u16;
				else
					naja->fvec[rd].l[i] = u16;
				break;
			case 3:
				if (mvn)
					naja->fvec[rd].q[i] = ~u16;
				else
					naja->fvec[rd].q[i] = u16;
				break;
			default:
				scf_loge("\n");
				return -1;
				break;
		};
	}

	return 0;
}

static int __naja_jmp_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	naja->ip += simm26 << 2;
	NAJA_PRINTF("jmp    %#lx\n", naja->ip);
	return 0;
}

static int __naja_call_disp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	naja->regs[NAJA_REG_LR] = naja->ip + 4;

	naja->ip += simm26 << 2;

	NAJA_PRINTF("call   %#lx\n", naja->ip);
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

		if (0 == (cc & naja->flags))
			naja->ip  += s21;
		else
			naja->ip  += 4;

		if (0 == cc)
			NAJA_PRINTF("jz     %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (1 == cc)
			NAJA_PRINTF("jnz    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (2 == cc)
			NAJA_PRINTF("jge    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (3 == cc)
			NAJA_PRINTF("jgt    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (4 == cc)
			NAJA_PRINTF("jle    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (5 == cc)
			NAJA_PRINTF("jlt    %#lx, flags: %#lx\n", naja->ip, naja->flags);
		else {
			scf_loge("\n");
			return -EINVAL;
		}

	} else {
		int rd = (inst >> 21) & 0x1f;

		if (naja_vm_dynamic_link == (void*)naja->regs[rd]) {

			NAJA_PRINTF("\033[36mjmp    r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

			int ret = naja_vm_dynamic_link(vm);
			if (ret < 0) {
				scf_loge("\n");
				return ret;
			}

			naja->ip = naja->regs[NAJA_REG_LR];

		} else if (naja->regs[rd] < vm->text->addr
				|| naja->regs[rd] > vm->text->addr + vm->text->len) {

			NAJA_PRINTF("\033[36mjmp    r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

			dyn_func_pt f = (void*)naja->regs[rd];

			naja->regs[0] = f(naja->regs[0],
					naja->regs[1],
					naja->regs[2],
					naja->regs[3],
					naja->regs[4],
					naja->regs[5],
					naja->fvec[0].d[0],
					naja->fvec[1].d[0],
					naja->fvec[2].d[0],
					naja->fvec[3].d[0],
					naja->fvec[4].d[0],
					naja->fvec[5].d[0],
					naja->fvec[6].d[0],
					naja->fvec[7].d[0]);

			naja->ip = naja->regs[NAJA_REG_LR];
		} else {
			naja->ip = naja->regs[rd];

			NAJA_PRINTF("jmp    r%d, %#lx\n", rd, naja->regs[rd]);
		}
	}

	return 0;
}

static int __naja_call_reg(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0x1f;

	naja->regs[NAJA_REG_LR]   = naja->ip + 4;

	if (naja_vm_dynamic_link == (void*)naja->regs[rd]) {

		NAJA_PRINTF("\033[36mcall  r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

		int ret = naja_vm_dynamic_link(vm);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

		naja->ip = naja->regs[NAJA_REG_LR];

	} else if (naja->regs[rd] < vm->text->addr
			|| naja->regs[rd] > vm->text->addr + vm->text->len) {

		NAJA_PRINTF("\033[36mcall  r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

		dyn_func_pt f = (void*)naja->regs[rd];

		naja->regs[0]  = f(naja->regs[0],
				naja->regs[1],
				naja->regs[2],
				naja->regs[3],
				naja->regs[4],
				naja->regs[5],
				naja->fvec[0].d[0],
				naja->fvec[1].d[0],
				naja->fvec[2].d[0],
				naja->fvec[3].d[0],
				naja->fvec[4].d[0],
				naja->fvec[5].d[0],
				naja->fvec[6].d[0],
				naja->fvec[7].d[0]);

		naja->ip = naja->regs[NAJA_REG_LR];
	} else {
		NAJA_PRINTF("call  r%d, %#lx\n", rd, naja->regs[rd]);
		naja->ip = naja->regs[rd];
	}

	return 0;
}

static int __naja_setcc(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0x1f;
	int cc = (inst >>  1) & 0xf;

	naja->regs[rd] = 0 == (cc & naja->flags);

	if (SCF_VM_Z == cc)
		NAJA_PRINTF("setz   r%d\n", rd);

	else if (SCF_VM_NZ == cc)
		NAJA_PRINTF("setnz  r%d\n", rd);

	else if (SCF_VM_GE == cc)
		NAJA_PRINTF("setge  r%d\n", rd);

	else if (SCF_VM_GT == cc)
		NAJA_PRINTF("setgt  r%d\n", rd);

	else if (SCF_VM_LT == cc)
		NAJA_PRINTF("setlt  r%d\n", rd);

	else if (SCF_VM_LE == cc)
		NAJA_PRINTF("setle  r%d\n", rd);
	else {
		scf_loge("inst: %#x\n", inst);
		return -EINVAL;
	}

	naja->ip  += 4;
	return 0;
}

static int __naja_adrp(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rd  = (inst >> 21) & 0xf;
	int s21 =  inst & 0x1fffff;

	if (s21  & 0x100000)
		s21 |= ~0x1fffff;

	naja->regs[rd] = (naja->ip + ((int64_t)s21 << 12)) & ~0xfffULL;

	if (naja->regs[rd] >= 0x800000)
		naja->regs[rd]  = naja->regs[rd] - vm->data->addr + (uint64_t)vm->data->data;

	else if (naja->regs[rd] >= 0x600000)
		naja->regs[rd]  = naja->regs[rd] - vm->rodata->addr + (uint64_t)vm->rodata->data;

	else if (naja->regs[rd] >= 0x400000)
		naja->regs[rd]  = naja->regs[rd] - vm->text->addr + (uint64_t)vm->text->data;

	NAJA_PRINTF("adrp   r%d, [rip, %d],  %#lx\n", rd, s21, naja->regs[rd]);

	naja->ip += 4;
	return 0;
}

static int __naja_ret(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	naja->ip   =  naja->regs[NAJA_REG_LR];
	int64_t sp = -naja->regs[NAJA_REG_SP];

	assert (sp >= 0);

	if (naja->size > sp + STACK_INC) {

		void* p = realloc(naja->stack, sp + STACK_INC);
		if (!p) {
			scf_loge("\n");
			return -ENOMEM;
		}

		naja->stack = p;
		naja->size  = sp + STACK_INC;
	}

	NAJA_PRINTF("ret,   %#lx, sp: %ld, stack->size: %ld\n", naja->ip, sp, naja->size);
	return 0;
}

static int __naja_spop(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int sr = (inst >> 21) & 0xf;

	NAJA_PRINTF("spop     sr%d, [%s]\n", sr, __naja_reg[rb]);
	return 0;
}

static int __naja_spush(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int rb =  inst        & 0xf;
	int sr = (inst >> 21) & 0xf;

	NAJA_PRINTF("spush    sr%d, [%s]\n", sr, __naja_reg[rb]);
	return 0;
}

static int __naja_in(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int u16 =  inst        & 0xffff;
	int rd  = (inst >> 21) & 0xf;
	int D   = (inst >> 17) & 0xf;

	if (D)
		NAJA_PRINTF("out      %s, %#x\n", __naja_reg[rd], u16);
	else
		NAJA_PRINTF("in       %s, %#x\n", __naja_reg[rd], u16);
	return 0;
}

static int __naja_smov(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	int sr =  inst        & 0xf;
	int rd = (inst >> 21) & 0xf;
	int D  = (inst >> 17) & 0xf;

	if (D)
		NAJA_PRINTF("smov     sr%d, %s\n", sr, __naja_reg[rd]);
	else
		NAJA_PRINTF("smov     %s, sr%d\n", __naja_reg[rd], sr);
	return 0;
}

static int __naja_iret(scf_vm_t* vm, uint32_t inst)
{
	scf_vm_naja_t* naja = vm->priv;

	NAJA_PRINTF("iret\n");
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

static void __naja_vm_exit()
{
}

static int __naja_vm_run(scf_vm_t* vm, const char* path, const char* sys)
{
	scf_vm_naja_t* naja = vm->priv;
	Elf64_Ehdr     eh;
	Elf64_Shdr     sh;

	fseek(vm->elf->fp, 0, SEEK_SET);

	int ret  = fread(&eh, sizeof(Elf64_Ehdr), 1, vm->elf->fp);
	if (ret != 1)
		return -1;

	if (vm->jmprel) {
		fseek(vm->elf->fp, eh.e_shoff, SEEK_SET);

		int i;
		for (i = 0; i < eh.e_shnum; i++) {

			ret = fread(&sh, sizeof(Elf64_Shdr), 1, vm->elf->fp);
			if (ret != 1)
				return -1;

			if (vm->jmprel_addr == sh.sh_addr) {
				vm->jmprel_size  = sh.sh_size;
				break;
			}
		}

		if (i == eh.e_shnum) {
			scf_loge("\n");
			return -1;
		}
	}

	naja->stack  = calloc(STACK_INC, sizeof(uint64_t));
	if (!naja->stack)
		return -ENOMEM;

	naja->size   = STACK_INC;
	naja->_start = eh.e_entry;
	naja->ip     = eh.e_entry;

	naja->regs[NAJA_REG_LR] = (uint64_t)__naja_vm_exit;

	scf_logw("naja->regs[NAJA_REG_LR]: %#lx\n", naja->regs[NAJA_REG_LR]);

	int n = 0;
	while ((uint64_t)__naja_vm_exit != naja->ip) {

		int64_t offset = naja->ip - vm->text->addr;

		if (offset >= vm->text->len) {
			scf_loge("naja->ip: %#lx, %p\n", naja->ip, __naja_vm_exit);
			return -1;
		}

		uint32_t inst = *(uint32_t*)(vm->text->data + offset);

		naja_opcode_pt pt = naja_opcodes[(inst >> 26) & 0x3f];

		if (!pt) {
			scf_loge("inst: %d, %#x\n", (inst >> 26) & 0x3f, inst);
			return -EINVAL;
		}

		NAJA_PRINTF("%4d, %#lx: ", n++, naja->ip);
		ret = pt(vm, inst);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

//		usleep(10 * 1000);
	}

	scf_logw("r0: %ld, sizeof(fv256_t): %ld\n", naja->regs[0], sizeof(fv256_t));
	return naja->regs[0];
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

scf_vm_ops_t  vm_ops_naja =
{
	.name  = "naja",
	.open  = naja_vm_open,
	.close = naja_vm_close,
	.run   = naja_vm_run,
};
