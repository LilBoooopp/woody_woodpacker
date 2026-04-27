cmake_minimum_required(VERSION 3.16...3.30 FATAL_ERROR)

project(
  woody
  VERSION 1.0
  DESCRIPTION "woody packer"
  LANGUAGES C)

# set(CMAKE_C_STANDARD)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_language(ASM_NASM)
find_program(XXD_EXECUTABLE xxd REQUIRED)

if(UNIX
   AND NOT APPLE
   AND CMAKE_BUILD_TYPE MATCHES "Debug")
  add_link_options(-g -O0)
  message(WARNING "Linux debug Compiling")
elseif(APPLE AND CMAKE_BUILD_TYPE MATCHES "ASan")
  add_link_options(
    -g3
    -O0
    -fsanitize=address
    -fsanitize=pointer-compare
    -fsanitize=pointer-subtract
    -fsanitize=bounds
    -fsanitize=nonnull-attribute
    -fno-omit-frame-pointer
    -fgpu-sanitize
    -shared-libsan
    -fsanitize-address-use-after-scope)
  message(WARNING "Compiling for Macos with ASan options")
elseif(APPLE AND CMAKE_BUILD_TYPE MATCHES "TSan")
  add_link_options(-g -O0 -fsanitize=thread -fno-omit-frame-pointer -lpthread)
  message(WARNING "Compiling for Macos with TSan options")
elseif(APPLE AND CMAKE_BUILD_TYPE MATCHES "Debug")
  add_link_options(-g -O0)
  message(WARNING "Compiling for Macos with debug options")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GCC")
  add_compile_options(-Wall -Wextra -Werror)
endif()

find_program(NASM_EXECUTABLE nasm REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stub.bin
    COMMAND ${NASM_EXECUTABLE} -f bin
            ${CMAKE_CURRENT_SOURCE_DIR}/stub.asm
            -o ${CMAKE_CURRENT_BINARY_DIR}/stub.bin
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/stub.asm
    COMMENT "Assembling stub.asm -> stub.bin"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stub.h
    COMMAND xxd -i ${CMAKE_CURRENT_SOURCE_DIR}/stub.bin
            ${CMAKE_CURRENT_BINARY_DIR}/stub.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/stub.bin
)

# --------------------------
# add files here
# --------------------------

set(SOURCES main.c)
set(CMAKE_ASM_NASM_COMPILE_OBJECT nasm -f bin stub.asm -o stub.bin)

add_executable(woody_woodpacker ${SOURCES})
