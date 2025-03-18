PROJECT(scf LANGUAGES C)
SET(BUILD_TARGET_NAME ${PROJECT_NAME})
cmake_minimum_required(VERSION 3.30)

add_executable(${BUILD_TARGET_NAME}
        util/scf_string.c
        util/scf_graph.c
        lex/scf_lex.c
        lex/scf_lex_util.c

        parse/scf_parse_util.c
        parse/scf_parse.c
        parse/main.c
        parse/scf_operator_handler_semantic.c
        parse/scf_operator_handler_expr.c
        parse/scf_operator_handler_const.c

        native/scf_native.c
        native/x64/scf_x64.c
        native/x64/scf_x64_inst.c
        native/x64/scf_x64_inst_util.c
        native/x64/scf_x64_inst_shift.c
        native/x64/scf_x64_inst_common.c
        native/x64/scf_x64_inst_binary.c
        native/x64/scf_x64_inst_mul.c
        native/x64/scf_x64_inst_div.c
        native/x64/scf_x64_inst_cmp.c
        native/x64/scf_x64_inst_unary.c

        native/x64/scf_x64_rcg.c
        native/x64/scf_x64_graph.c
        native/x64/scf_x64_reg.c
        native/x64/scf_x64_opcode.c
        native/x64/scf_x64_peephole.c
        native/x64/scf_x64_bb_color.c

        native/risc/scf_risc_bb_color.c
        native/risc/scf_risc.c
        native/risc/scf_risc_graph.c
        native/risc/scf_risc_inst.c
        native/risc/scf_risc_opcode.c
        native/risc/scf_risc_rcg.c
        native/risc/scf_risc_reg.c
        native/risc/scf_risc_reg_arm64.c
        native/risc/scf_risc_reg_arm32.c
        native/risc/scf_risc_reg_naja.c
        native/risc/scf_arm64.c
        native/risc/scf_arm32.c
        native/risc/scf_naja.c

        native/eda/scf_eda.c
        native/eda/scf_eda_inst.c
        native/eda/scf_eda_pack.c
        pack/scf_pack.c

        elf/scf_elf.c
        elf/scf_elf_link.c
        elf/scf_elf_native.c
        elf/scf_elf_native32.c
        elf/scf_elf_x64.c
        elf/scf_elf_x64_so.c
        elf/scf_elf_arm64.c
        elf/scf_elf_arm64_so.c
        elf/scf_elf_arm32.c
        elf/scf_elf_arm32_so.c
        elf/scf_elf_naja.c
        elf/scf_elf_naja_so.c
        elf/scf_dwarf.c
        elf/scf_dwarf_abbrev.c
        elf/scf_dwarf_info.c
        elf/scf_dwarf_line.c

        core/scf_lex_word.c
        core/scf_type.c
        core/scf_variable.c

        core/scf_node.c
        core/scf_label.c

        core/scf_expr.c
        core/scf_block.c
        core/scf_scope.c
        core/scf_function.c

        core/scf_operator.c

        core/scf_type_cast.c
        core/scf_type_cast_i8.c
        core/scf_type_cast_i16.c
        core/scf_type_cast_i32.c
        core/scf_type_cast_i64.c
        core/scf_type_cast_u8.c
        core/scf_type_cast_u16.c
        core/scf_type_cast_u32.c
        core/scf_type_cast_u64.c
        core/scf_type_cast_float.c
        core/scf_type_cast_double.c

        core/scf_operator_handler_3ac.c

        core/scf_dag.c
        core/scf_operator_dag.c
        core/scf_basic_block.c
        core/scf_3ac.c

        core/scf_pointer_alias.c

        core/scf_optimizer.c
        core/scf_optimizer_dag.c
        core/scf_optimizer_inline.c
        core/scf_optimizer_split_call.c
        core/scf_optimizer_call.c
        core/scf_optimizer_common_expr.c
        core/scf_optimizer_pointer_alias.c
        core/scf_optimizer_active_vars.c
        core/scf_optimizer_pointer_aliases.c
        core/scf_optimizer_loads_saves.c
        core/scf_optimizer_auto_gc_find.c
        core/scf_optimizer_auto_gc.c
        core/scf_optimizer_dominators.c
        core/scf_optimizer_basic_block.c
        core/scf_optimizer_const_teq.c
        core/scf_optimizer_loop.c
        core/scf_optimizer_vla.c
        core/scf_optimizer_group.c
        core/scf_optimizer_generate_loads_saves.c

        core/scf_ast.c
        core/scf_calculate.c

        parse/scf_dfa.c
        parse/scf_dfa_parse.c

        parse/scf_dfa_macro.c
        parse/scf_dfa_include.c

        parse/scf_dfa_call.c
        parse/scf_dfa_create.c
        parse/scf_dfa_sizeof.c
        parse/scf_dfa_container.c
        parse/scf_dfa_va_arg.c
        parse/scf_dfa_expr.c

        parse/scf_dfa_enum.c
        parse/scf_dfa_union.c
        parse/scf_dfa_class.c

        parse/scf_dfa_type.c
        parse/scf_dfa_identity.c

        parse/scf_dfa_function.c
        parse/scf_dfa_operator.c
        parse/scf_dfa_var.c

        parse/scf_dfa_init_data.c
        parse/scf_struct_array.c

        parse/scf_dfa_if.c
        parse/scf_dfa_while.c
        parse/scf_dfa_do.c
        parse/scf_dfa_for.c
        parse/scf_dfa_switch.c

        parse/scf_dfa_break.c
        parse/scf_dfa_continue.c
        parse/scf_dfa_goto.c
        parse/scf_dfa_return.c
        parse/scf_dfa_label.c
        parse/scf_dfa_async.c

        parse/scf_dfa_block.c)

target_include_directories(${BUILD_TARGET_NAME} PRIVATE util)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE core)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE lex)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE parse)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE pack)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE elf)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE vm)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE native)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE native/x64)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE native/risc)
target_include_directories(${BUILD_TARGET_NAME} PRIVATE native/eda)

set(CMAKE_BUILD_TYPE Debug)
SET(GCC_COVERAGE_LINK_FLAGS "-ldl")
