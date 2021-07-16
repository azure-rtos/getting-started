# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# Define the CPU architecture for Threadx
set(THREADX_ARCH "cortex_m4")
set(THREADX_TOOLCHAIN "gnu")

# Define ThreadX user configuration
set(TX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/threadx/tx_user.h" CACHE STRING "Enable TX user configuration")

# Define NetXDuo user configuration
set(NX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/netxduo/nx_user.h" CACHE STRING "Enable NX user configuration")
set(NXD_ENABLE_AZURE_IOT ON CACHE BOOL "Enable Azure IoT")
set(NXD_ENABLE_FILE_SERVERS OFF CACHE BOOL "Disable fileX dependency by netxduo")

# Disable ASC
set(NX_AZURE_DISABLE_IOT_SECURITY_MODULE ON CACHE BOOL "Disable ASC")

# Core libraries
add_subdirectory(${CORE_LIB_DIR}/threadx threadx)
add_subdirectory(${CORE_LIB_DIR}/netxduo netxduo)
add_subdirectory(${CORE_LIB_DIR}/jsmn jsmn)

# Gecko SDK
add_subdirectory(gecko_sdk_3.0.0_efr32mg12)
# WFX200 network driver
add_subdirectory(netx_driver)
