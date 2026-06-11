cmake_minimum_required(VERSION 3.13.1)

set(EXTRA_CONF_FILE ../../base.conf)

# adds out-of-tree custom drivers as a Zephyr extra module (this line should be above the find_package line)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR}/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Warnings as errors for C++ (C/ASM already handled by CONFIG_COMPILER_WARNINGS_AS_ERRORS)
zephyr_compile_options($<$<COMPILE_LANGUAGE:CXX>:$<TARGET_PROPERTY:compiler,warnings_as_errors>>)

project(he_app)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE main.cpp )
