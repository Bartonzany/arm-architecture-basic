cmake_minimum_required(VERSION 3.16)
project(arm64_a64_isa_verify LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 交叉编译工具链 -------------------------------------------------------
# 默认针对 AArch64。构建：cmake -B build -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
set(CROSS_COMPILE_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross toolchain prefix")
if(NOT DEFINED CMAKE_CXX_COMPILER OR CMAKE_CXX_COMPILER STREQUAL "")
    set(CMAKE_CXX_COMPILER "${CROSS_COMPILE_PREFIX}g++" CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_FLAGS "-O2 -march=armv8-a+simd -fno-stack-protector -fno-stack-clash-protection")

# ---- 每个 src/*.cpp 生成一个可执行 ----------------------------------------
file(GLOB DEMO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
foreach(src ${DEMO_SOURCES})
    get_filename_component(demo_name ${src} NAME_WE)
    add_executable(${demo_name} ${src})
endforeach()

# ---- 可选：为每个 demo 生成汇编 (.s) ---------------------------------------
# 用法：cmake --build build --target asm
set(ASM_FLAGS "-O2" "-march=armv8-a+simd" "-fno-stack-protector" "-fno-stack-clash-protection")
set(ASM_DIR "${CMAKE_CURRENT_BINARY_DIR}/asm")
file(MAKE_DIRECTORY "${ASM_DIR}")
set(ASM_OUTPUTS "")
foreach(src ${DEMO_SOURCES})
    get_filename_component(demo_name ${src} NAME_WE)
    set(asm_file "${ASM_DIR}/${demo_name}.s")
    add_custom_command(
        OUTPUT ${asm_file}
        COMMAND ${CMAKE_CXX_COMPILER} ${ASM_FLAGS} -S ${src} -o ${asm_file}
        DEPENDS ${src}
        COMMENT "Generating assembly for ${demo_name}"
        VERBATIM)
    list(APPEND ASM_OUTPUTS ${asm_file})
endforeach()
add_custom_target(asm ALL DEPENDS ${ASM_OUTPUTS})