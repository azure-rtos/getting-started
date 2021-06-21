# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

set(CMAKE_C_FLAGS "${MCPU_FLAGS} ${VFP_FLAGS} ${CMAKE_COMMON_FLAGS} -Wno-old-style-declaration")

set(SOURCES
    src/${THREADX_TOOLCHAIN}/nx_driver_msp432e_low_level.S
    src/nx_driver_msp432e.c
    src/nx_driver_msp432e.h
)

set(TARGET netxdriver)

add_library(${TARGET} OBJECT
    ${SOURCES}
)

target_compile_definitions(${TARGET}
    PUBLIC 
        __MSP432E401Y__
        NX_INCLUDE_USER_DEFINE_FILE
)

target_include_directories(${TARGET}
    PUBLIC
        src
        ${COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR}/source/ti/devices/msp432e4/inc
)

target_link_libraries(${TARGET} 
    PUBLIC
        azrtos::threadx
        azrtos::netxduo
        ti_start
)
