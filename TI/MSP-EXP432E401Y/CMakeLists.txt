# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_C_STANDARD 99)

set(GSG_BASE_DIR ${CMAKE_SOURCE_DIR}/../..)
set(CORE_SRC_DIR ${GSG_BASE_DIR}/core/src)
set(CORE_LIB_DIR ${GSG_BASE_DIR}/core/lib)
set(COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR c:/ti/simplelink_msp432e4_sdk_4_20_00_12)
set(TI_DRV_LIB_DIR ${COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR}/source/ti/devices/msp432e4/driverlib/lib/gcc/m4f)

# use the repo version of ninja on Windows as there is no Ninja installer
if(WIN32)
    set(CMAKE_MAKE_PROGRAM ${GSG_BASE_DIR}/cmake/ninja CACHE STRING "Ninja location")
endif()

# Set the toolchain if not defined
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${GSG_BASE_DIR}/cmake/arm-gcc-cortex-m4.cmake")
endif()

include(${GSG_BASE_DIR}/cmake/utilities.cmake)

# Define the Project
project(msp432e_azure_iot C ASM)

add_subdirectory(${CORE_SRC_DIR} core_src)
add_subdirectory(lib)
add_subdirectory(app)
