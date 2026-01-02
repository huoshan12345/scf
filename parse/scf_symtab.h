#ifndef SCF_SYMTAB_H
#define SCF_SYMTAB_H

#include"scf_elf.h"
#include"scf_instruction.h"

int scf_symtab_add_sym(scf_vector_t* symtab, const char* name,
		uint64_t   st_size,
		Elf64_Addr st_value,
		uint16_t   st_shndx,
		uint8_t    st_info);

int scf_symtab_add_rela(scf_vector_t* relas, scf_vector_t* symtab, scf_rela_t* r, const char* name, uint16_t st_shndx);

#define ADD_SECTION_SYMBOL(symtab, sh_index, sh_name) \
	do { \
		int ret = scf_symtab_add_sym(symtab, sh_name, 0, 0, sh_index, ELF64_ST_INFO(STB_LOCAL, STT_SECTION)); \
		if (ret < 0) { \
			scf_loge("\n"); \
			return ret; \
		} \
	} while (0)

static int __symtab_sort_cmp(const void* v0, const void* v1)
{
	const scf_elf_sym_t* sym0 = *(const scf_elf_sym_t**)v0;
	const scf_elf_sym_t* sym1 = *(const scf_elf_sym_t**)v1;

	if (STB_LOCAL == ELF64_ST_BIND(sym0->st_info)) {
		if (STB_GLOBAL == ELF64_ST_BIND(sym1->st_info))
			return -1;
	} else if (STB_LOCAL == ELF64_ST_BIND(sym1->st_info))
		return 1;
	return 0;
}

#endif
