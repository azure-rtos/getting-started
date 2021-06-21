# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

set(CMAKE_C_FLAGS "${MCPU_FLAGS} ${VFP_FLAGS} ${CMAKE_COMMON_FLAGS} -Wno-old-style-declaration")

set(SOURCES
    startup/${THREADX_TOOLCHAIN}/tx_initialize_low_level.S
    startup/${THREADX_TOOLCHAIN}/startup_msp432e401y.c
    startup/${THREADX_TOOLCHAIN}/system_msp432e401y.c
#    legacy/mqtt.c
    azure_config.h
    nx_client.c
    board_init.c
    main.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_definitions(${PROJECT_NAME}
    PUBLIC 
        __MSP432E401Y__
)


target_link_libraries(${PROJECT_NAME}
    PUBLIC
        azrtos::threadx
        azrtos::netxduo
         
        ti_start
        app_common
        jsmn
        netxdriver
        ${TI_DRV_LIB_DIR}/msp432e4_driverlib.a
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        SUFFIX ".elf"
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        .
        ${COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR}/source
        ${COM_TI_SIMPLELINK_MSP432E4_SDK_INSTALL_DIR}/source/ti/devices/msp432e4/inc
)

target_link_options(${PROJECT_NAME}
    PRIVATE 
        -T${CMAKE_CURRENT_LIST_DIR}/startup/gnu/msp432e401y.lds -Wl,-Map=${PROJECT_NAME}.map)

post_build(${PROJECT_NAME})
