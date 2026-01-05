cmake_minimum_required(VERSION 3.13.1)

set(EXTRA_CONF_FILE ../../base.conf)

# adds out-of-tree custom drivers as a Zephyr extra module (this line should be above the find_package line)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR}/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(he_app)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE main.cpp )



