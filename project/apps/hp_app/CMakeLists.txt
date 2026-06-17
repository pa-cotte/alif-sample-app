cmake_minimum_required(VERSION 3.13.1)

set(EXTRA_CONF_FILE ../../base.conf)

# adds out-of-tree custom drivers as a Zephyr extra module (this line should be above the find_package line)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR}/drivers)

# Ethernet + zperf benchmark. The MAC is a single shared peripheral, so only one
# core may own it at a time (see project/apps/common/eth.overlay). HP defaults
# OFF (HE owns Ethernet by default); benchmark HP with -DAPP_ETHERNET=ON AND
# flash HP only, so the two cores never drive the MAC simultaneously.
option(APP_ETHERNET "Build with the Alif Ethernet + zperf benchmark" OFF)
if(APP_ETHERNET)
  list(APPEND EXTRA_CONF_FILE ${PROJECT_DIR}/apps/common/eth_zperf.conf)
  list(APPEND EXTRA_DTC_OVERLAY_FILE ${PROJECT_DIR}/apps/common/eth.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Warnings as errors for C++ (C/ASM already handled by CONFIG_COMPILER_WARNINGS_AS_ERRORS)
zephyr_compile_options($<$<COMPILE_LANGUAGE:CXX>:$<TARGET_PROPERTY:compiler,warnings_as_errors>>)

project(hp_app)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE main.cpp )
