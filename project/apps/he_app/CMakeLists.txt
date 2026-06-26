cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_CONF_FILE ../../base.conf)

# adds out-of-tree custom drivers as a Zephyr extra module (this line should be above the find_package line)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR}/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Warnings as errors for C++ (C/ASM already handled by CONFIG_COMPILER_WARNINGS_AS_ERRORS)
zephyr_compile_options($<$<COMPILE_LANGUAGE:CXX>:$<TARGET_PROPERTY:compiler,warnings_as_errors>>)

project(he_app)

# Pulls in the USB sample helper (sample_usbd_init.c) used by the USB device
# "next" stack, and adds samples/subsys/usb/common to the include path.
include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE main.cpp)
