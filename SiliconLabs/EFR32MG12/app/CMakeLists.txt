# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/startup/efr32mg12p.ld")

set(SOURCES
    startup/startup_efr32mg12p.S
    startup/system_efr32mg12p.c
    startup/tx_initialize_low_level.S

    sl_wfx_host/sl_wfx_host_bus.c
    sl_wfx_host/sl_wfx_host_gpio.c
    sl_wfx_host/sl_wfx_host_spi.c
    sl_wfx_host/sl_wfx_host.c
    sl_wfx_host/sl_wfx_task.c

    networking.c
    console.c
    main.c
    nx_client.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    azrtos::threadx
    azrtos::netxduo
    
    app_common
    jsmn
    gecko_sdk_3.0.0_efr32mg12
    netx_driver
)

target_include_directories(${PROJECT_NAME}
    PRIVATE 
        pds
        sl_wfx_host
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        SL_WFX_USE_SPI
        SLEXP802X
)

if(CMAKE_C_COMPILER_ID STREQUAL "IAR")
    message(FATAL_ERROR, "IAR is not currently implemented for this device")
else()
    set_target_linker(${PROJECT_NAME} ${LINKER_SCRIPT})
endif()

post_build(${PROJECT_NAME})
