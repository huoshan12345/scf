cmake_minimum_required(VERSION 3.30)
project(scf)

set(CMAKE_CXX_STANDARD 20)

add_executable(scf vm/main.c)

target_include_directories(scf PRIVATE util)
target_include_directories(scf PRIVATE core)
target_include_directories(scf PRIVATE lex)
target_include_directories(scf PRIVATE parse)
target_include_directories(scf PRIVATE pack)
target_include_directories(scf PRIVATE elf)
target_include_directories(scf PRIVATE vm)
target_include_directories(scf PRIVATE native)
target_include_directories(scf PRIVATE native/x64)
target_include_directories(scf PRIVATE native/risc)
target_include_directories(scf PRIVATE native/eda)
