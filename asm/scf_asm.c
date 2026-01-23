#include"scf_asm.h"
#include"scf_symtab.h"

int  _x64_set_offset_for_jmps(scf_vector_t* text);
int _naja_set_offset_for_jmps(scf_vector_t* text);

int scf_asm_open(scf_asm_t** pasm, const char* arch)
{
	if (!pasm)
		return -EINVAL;

	scf_asm_t* _asm = calloc(1, sizeof(scf_asm_t));
	if (!_asm)
		return -EINVAL;

	_asm->text = scf_vector_alloc();
	if (!_asm->text)
		goto text_error;

	_asm->data = scf_vector_alloc();
	if (!_asm->data)
		goto data_error;

	_asm->text_relas = scf_vector_alloc();
	if (!_asm->text_relas)
		goto text_rela_error;

	_asm->data_relas = scf_vector_alloc();
	if (!_asm->data_relas)
		goto data_rela_error;

	_asm->global = scf_vector_alloc();
	if (!_asm->global)
		goto global_error;

	_asm->labels = scf_vector_alloc();
	if (!_asm->labels)
		goto labels_error;

	_asm->symtab = scf_vector_alloc();
	if (!_asm->symtab)
		goto symtab_error;

	if (scf_asm_dfa_init(_asm, arch) < 0) {
		scf_loge("\n");
		goto dfa_error;
	}

	*pasm = _asm;
	return 0;

dfa_error:
	scf_vector_free(_asm->symtab);
symtab_error:
	scf_vector_free(_asm->labels);
labels_error:
	scf_vector_free(_asm->global);
global_error:
	scf_vector_free(_asm->data_relas);
data_rela_error:
	scf_vector_free(_asm->text_relas);
text_rela_error:
	scf_vector_free(_asm->data);
data_error:
	scf_vector_free(_asm->text);
text_error:
	free(_asm);
	return -1;
}

int scf_asm_close(scf_asm_t* _asm)
{
	if (_asm) {
		free(_asm);
		_asm = NULL;
	}

	return 0;
}

int scf_asm_file(scf_asm_t* _asm, const char* path)
{
	if (!_asm || !path)
		return -EINVAL;

	scf_lex_t* lex = _asm->lex_list;

	while (lex) {
		if (!strcmp(lex->file->data, path))
			break;

		lex = lex->next;
	}

	if (lex) {
		_asm->lex = lex;
		return 0;
	}

	if (scf_lex_open(&_asm->lex, path, NULL) < 0)
		return -1;

	_asm->lex->next = _asm->lex_list;
	_asm->lex_list  = _asm->lex;

	dfa_asm_t*       d = _asm->dfa_data;
	scf_lex_word_t*  w = NULL;

	int ret = 0;

	while (1) {
		ret = scf_lex_pop_word(_asm->lex, &w);
		if (ret < 0) {
			scf_loge("lex pop word failed\n");
			break;
		}

		if (SCF_LEX_WORD_EOF == w->type) {
			scf_logi("eof\n\n");
			scf_lex_push_word(_asm->lex, w);
			w = NULL;
			break;
		}

		if (SCF_LEX_WORD_SPACE == w->type) {
			scf_lex_word_free(w);
			w = NULL;
			continue;
		}

		if (SCF_LEX_WORD_SEMICOLON == w->type) {
			scf_lex_word_free(w);
			w = NULL;
			continue;
		}

		ret = scf_dfa_parse_word(_asm->dfa, w, d);
		if (ret < 0)
			break;
	}

	fclose(_asm->lex->fp);
	_asm->lex->fp = NULL;
	return ret;
}

int scf_asm_len(scf_vector_t* instructions)
{
	scf_instruction_t* inst;
	int offset = 0;
	int i;

	for (i = 0; i < instructions->size; i++) {
		inst      = instructions->data[i];

		if (inst->align > 0) {
			int n = offset & (inst->align - 1);
			if (n > 0)
				offset += inst->align - n;
		}

		if (inst->org > 0) {
			if (offset > inst->org) {
				scf_loge(".org %#x less than .text length %#x\n", inst->org, offset);
				return -1;
			}

			offset = inst->org;
		}

		inst->offset = offset;

		if (inst->len > 0)
			offset += inst->len;
		else if (inst->bin)
			offset += inst->bin->len;
	}

	return offset;
}

static int __asm_add_text(scf_elf_context_t* elf, scf_asm_t* _asm)
{
	scf_instruction_t* inst;
	scf_string_t*      text;
	int ret;
	int i;

	switch (elf->ops->arch)
	{
		case SCF_ELF_X64:
			ret = _x64_set_offset_for_jmps(_asm->text);
			break;

		case SCF_ELF_NAJA:
			ret = _naja_set_offset_for_jmps(_asm->text);
			break;
		default:
			scf_loge("%s NOT support\n", elf->ops->machine);
			break;
	};
	if (ret < 0)
		return ret;

	text = scf_string_alloc();
	if (!text)
		return -ENOMEM;

	for (i = 0; i < _asm->text->size; i++) {
		inst      = _asm->text->data[i];

		int n = inst->offset - text->len;
		assert(n >= 0);

		if (n > 0) {
			ret = scf_string_fill_zero(text, n);
			if (ret < 0) {
				scf_string_free(text);
				return ret;
			}
		}

		if (inst->len > 0)
			ret = scf_string_cat_cstr_len(text, inst->code, inst->len);

		else if (inst->bin)
			ret = scf_string_cat_cstr_len(text, inst->bin->data, inst->bin->len);
		else
			continue;

		if (ret < 0) {
			scf_string_free(text);
			return ret;
		}
	}

	if (text->len & 0x7) {
		size_t n = 8 - (text->len & 0x7);

		ret = scf_string_fill_zero(text, n);
		if (ret < 0) {
			scf_string_free(text);
			return ret;
		}
	}

	int end  = text->len;
	for (i   = _asm->text->size - 1; i >= 0; i--) {
		inst = _asm->text->data[i];

		if (inst->label && SCF_LEX_WORD_ID == inst->label->type) {

			ret = scf_symtab_add_sym(_asm->symtab, inst->label->text->data, end - inst->offset, inst->offset, ASM_SHNDX_TEXT, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC));
			if (ret < 0) {
				scf_string_free(text);
				return ret;
			}

			end = inst->offset;
		}
	}

	scf_elf_section_t  cs = {
		.name         = ".text",
		.sh_type      = SHT_PROGBITS,
		.sh_flags     = SHF_ALLOC | SHF_EXECINSTR,
		.sh_addralign = 1,
		.data         = text->data,
		.data_len     = text->len,
		.index        = ASM_SHNDX_TEXT,
	};

	ret = scf_elf_add_section(elf, &cs);

	scf_string_free(text);
	return ret;
}

static int __asm_add_data(scf_elf_context_t* elf, scf_asm_t* _asm)
{
	scf_instruction_t* inst;
	scf_string_t*      data;
	int ret;
	int i;

	data = scf_string_alloc();
	if (!data)
		return -ENOMEM;

	for (i = 0; i < _asm->data->size; i++) {
		inst      = _asm->data->data[i];

		inst->offset = data->len;

		if (inst->len > 0)
			ret = scf_string_cat_cstr_len(data, inst->code, inst->len);

		else if (inst->bin)
			ret = scf_string_cat_cstr_len(data, inst->bin->data, inst->bin->len);
		else
			continue;

		if (ret < 0) {
			scf_string_free(data);
			return ret;
		}
	}

	int end  = data->len;
	for (i   = _asm->data->size - 1; i >= 0; i--) {
		inst = _asm->data->data[i];

		if (inst->label && SCF_LEX_WORD_ID == inst->label->type) {

			ret = scf_symtab_add_sym(_asm->symtab, inst->label->text->data, end - inst->offset, inst->offset, ASM_SHNDX_DATA, ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT));
			if (ret < 0) {
				scf_string_free(data);
				return ret;
			}

			end = inst->offset;
		}
	}

	scf_elf_section_t  ds = {
		.name         = ".data",
		.sh_type      = SHT_PROGBITS,
		.sh_flags     = SHF_ALLOC | SHF_WRITE,
		.sh_addralign = 8,
		.data         = data->data,
		.data_len     = data->len,
		.index        = ASM_SHNDX_DATA,
	};

	ret = scf_elf_add_section(elf, &ds);

	scf_string_free(data);
	return ret;
}

static int __asm_add_rela_text(scf_elf_context_t* elf, scf_asm_t* _asm)
{
	scf_instruction_t* inst;
	scf_vector_t*      relas;
	scf_rela_t*        r;
	int ret;
	int i;

	relas = scf_vector_alloc();
	if (!relas)
		return -ENOMEM;

	for (i = 0; i < _asm->text_relas->size; i++) {
		r  =        _asm->text_relas->data[i];

		r->text_offset = r->inst->offset + r->inst_offset;

		if (scf_vector_find(_asm->text, inst))
			ret = scf_symtab_add_rela(relas, _asm->symtab, r, r->name->data, ASM_SHNDX_TEXT);
		else
			ret = scf_symtab_add_rela(relas, _asm->symtab, r, r->name->data, 0);

		if (ret < 0) {
			scf_loge("\n");
			goto error;
		}
	}

	ret = 0;
	if (relas->size > 0) {
		scf_elf_section_t s = {0};

		s.name         = ".rela.text";
		s.sh_type      = SHT_RELA;
		s.sh_flags     = SHF_INFO_LINK;
		s.sh_addralign = 8;
		s.data         = NULL;
		s.data_len     = 0;
		s.sh_link      = 0;
		s.sh_info      = ASM_SHNDX_TEXT;

		ret = scf_elf_add_rela_section(elf, &s, relas);
	}
error:
	scf_vector_clear(relas, (void (*)(void*) ) free);
	scf_vector_free (relas);
	return ret;
}

static int __asm_add_rela_data(scf_elf_context_t* elf, scf_asm_t* _asm)
{
	scf_instruction_t* inst;
	scf_vector_t*      relas;
	scf_rela_t*        r;
	int ret;
	int i;

	relas = scf_vector_alloc();
	if (!relas)
		return -ENOMEM;

	for (i = 0; i < _asm->data_relas->size; i++) {
		r  =        _asm->data_relas->data[i];

		r->text_offset = r->inst->offset + r->inst_offset;

		if (scf_vector_find(_asm->data, inst))
			ret = scf_symtab_add_rela(relas, _asm->symtab, r, r->name->data, ASM_SHNDX_DATA);
		else
			ret = scf_symtab_add_rela(relas, _asm->symtab, r, r->name->data, 0);

		if (ret < 0) {
			scf_loge("\n");
			goto error;
		}
	}

	ret = 0;
	if (relas->size > 0) {
		scf_elf_section_t s = {0};

		s.name         = ".rela.data";
		s.sh_type      = SHT_RELA;
		s.sh_flags     = SHF_INFO_LINK;
		s.sh_addralign = 8;
		s.data         = NULL;
		s.data_len     = 0;
		s.sh_link      = 0;
		s.sh_info      = ASM_SHNDX_DATA;

		ret = scf_elf_add_rela_section(elf, &s, relas);
	}
error:
	scf_vector_clear(relas, (void (*)(void*) ) free);
	scf_vector_free (relas);
	return ret;
}

int scf_asm_to_obj(scf_asm_t* _asm, const char* obj, const char* arch)
{
	scf_lex_t* lex = _asm->lex_list;

	while (lex) {
		int ret = scf_symtab_add_sym(_asm->symtab, lex->file->data, 0, 0, SHN_ABS, ELF64_ST_INFO(STB_LOCAL, STT_FILE));
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

		lex = lex->next;
	}

	ADD_SECTION_SYMBOL(_asm->symtab, ASM_SHNDX_TEXT, ".text");
	ADD_SECTION_SYMBOL(_asm->symtab, ASM_SHNDX_DATA, ".data");

	scf_elf_context_t*  elf = NULL;

	int ret = scf_elf_open(&elf, arch, obj, "wb");
	if (ret < 0) {
		scf_loge("open '%s' elf file '%s' failed\n", arch, obj);
		return ret;
	}

	ret = __asm_add_text(elf, _asm);
	if (ret < 0)
		goto error;

	ret = __asm_add_data(elf, _asm);
	if (ret < 0)
		goto error;

	qsort(_asm->symtab->data, _asm->symtab->size, sizeof(void*), __symtab_sort_cmp);

	ret = __asm_add_rela_text(elf, _asm);
	if (ret < 0)
		goto error;

	ret = __asm_add_rela_data(elf, _asm);
	if (ret < 0)
		goto error;

	ret = scf_elf_add_syms(elf, _asm->symtab, ".symtab");
	if (ret < 0)
		goto error;

	ret = scf_elf_write_rel(elf);
error:
	scf_elf_close(elf);
	return ret;
}
