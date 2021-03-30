/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include <stdio.h>

#include "tx_api.h"

#include "board_init.h"
#include "cmsis_utils.h"
#include "sntp_client.h"
#include "stm_networking.h"

#include "legacy/mqtt.h"
#include "nx_client.h"

#include "azure_config.h"
#include "prompt.h"

#define AZURE_THREAD_STACK_SIZE 4096
#define AZURE_THREAD_PRIORITY   4

TX_THREAD azure_thread;
ULONG azure_thread_stack[AZURE_THREAD_STACK_SIZE / sizeof(ULONG)];

void azure_thread_entry(ULONG parameter);
void tx_application_define(void* first_unused_memory);

void azure_thread_entry(ULONG parameter)
{
    UINT status;

    printf("\r\nStarting Azure thread\r\n\r\n");

    serial_setup();

    DevConfig_IoT_Info_t device_info;
    status = read_flash(&device_info);
    if (status != STATUS_OK) 
    {
      printf("Unable to read credentials from flash.\n");
      return;
    }

    if (stm32_network_init(device_info.ssid, device_info.pswd, WIFI_MODE) != NX_SUCCESS)
    {
        printf("Failed to initialize the network\r\n");
        return;
    }

    // Start the SNTP client
    status = sntp_start();
    if (status != NX_SUCCESS)
    {
        printf("Failed to start the SNTP client (0x%02x)\r\n", status);
        return;
    }

    // Wait for an SNTP sync
    status = sntp_sync_wait();
    if (status != NX_SUCCESS)
    {
        printf("Failed to start sync SNTP time (0x%02x)\r\n", status);
        return;
    }

#ifdef ENABLE_LEGACY_MQTT
    if ((status = azure_iot_mqtt_entry(&nx_ip, &nx_pool, &nx_dns_client, sntp_time_get)))
#else
    if ((status = azure_iot_nx_client_entry(&nx_ip, &nx_pool, &nx_dns_client, sntp_time)))
#endif
    {
        printf("Failed to run Azure IoT (0x%04x)\r\n", status);
        return;
    }
}

void tx_application_define(void* first_unused_memory)
{
    systick_interval_set(TX_TIMER_TICKS_PER_SECOND);

    // Create Azure thread
    UINT status = tx_thread_create(&azure_thread,
        "Azure Thread",
        azure_thread_entry,
        0,
        azure_thread_stack,
        AZURE_THREAD_STACK_SIZE,
        AZURE_THREAD_PRIORITY,
        AZURE_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        printf("Azure IoT thread creation failed\r\n");
    }
}

int main(void)
{
    // Initialize the board
    board_init();

    // Enter the ThreadX kernel
    tx_kernel_enter();
    return 0;
}
