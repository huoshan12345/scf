cmake_minimum_required(VERSION 3.16)

project(scf_lex_test C)

# 使用 C11（可改）
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Debug 信息（等价 -g）
set(CMAKE_BUILD_TYPE Debug)

# =========================
# 源文件
# =========================

set(CFILES
        ../util/scf_string.c
        scf_lex.c
        scf_lex_util.c
        ../core/scf_lex_word.c
        scf_lex_test.c
)

# =========================
# 可执行文件
# =========================

add_executable(scf_lex_test ${CFILES})

# =========================
# 头文件目录 (-I)
# =========================

target_include_directories(scf_lex_test PRIVATE
        ../util
        ../core
)
