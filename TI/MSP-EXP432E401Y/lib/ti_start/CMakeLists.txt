# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

set(TARGET ti_start)

set(CMAKE_C_FLAGS "${MCPU_FLAGS} ${VFP_FLAGS} ${CMAKE_COMMON_FLAGS} -Wno-old-style-declaration")

set(SOURCES
    src/ti_start.c
    src/stdio_redirect/uartstdio.c
    src/stdio_redirect/read.c
    src/stdio_redirect/write.c
)

add_library(${TARGET} OBJECT
    ${SOURCES}
)

target_compile_definitions(${TARGET}
    PUBLIC 
        __MSP432E401Y__
)

target_include_directories(${TARGET}
    PUBLIC
        config
        src
        ${COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR}/source
        ${COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR}/source/third_party/CMSIS/Include
)

target_link_libraries(${TARGET}
    PUBLIC
        azrtos::threadx
        azrtos::netxduo
)
