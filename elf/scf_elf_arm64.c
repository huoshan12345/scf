#include"scf_elf_arm64.h"
#include"scf_elf_link.h"

static int _arm64_elf_write_rel(scf_elf_context_t* elf)
{
	return elf_write_rel(elf, EM_AARCH64);
}

static int _arm64_elf_link_cs(elf_native_t* arm64, elf_section_t* s, elf_section_t* rs, uint64_t cs_base)
{
	elf_sym_t*  sym;
	Elf64_Rela*         rela;

	assert(rs->data_len % sizeof(Elf64_Rela) == 0);

	int i;
	for (i   = 0; i < rs->data_len; i += sizeof(Elf64_Rela)) {

		rela = (Elf64_Rela* )(rs->data + i);
		sym  = NULL;

		int sym_idx = ELF64_R_SYM(rela->r_info);

		assert(sym_idx >= 1);
		assert(sym_idx -  1 < arm64->symbols->size);

		sym = arm64->symbols->data[sym_idx - 1];
		if (sym->dyn_flag) {
			scf_loge("sym '%s' in dynamic so\n", sym->name->data);
			continue;
		}

		int j = elf_find_sym(&sym, rela, arm64->symbols);
		if (j < 0)
			return -1;

		int32_t offset = sym->sym.st_value - (cs_base + rela->r_offset) + rela->r_addend;

		rela->r_info = ELF64_R_INFO(j, ELF64_R_TYPE(rela->r_info));

		switch (ELF64_R_TYPE(rela->r_info)) {

			case R_AARCH64_CALL26:

				assert(0 == (offset & 0x3));

				offset >>= 2;

				if (offset > 0x1ffffff || offset < -0x1ffffff) {
					scf_loge("\n");
					return -EINVAL;
				}

				offset &= 0x3ffffff;

				scf_logd("sym: %s, offset: %#x, %#lx\n", sym->name->data, offset, rela->r_offset);

				*(uint32_t*)(s->data + rela->r_offset) |= offset;
				break;

			case R_AARCH64_ADR_PREL_PG_HI21:

				offset >>= 12;
				offset   = ((offset & 0x3) << 29) | (((offset >> 2) & 0x7ffff) << 5);

				*(uint32_t*)(s->data + rela->r_offset) |= offset;
				break;

			case R_AARCH64_ADD_ABS_LO12_NC:

				*(uint32_t*)(s->data + rela->r_offset) |= (sym->sym.st_value & 0xfff) << 10;
				break;

			default:
				scf_loge("\n");
				return -EINVAL;
				break;
		};
	}

	return 0;
}

static int _arm64_elf_link_ds(elf_native_t* arm64, elf_section_t* s, elf_section_t* rs)
{
	elf_sym_t*  sym;
	Elf64_Rela*         rela;

	assert(rs->data_len % sizeof(Elf64_Rela) == 0);

	int i;
	for (i   = 0; i < rs->data_len; i += sizeof(Elf64_Rela)) {

		rela = (Elf64_Rela* )(rs->data + i);
		sym  = NULL;

		int j = elf_find_sym(&sym, rela, arm64->symbols);
		if (j < 0)
			return -1;

		assert(ELF64_R_TYPE(rela->r_info) == R_X86_64_64
			|| ELF64_R_TYPE(rela->r_info) == R_X86_64_32);

		uint64_t offset = sym->sym.st_value + rela->r_addend;

		rela->r_info    = ELF64_R_INFO(j, ELF64_R_TYPE(rela->r_info));

		switch (ELF64_R_TYPE(rela->r_info)) {

			case R_AARCH64_ABS64:
				memcpy(s->data + rela->r_offset, &offset, 8);
				break;

			case R_AARCH64_ABS32:
				memcpy(s->data + rela->r_offset, &offset, 4);
				break;
			default:
				scf_loge("\n");
				return -EINVAL;
				break;
		};
	}

	return 0;
}

static int _arm64_elf_link_debug(elf_native_t* arm64, elf_section_t* s, elf_section_t* rs)
{
	elf_sym_t*  sym;
	elf_sym_t*  sym2;
	Elf64_Rela*         rela;

	assert(rs->data_len % sizeof(Elf64_Rela) == 0);

	int i;
	for (i   = 0; i < rs->data_len; i += sizeof(Elf64_Rela)) {

		rela = (Elf64_Rela* )(rs->data + i);
		sym  = NULL;

		int j = elf_find_sym(&sym, rela, arm64->symbols);
		if (j < 0)
			return -1;

		uint64_t offset = sym->sym.st_value + rela->r_addend;

		if (!strncmp(sym->name->data, ".debug_", 7)) {

			int k  = ELF64_R_SYM(rela->r_info);

			sym2   = arm64->symbols->data[k - 1];

			offset = sym2->sym.st_value + rela->r_addend;
			rela->r_addend = offset;

		} else if (!strcmp(sym->name->data, ".text")) {

			int k  = ELF64_R_SYM(rela->r_info);

			sym2   = arm64->symbols->data[k - 1];

			offset = sym2->sym.st_value;
			rela->r_addend = sym2->sym.st_value - sym->sym.st_value;
		}

		rela->r_info = ELF64_R_INFO(j, ELF64_R_TYPE(rela->r_info));

		switch (ELF64_R_TYPE(rela->r_info)) {

			case R_AARCH64_ABS64:
				memcpy(s->data + rela->r_offset, &offset, 8);
				break;

			case R_AARCH64_ABS32:
				memcpy(s->data + rela->r_offset, &offset, 4);
				break;
			default:
				scf_loge("\n");
				return -EINVAL;
				break;
		};
	}

	return 0;
}

static int _arm64_elf_link_sections(elf_native_t* arm64, uint32_t cs_index, uint32_t ds_index)
{
	elf_section_t* s;
	elf_section_t* rs;

	int i;
	for (i = 0; i < arm64->sections->size; i++) {
		rs =        arm64->sections->data[i];

		if (SHT_RELA != rs->sh.sh_type)
			continue;

		assert(rs->sh.sh_info < arm64->sections->size);

		if (cs_index == rs->sh.sh_info
				|| ds_index == rs->sh.sh_info)
			continue;

		if (!strcmp(rs->name->data, ".rela.plt"))
			continue;

		s = arm64->sections->data[rs->sh.sh_info - 1];

		scf_logd("s: %s, rs: %s, rs->sh.sh_info: %u\n", s->name->data, rs->name->data, rs->sh.sh_info);

		assert(!strcmp(s->name->data, rs->name->data + 5));

		if (_arm64_elf_link_debug(arm64, s, rs) < 0) {
			scf_loge("\n");
			return -1;
		}
	}

	return 0;
}

static int _arm64_elf_write_exec(scf_elf_context_t* elf, const char* sysroot)
{
	elf_native_t* arm64    = elf->priv;
	int 		  nb_phdrs = 3;

	if (arm64->dynsyms && arm64->dynsyms->size) {
		__arm64_elf_add_dyn(arm64, sysroot);
		nb_phdrs = 6;
	}

	int 		   nb_sections      = 1 + arm64->sections->size + 1 + 1 + 1;
	uint64_t	   shstrtab_offset  = 1;
	uint64_t	   strtab_offset    = 1;
	uint64_t	   dynstr_offset    = 1;
	Elf64_Off      phdr_offset      = sizeof(arm64->eh) + sizeof(Elf64_Shdr) * nb_sections;
	Elf64_Off      section_offset   = phdr_offset     + sizeof(Elf64_Phdr) * nb_phdrs;

	elf_section_t* s;
	elf_section_t* cs    = NULL;
	elf_section_t* ros   = NULL;
	elf_section_t* ds    = NULL;
	elf_section_t* crela = NULL;
	elf_section_t* drela = NULL;
	elf_sym_t*     sym;

	int i;
	for (i = 0; i < arm64->sections->size; i++) {
		s  =        arm64->sections->data[i];

		if (!strcmp(".text", s->name->data)) {

			assert(s->data_len > 0);
			assert(!cs);
			cs = s;

		} else if (!strcmp(".rodata", s->name->data)) {

			assert(s->data_len >= 0);
			assert(!ros);
			ros = s;

		} else if (!strcmp(".data", s->name->data)) {

			assert(s->data_len >= 0);
			assert(!ds);
			ds = s;

		} else if (!strcmp(".rela.text", s->name->data)) {

			assert(!crela);
			crela = s;

		} else if (!strcmp(".rela.data", s->name->data)) {

			assert(!drela);
			drela = s;
		}

		s->offset        = section_offset;
		section_offset  += s->data_len;
	}
	assert(crela);

	uint64_t cs_align  = (cs ->offset + cs ->data_len + 0x200000 - 1) >> 21 << 21;
	uint64_t ro_align  = (ros->offset + ros->data_len + 0x200000 - 1) >> 21 << 21;

	uint64_t rx_base   = 0x400000;
	uint64_t r_base    = 0x400000 + cs_align;
	uint64_t rw_base   = 0x400000 + cs_align + ro_align;

	uint64_t cs_base   = cs->offset  + rx_base;
	uint64_t ro_base   = ros->offset + r_base;
	uint64_t ds_base   = ds->offset  + rw_base;
	uint64_t _start    =  0;

	for (i  = 0; i < arm64->symbols->size; i++) {
		sym =        arm64->symbols->data[i];

		uint32_t shndx = sym->sym.st_shndx;

		if (shndx == cs->index)
			sym->sym.st_value += cs_base;

		else if (shndx == ros->index)
			sym->sym.st_value += ro_base;

		else if (shndx == ds->index)
			sym->sym.st_value += ds_base;

		scf_logd("sym: %s, %#lx, st_shndx: %d\n", sym->name->data, sym->sym.st_value, sym->sym.st_shndx);
	}

	int ret = _arm64_elf_link_cs(arm64, cs, crela, cs_base);
	if (ret < 0) {
		scf_loge("ret: %d\n", ret);
		return ret;
	}

	if (drela) {
		ret = _arm64_elf_link_ds(arm64, ds, drela);
		if (ret < 0)
			return ret;
	}

	ret = _arm64_elf_link_sections(arm64, cs->index, ds->index);
	if (ret < 0)
		return ret;

	elf_process_syms(arm64, cs->index);

	cs ->sh.sh_addr = cs_base;
	ds ->sh.sh_addr = ds_base;
	ros->sh.sh_addr = ro_base;


	if (6 == nb_phdrs) {
		__arm64_elf_post_dyn(arm64, rx_base, rw_base, cs);
	}

	for (i  = 0; i < arm64->symbols->size; i++) {
		sym =        arm64->symbols->data[i];

		if (!strcmp(sym->name->data, "_start")) {

			if (0 != _start) {
				scf_loge("\n");
				return -EINVAL;
			}

			_start = sym->sym.st_value;
			break;
		}
	}

	// write elf header
	elf_header(&arm64->eh, ET_EXEC, EM_AARCH64, _start, phdr_offset, nb_phdrs, nb_sections, nb_sections - 1);
	fwrite(&arm64->eh, sizeof(arm64->eh), 1, elf->fp);

	// write null section header
	fwrite(&arm64->sh_null, sizeof(arm64->sh_null), 1, elf->fp);

	// write user's section header
	section_offset   = phdr_offset + sizeof(Elf64_Phdr) * nb_phdrs;

	for (i = 0; i < arm64->sections->size; i++) {
		s  =        arm64->sections->data[i];

		if (SHT_RELA == s->sh.sh_type && 0 == s->sh.sh_link)
			s->sh.sh_link = nb_sections - 3;

		section_header(&s->sh, shstrtab_offset, s->sh.sh_addr,
				section_offset, s->data_len,
				s->sh.sh_link,  s->sh.sh_info, s->sh.sh_entsize);

		if (SHT_STRTAB != s->sh.sh_type)
			s->sh.sh_addralign = 8;

		section_offset  += s->data_len;
		shstrtab_offset += s->name->len + 1;

		fwrite(&s->sh, sizeof(s->sh), 1, elf->fp);
	}

	// set user's symbols' name
	int  nb_local_syms = 1;

	for (i  = 0; i < arm64->symbols->size; i++) {
		sym =        arm64->symbols->data[i];

		if (sym->name) {
			sym->sym.st_name = strtab_offset;
			strtab_offset	 += sym->name->len + 1;
		} else
			sym->sym.st_name = 0;

		if (STB_LOCAL == ELF64_ST_BIND(sym->sym.st_info))
			nb_local_syms++;
	}

	// write symtab section header
	section_header(&arm64->sh_symtab, shstrtab_offset, 0,
			section_offset, (arm64->symbols->size + 1) * sizeof(Elf64_Sym),
			nb_sections - 2, nb_local_syms, sizeof(Elf64_Sym));

	fwrite(&arm64->sh_symtab, sizeof(arm64->sh_symtab), 1, elf->fp);

	section_offset   += (arm64->symbols->size + 1) * sizeof(Elf64_Sym);
	shstrtab_offset  += strlen(".symtab") + 1;

	// write strtab section header
	section_header(&arm64->sh_strtab, shstrtab_offset, 0,
			section_offset, strtab_offset,
			0, 0, 0);
	fwrite(&arm64->sh_strtab, sizeof(arm64->sh_strtab), 1, elf->fp);
	section_offset   += strtab_offset;
	shstrtab_offset  += strlen(".strtab") + 1;

	// write shstrtab section header
	uint64_t shstrtab_len = shstrtab_offset + strlen(".shstrtab") + 1;
	section_header(&arm64->sh_shstrtab, shstrtab_offset, 0,
			section_offset, shstrtab_len, 0, 0, 0);
	fwrite(&arm64->sh_shstrtab, sizeof(arm64->sh_shstrtab), 1, elf->fp);

#if 1
	if (6 == nb_phdrs) {
		__arm64_elf_write_phdr(elf, rx_base, phdr_offset, nb_phdrs);

		__arm64_elf_write_interp(elf, rx_base, arm64->interp->offset, arm64->interp->data_len);
	}

	__arm64_elf_write_text  (elf, rx_base, 0,           cs->offset + cs->data_len);
	__arm64_elf_write_rodata(elf, r_base,  ros->offset, ros->data_len);

	if (6 == nb_phdrs) {
		__arm64_elf_write_data(elf, rw_base, arm64->dynamic->offset,
				arm64->dynamic->data_len + arm64->got_plt->data_len + ds->data_len);

		__arm64_elf_write_dynamic(elf, rw_base, arm64->dynamic->offset, arm64->dynamic->data_len);
	} else {
		__arm64_elf_write_data(elf, rw_base, ds->offset, ds->data_len);
	}
#endif

	elf_write_sections(elf);
	elf_write_symtab  (elf);
	elf_write_strtab  (elf);
	elf_write_shstrtab(elf);
	return 0;
}

scf_elf_ops_t	elf_ops_arm64 =
{
	.machine	      = "arm64",

	.open		      = elf_open,
	.close		      = elf_close,

	.add_sym	      = elf_add_sym,
	.add_section      = elf_add_section,

	.add_rela_section = elf_add_rela_section,

	.add_dyn_need     = elf_add_dyn_need,
	.add_dyn_rela     = elf_add_dyn_rela,

	.read_syms        = elf_read_syms,
	.read_relas       = elf_read_relas,
	.read_section     = elf_read_section,

	.write_rel	      = _arm64_elf_write_rel,
	.write_exec       = _arm64_elf_write_exec,
};
