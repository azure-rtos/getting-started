/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include "networking.h"
#include "sntp_client.h"

#include "nx_client.h"

#include "nx_sl_wfx_driver.h"
#include "sl_system_init.h"
#include "sl_system_kernel.h"

/* Define Azure thread properties */
#define AZURE_THREAD_PRIORITY    4
#define AZURE_THREAD_STACK_SIZE  4096
#define AZURE_THREAD_ENTRY_INPUT 0

/* Static variables/constants */
static TX_THREAD azure_thread;
static ULONG azure_thread_stack[AZURE_THREAD_STACK_SIZE / sizeof(ULONG)];

void azure_thread_entry(ULONG thread_input)
{
    UINT status;

    printf("\r\nStarting Azure thread\r\n\r\n");

    // Initialize the network
    if (network_init(nx_sl_wfx_driver_entry) == false)
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

    status = azure_iot_nx_client_entry(&nx_ip, &nx_pool, &nx_dns_client, sntp_time);
    if (status != NX_SUCCESS)
    {
        printf("Failed to run Azure IoT (0x%04x)\r\n", status);
        return;
    }
}

void tx_application_define(void* first_unused_memory)
{
    // Create application main thread
    UINT status = tx_thread_create(&azure_thread,
        "Azure Thread",
        azure_thread_entry,
        AZURE_THREAD_ENTRY_INPUT,
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
    // Initialize Silicon Labs device, system, service(s) and protocol stack(s)
    sl_system_init();

    // Start the kernel. Task(s) created in tx_application_define() will start running
    sl_system_kernel_start();
}
