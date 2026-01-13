cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../../base.conf
                    ${CMAKE_CURRENT_SOURCE_DIR}/../../../tests.conf
    )
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(test_hpApp)

# Use same include directories as the app
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../)

# tests suite
target_sources(
    app
    PRIVATE tests.c
    )
