#include"scf_symtab.h"

static int _find_sym(const void* v0, const void* v1)
{
	const char*          name = v0;
	const scf_elf_sym_t* sym  = v1;

	if (!sym->name)
		return -1;

	return strcmp(name, sym->name);
}

int scf_symtab_add_sym(scf_vector_t* symtab, const char* name,
		uint64_t   st_size,
		Elf64_Addr st_value,
		uint16_t   st_shndx,
		uint8_t    st_info)
{
	scf_elf_sym_t* sym = NULL;

	if (name)
		sym = scf_vector_find_cmp(symtab, name, _find_sym);

	if (!sym) {
		sym = calloc(1, sizeof(scf_elf_sym_t));
		if (!sym)
			return -ENOMEM;

		if (name) {
			sym->name = strdup(name);
			if (!sym->name) {
				free(sym);
				return -ENOMEM;
			}
		}

		sym->st_size  = st_size;
		sym->st_value = st_value;
		sym->st_shndx = st_shndx;
		sym->st_info  = st_info;

		int ret = scf_vector_add(symtab, sym);
		if (ret < 0) {
			if (sym->name)
				free(sym->name);
			free(sym);
			scf_loge("\n");
			return ret;
		}
	}

	return 0;
}

int scf_symtab_add_rela(scf_vector_t* relas, scf_vector_t* symtab, scf_rela_t* r, const char* name, uint16_t st_shndx)
{
	scf_elf_rela_t* rela;
	scf_elf_sym_t*  sym;

	int ret;
	int i;

	for (i = 0; i < symtab->size; i++) {
		sym       = symtab->data[i];

		if (!sym->name)
			continue;

		if (!strcmp(name, sym->name))
			break;
	}

	if (i == symtab->size) {
		ret = scf_symtab_add_sym(symtab, name, 0, 0, st_shndx, ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE));
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}

	scf_logd("rela: %s, offset: %ld\n", name, r->text_offset);

	rela = calloc(1, sizeof(scf_elf_rela_t));
	if (!rela)
		return -ENOMEM;

	rela->name     = (char*)name;
	rela->r_offset = r->text_offset;
	rela->r_info   = ELF64_R_INFO(i + 1, r->type);
	rela->r_addend = r->addend;

	ret = scf_vector_add(relas, rela);
	if (ret < 0) {
		free(rela);
		return ret;
	}

	return 0;
}
