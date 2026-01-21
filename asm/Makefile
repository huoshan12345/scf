CFILES += ../util/scf_string.c
CFILES += ../lex/scf_lex.c
CFILES += ../lex/scf_lex_util.c

CFILES += main.c
CFILES += scf_asm.c
CFILES += scf_dfa_asm.c
CFILES += scf_dfa_x64.c
CFILES += scf_dfa_naja.c

CFILES += ../core/scf_lex_word.c
CFILES += ../parse/scf_dfa.c
CFILES += ../parse/scf_symtab.c

CFILES += ../native/scf_instruction.c
CFILES += ../native/x64/scf_x64_opcode.c
CFILES += ../native/x64/scf_x64_reg_util.c
CFILES += ../native/x64/scf_x64_inst_util.c
CFILES += ../native/risc/scf_risc_opcode.c

CFILES += ../elf/scf_elf.c
CFILES += ../elf/scf_elf_link.c
CFILES += ../elf/scf_elf_native.c
CFILES += ../elf/scf_elf_native32.c
CFILES += ../elf/scf_elf_x64.c
CFILES += ../elf/scf_elf_x64_so.c
CFILES += ../elf/scf_elf_arm64.c
CFILES += ../elf/scf_elf_arm64_so.c
CFILES += ../elf/scf_elf_arm32.c
CFILES += ../elf/scf_elf_arm32_so.c
CFILES += ../elf/scf_elf_naja.c
CFILES += ../elf/scf_elf_naja_so.c
CFILES += ../elf/scf_dwarf.c
CFILES += ../elf/scf_dwarf_abbrev.c
CFILES += ../elf/scf_dwarf_info.c
CFILES += ../elf/scf_dwarf_line.c

CFLAGS += -g
#CFLAGS += -Wall 
CFLAGS += -I../util
CFLAGS += -I../lex
CFLAGS += -I../parse
CFLAGS += -I../core
CFLAGS += -I../elf
CFLAGS += -I../native
CFLAGS += -I../native/x64
CFLAGS += -I../native/risc

LDFLAGS += -ldl -lm

all:
	gcc $(CFLAGS) $(CFILES) $(LDFLAGS) -o sasm

clean:
	rm *.o
