# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

set(SOURCES
    src/nx_sl_wfx_driver.c
)

set(PROJECT netx_driver)

add_library(${PROJECT} OBJECT
    ${SOURCES}
)

target_link_libraries(${PROJECT}
    PUBLIC
        azrtos::threadx
        azrtos::netxduo
        gecko_sdk_3.0.0_efr32mg12
)

target_include_directories(${PROJECT}
    PUBLIC
        src
        ../../app/sl_wfx_host
)
