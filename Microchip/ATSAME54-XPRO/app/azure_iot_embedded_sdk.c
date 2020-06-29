/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include <stdio.h>

#include "Bosch_BME280.h"
#include "atmel_start.h"

#include "nx_api.h"
#include "nx_azure_iot_hub_client.h"
#include "nx_azure_iot_provisioning_client.h"

#include "jsmn.h"

/* These are sample files, user can build their own certificate and
 * ciphersuites.  */
#include "azure_iot_cert.h"
#include "azure_iot_ciphersuites.h"
//#include "sample_config.h"

#include "azure_config.h"

#define NX_AZURE_IOT_STACK_SIZE      2048
#define NX_AZURE_IOT_THREAD_PRIORITY 3
#define SAMPLE_STACK_SIZE            2048
#define SAMPLE_THREAD_PRIORITY       16

//#define MAX_PROPERTY_COUNT     2

#define MODULE_ID ""

/* Define Azure RTOS TLS info.  */
static NX_SECURE_X509_CERT root_ca_cert;
static UCHAR nx_azure_iot_tls_metadata_buffer[NX_AZURE_IOT_TLS_METADATA_BUFFER_SIZE];
static ULONG nx_azure_iot_thread_stack[NX_AZURE_IOT_STACK_SIZE / sizeof(ULONG)];

/* Define the prototypes for AZ IoT.  */
static NX_AZURE_IOT nx_azure_iot;
static NX_AZURE_IOT_HUB_CLIENT iothub_client;
#ifdef ENABLE_DPS_SAMPLE
static NX_AZURE_IOT_PROVISIONING_CLIENT prov_client;
#endif /* ENABLE_DPS_SAMPLE */

/* Define buffer for IoTHub info. */
#ifdef ENABLE_DPS_SAMPLE
static UCHAR sample_iothub_hostname[SAMPLE_MAX_BUFFER];
static UCHAR sample_iothub_device_id[SAMPLE_MAX_BUFFER];
#endif /* ENABLE_DPS_SAMPLE */

static INT telemetry_interval = 10;

/* Define sample threads. */
static TX_THREAD sample_telemetry_thread;
static ULONG sample_telemetry_thread_stack[SAMPLE_STACK_SIZE / sizeof(ULONG)];
static TX_THREAD sample_c2d_thread;
static ULONG sample_c2d_thread_stack[SAMPLE_STACK_SIZE / sizeof(ULONG)];
static TX_THREAD sample_direct_method_thread;
static ULONG sample_direct_method_thread_stack[SAMPLE_STACK_SIZE / sizeof(ULONG)];
static TX_THREAD sample_device_twin_thread;
static ULONG sample_device_twin_thread_stack[SAMPLE_STACK_SIZE / sizeof(ULONG)];

#ifdef ENABLE_DPS_SAMPLE
static UINT sample_dps_entry(
    UCHAR** iothub_hostname, UINT* iothub_hostname_length, UCHAR** iothub_device_id, UINT* iothub_device_id_length);
#endif /* ENABLE_DPS_SAMPLE */

static void sample_telemetry_thread_entry(ULONG parameter);
static void sample_c2d_thread_entry(ULONG parameter);
static void sample_direct_method_thread_entry(ULONG parameter);
static void sample_device_twin_thread_entry(ULONG parameter);

static void set_led_state(bool level)
{
    if (level)
    {
        // Pin level set to "high" state
        printf("LED0 is turned OFF\r\n");
    }
    else
    {
        // Pin level set to "low" state
        printf("LED0 is turned ON\r\n");
    }

    gpio_set_pin_level(PC18, level);
}

static VOID printf_packet(NX_PACKET* packet_ptr)
{
    while (packet_ptr != NX_NULL)
    {
        printf("%.*s", (INT)(packet_ptr->nx_packet_length), (CHAR*)packet_ptr->nx_packet_prepend_ptr);
        packet_ptr = packet_ptr->nx_packet_next;
    }
    printf("\r\n");
}

static VOID connection_status_callback(NX_AZURE_IOT_HUB_CLIENT* hub_client_ptr, UINT status)
{
    NX_PARAMETER_NOT_USED(hub_client_ptr);
    if (status)
    {
        printf("Disconnected from IoTHub!: error code = 0x%08x\r\n", status);
    }
    else
    {
        printf("Connected to IoTHub.\r\n");
    }
}

static bool findJsonInt(const char* json, jsmntok_t* tokens, int tokens_count, const char* s, int* value)
{
    for (int i = 1; i < tokens_count; i++)
    {
        if ((tokens[i].type == JSMN_STRING) && (strlen(s) == tokens[i].end - tokens[i].start) &&
            (strncmp(json + tokens[i].start, s, tokens[i].end - tokens[i].start) == 0))
        {
            *value = atoi(json + tokens[i + 1].start);

            printf("Desired property %s = %d\r\n", "telemetryInterval", *value);
            return true;
        }
    }

    return false;
}

static UINT report_telemetry_float(CHAR* key, float value, NX_PACKET* packet_ptr)
{
    UINT status;
    CHAR buffer[30];

    snprintf(buffer, sizeof(buffer), "{\"%s\":%0.2f}", key, value);

    if ((status = nx_azure_iot_hub_client_telemetry_send(
             &iothub_client, packet_ptr, (UCHAR*)buffer, strlen(buffer), NX_WAIT_FOREVER)))
    {
        printf("Telemetry message send failed!: error code = 0x%08x\r\n", status);
        //        nx_azure_iot_hub_client_telemetry_message_delete(packet_ptr);
        return status;
    }

    printf("Telemetry message sent: %s.\r\n", buffer);

    return NX_SUCCESS;
}

static UINT report_device_twin_property_float(CHAR* key, float value)
{
    UINT status;
    UINT response_status;
    UINT request_id;
    CHAR buffer[30];

    snprintf(buffer, sizeof(buffer), "{\"%s\":%0.2f}", key, value);

    if ((status = nx_azure_iot_hub_client_device_twin_reported_properties_send(
             &iothub_client, (UCHAR*)buffer, strlen(buffer), &request_id, &response_status, NX_WAIT_FOREVER)))
    {
        printf("Device twin reported properties failed!: error code = 0x%08x\r\n", status);
        return status;
    }

    if ((response_status < 200) || (response_status >= 300))
    {
        printf("device twin report properties failed with code : %d\r\n", response_status);
        return status;
    }

    printf("Device twin property sent: %s.\r\n", buffer);

    return NX_SUCCESS;
}

UINT azure_iot_embedded_sdk_entry(
    NX_IP* ip_ptr, NX_PACKET_POOL* pool_ptr, NX_DNS* dns_ptr, UINT (*unix_time_callback)(ULONG* unix_time))
{
    UINT status = 0;
#ifdef ENABLE_DPS_SAMPLE
    UCHAR* iothub_hostname       = NX_NULL;
    UCHAR* iothub_device_id      = NX_NULL;
    UINT iothub_hostname_length  = 0;
    UINT iothub_device_id_length = 0;
#else
    UCHAR* iothub_hostname       = (UCHAR*)IOT_HUB_HOSTNAME;
    UCHAR* iothub_device_id      = (UCHAR*)IOT_DEVICE_ID;
    UINT iothub_hostname_length  = strlen(IOT_HUB_HOSTNAME);
    UINT iothub_device_id_length = strlen(IOT_DEVICE_ID);
#endif /* ENABLE_DPS_SAMPLE */

    /* Create Azure IoT handler.  */
    if ((status = nx_azure_iot_create(&nx_azure_iot,
             (UCHAR*)"Azure IoT",
             ip_ptr,
             pool_ptr,
             dns_ptr,
             nx_azure_iot_thread_stack,
             sizeof(nx_azure_iot_thread_stack),
             NX_AZURE_IOT_THREAD_PRIORITY,
             unix_time_callback)))
    {
        printf("Failed on nx_azure_iot_create!: error code = 0x%08x\r\n", status);
        return status;
    }

    /* Initialize CA certificate. */
    if ((status = nx_secure_x509_certificate_initialize(&root_ca_cert,
             (UCHAR*)azure_iot_root_ca,
             (USHORT)azure_iot_root_ca_len,
             NX_NULL,
             0,
             NULL,
             0,
             NX_SECURE_X509_KEY_TYPE_NONE)))
    {
        printf("Failed to initialize ROOT CA certificate!: error code = 0x%08x\r\n", status);
        nx_azure_iot_delete(&nx_azure_iot);
        return status;
    }

#ifdef ENABLE_DPS_SAMPLE
    /* Run DPS. */
    if ((status = sample_dps_entry(
             &iothub_hostname, &iothub_hostname_length, &iothub_device_id, &iothub_device_id_length)))
    {
        printf("Failed on sample_dps_entry!: error code = 0x%08x\r\n", status);
        nx_azure_iot_delete(&nx_azure_iot);
        return status;
    }
#endif /* ENABLE_DPS_SAMPLE */

    /* Initialize IoTHub client. */
    if ((status = nx_azure_iot_hub_client_initialize(&iothub_client,
             &nx_azure_iot,
             iothub_hostname,
             iothub_hostname_length,
             iothub_device_id,
             iothub_device_id_length,
             (UCHAR*)MODULE_ID,
             strlen(MODULE_ID),
             _nx_azure_iot_tls_supported_crypto,
             _nx_azure_iot_tls_supported_crypto_size,
             _nx_azure_iot_tls_ciphersuite_map,
             _nx_azure_iot_tls_ciphersuite_map_size,
             nx_azure_iot_tls_metadata_buffer,
             sizeof(nx_azure_iot_tls_metadata_buffer),
             &root_ca_cert)))
    {
        printf("Failed on nx_azure_iot_hub_client_initialize!: error code = "
               "0x%08x\r\n",
            status);
        nx_azure_iot_delete(&nx_azure_iot);
        return status;
    }

    /* Set symmetric key.  */
    if ((status = nx_azure_iot_hub_client_symmetric_key_set(
             &iothub_client, (UCHAR*)IOT_PRIMARY_KEY, strlen(IOT_PRIMARY_KEY))))
    {
        printf("Failed on nx_azure_iot_hub_client_symmetric_key_set!\r\n");
        return status;
    }

    /* Set connection status callback. */
    if (nx_azure_iot_hub_client_connection_status_callback_set(&iothub_client, connection_status_callback))
    {
        printf("Failed on connection_status_callback!\r\n");
        return status;
    }

    /* Connect to IoTHub client. */
    if (nx_azure_iot_hub_client_connect(&iothub_client, NX_TRUE, NX_WAIT_FOREVER))
    {
        printf("Failed on nx_azure_iot_hub_client_connect!\r\n");
        return status;
    }

    /* Create Telemetry sample thread. */
    if ((status = tx_thread_create(&sample_telemetry_thread,
             "Sample Telemetry Thread",
             sample_telemetry_thread_entry,
             0,
             (UCHAR*)sample_telemetry_thread_stack,
             SAMPLE_STACK_SIZE,
             SAMPLE_THREAD_PRIORITY,
             SAMPLE_THREAD_PRIORITY,
             1,
             TX_AUTO_START)))
    {
        printf("Failed to create telemetry sample thread!: error code = 0x%08x\r\n", status);
        return status;
    }

    /* Create C2D sample thread. */
    if ((status = tx_thread_create(&sample_c2d_thread,
             "Sample C2D Thread",
             sample_c2d_thread_entry,
             0,
             (UCHAR*)sample_c2d_thread_stack,
             SAMPLE_STACK_SIZE,
             SAMPLE_THREAD_PRIORITY,
             SAMPLE_THREAD_PRIORITY,
             1,
             TX_AUTO_START)))
    {
        printf("Failed to create c2d sample thread!: error code = 0x%08x\r\n", status);
        return status;
    }

    /* Create Direct Method sample thread. */
    if ((status = tx_thread_create(&sample_direct_method_thread,
             "Sample Direct Method Thread",
             sample_direct_method_thread_entry,
             0,
             (UCHAR*)sample_direct_method_thread_stack,
             SAMPLE_STACK_SIZE,
             SAMPLE_THREAD_PRIORITY,
             SAMPLE_THREAD_PRIORITY,
             1,
             TX_AUTO_START)))
    {
        printf("Failed to create direct method sample thread!: error code = "
               "0x%08x\r\n",
            status);
        return status;
    }

    /* Create Device twin sample thread. */
    if ((status = tx_thread_create(&sample_device_twin_thread,
             "Sample Device Twin Thread",
             sample_device_twin_thread_entry,
             0,
             (UCHAR*)sample_device_twin_thread_stack,
             SAMPLE_STACK_SIZE,
             SAMPLE_THREAD_PRIORITY,
             SAMPLE_THREAD_PRIORITY,
             1,
             TX_AUTO_START)))
    {
        printf("Failed to create device twin sample thread!: error code = "
               "0x%08x\r\n",
            status);
        return status;
    }

    /* Simply loop in sample. */
    while (true)
    {
        tx_thread_sleep(NX_IP_PERIODIC_RATE);
    }

    /* Destroy IoTHub Client. */
    nx_azure_iot_hub_client_disconnect(&iothub_client);
    nx_azure_iot_hub_client_deinitialize(&iothub_client);
    nx_azure_iot_delete(&nx_azure_iot);
}

#ifdef ENABLE_DPS_SAMPLE
static UINT sample_dps_entry(
    UCHAR** iothub_hostname, UINT* iothub_hostname_length, UCHAR** iothub_device_id, UINT* iothub_device_id_length)
{
    UINT status;

    /* Initialize IoT provisioning client.  */
    if ((status = nx_azure_iot_provisioning_client_initialize(&prov_client,
             &nx_azure_iot,
             (UCHAR*)ENDPOINT,
             sizeof(ENDPOINT) - 1,
             (UCHAR*)ID_SCOPE,
             sizeof(ID_SCOPE) - 1,
             (UCHAR*)REGISTRATION_ID,
             sizeof(REGISTRATION_ID) - 1,
             _nx_azure_iot_tls_supported_crypto,
             _nx_azure_iot_tls_supported_crypto_size,
             _nx_azure_iot_tls_ciphersuite_map,
             _nx_azure_iot_tls_ciphersuite_map_size,
             nx_azure_iot_tls_metadata_buffer,
             sizeof(nx_azure_iot_tls_metadata_buffer),
             &root_ca_cert)))
    {
        printf("Failed on nx_azure_iot_provisioning_client_initialize!: error code "
               "= 0x%08x\r\n",
            status);
        return (status);
    }

    /* Initialize length of hostname and device ID. */
    *iothub_hostname_length  = sizeof(sample_iothub_hostname);
    *iothub_device_id_length = sizeof(sample_iothub_device_id);

    /* Set symmetric key.  */
    if ((status = nx_azure_iot_provisioning_client_symmetric_key_set(
             &prov_client, (UCHAR*)DEVICE_SYMMETRIC_KEY, sizeof(DEVICE_SYMMETRIC_KEY) - 1)))
    {
        printf("Failed on nx_azure_iot_hub_client_symmetric_key_set!: error code = "
               "0x%08x\r\n",
            status);
    }

    /* Register device */
    else if ((status = nx_azure_iot_provisioning_client_register(&prov_client, NX_WAIT_FOREVER)))
    {
        printf("Failed on nx_azure_iot_provisioning_client_register!: error code = "
               "0x%08x\r\n",
            status);
    }

    /* Get Device info */
    else if ((status = nx_azure_iot_provisioning_client_iothub_device_info_get(&prov_client,
                  sample_iothub_hostname,
                  iothub_hostname_length,
                  sample_iothub_device_id,
                  iothub_device_id_length)))
    {
        printf("Failed on nx_azure_iot_provisioning_client_iothub_device_info_get!: "
               "error code = 0x%08x\r\n",
            status);
    }
    else
    {
        *iothub_hostname  = sample_iothub_hostname;
        *iothub_device_id = sample_iothub_device_id;
    }

    /* Destroy Provisioning Client.  */
    nx_azure_iot_provisioning_client_deinitialize(&prov_client);

    return (status);
}
#endif /* ENABLE_DPS_SAMPLE */

static void sample_telemetry_thread_entry(ULONG parameter)
{
    NX_PACKET* packet_ptr;
    UINT status;
    float temperature = 28.5;

    NX_PARAMETER_NOT_USED(parameter);

    /* Loop to send telemetry message. */
    while (true)
    {
        /* Create a telemetry message packet. */
        if ((status = nx_azure_iot_hub_client_telemetry_message_create(&iothub_client, &packet_ptr, NX_WAIT_FOREVER)))
        {
            printf("Telemetry message create failed!: error code = 0x%08x\r\n", status);
            break;
        }

#if __SENSOR_BME280__ == 1
        WeatherClick_waitforRead();
        temperature = Weather_getTemperatureDegC();
#endif

        report_telemetry_float("temperature", temperature, packet_ptr);
        report_device_twin_property_float("currentTemperature", temperature);

        nx_azure_iot_hub_client_telemetry_message_delete(packet_ptr);

        tx_thread_sleep(telemetry_interval * NX_IP_PERIODIC_RATE);
    }
}

static void sample_c2d_thread_entry(ULONG parameter)
{
    NX_PACKET* packet_ptr;
    UINT status = 0;
    USHORT property_buf_size;
    UCHAR* property_buf;
    CHAR* temperature_property = "temperature";

    NX_PARAMETER_NOT_USED(parameter);

    if ((status = nx_azure_iot_hub_client_cloud_message_enable(&iothub_client)))
    {
        printf("C2D receive enable failed!: error code = 0x%08x\r\n", status);
        return;
    }

    /* Loop to receive c2d message.  */
    while (true)
    {
        if ((status = nx_azure_iot_hub_client_cloud_message_receive(&iothub_client, &packet_ptr, NX_WAIT_FOREVER)))
        {
            printf("C2D receive failed!: error code = 0x%08x\r\n", status);
            break;
        }

        if ((status = nx_azure_iot_hub_client_cloud_message_property_get(&iothub_client,
                 packet_ptr,
                 (UCHAR*)temperature_property,
                 (USHORT)strlen(temperature_property),
                 &property_buf,
                 &property_buf_size)))
        {
            printf("Property [%s] not found: 0x%08x\r\n", temperature_property, status);
        }
        else
        {
            printf("Receive property: %s = %.*s\r\n", temperature_property, (INT)property_buf_size, property_buf);
        }

        printf("Receive message:");
        printf_packet(packet_ptr);
        nx_packet_release(packet_ptr);
    }
}

static void sample_direct_method_thread_entry(ULONG parameter)
{
    NX_PACKET* packet_ptr;
    UINT status;
    UCHAR* method_name_ptr;
    USHORT method_name_length;
    VOID* context_ptr;
    USHORT context_length;

    UINT http_status;
    CHAR* http_response = "{}";

    NX_PARAMETER_NOT_USED(parameter);

    if ((status = nx_azure_iot_hub_client_direct_method_enable(&iothub_client)))
    {
        printf("Direct method receive enable failed!: error code = 0x%08x\r\n", status);
        return;
    }

    /* Loop to receive direct method message.  */
    while (true)
    {
        http_status = 501;

        if ((status = nx_azure_iot_hub_client_direct_method_message_receive(&iothub_client,
                 &method_name_ptr,
                 &method_name_length,
                 &context_ptr,
                 &context_length,
                 &packet_ptr,
                 NX_WAIT_FOREVER)))
        {
            printf("Direct method receive failed!: error code = 0x%08x\r\n", status);
            break;
        }

        printf("Receive method call: %.*s, with payload:", (INT)method_name_length, (CHAR*)method_name_ptr);
        printf_packet(packet_ptr);

        if (strncmp((CHAR*)method_name_ptr, "set_led_state", method_name_length) == 0)
        {
            // set_led_state command
            printf("received set_led_state\r\n");

            bool arg = strncmp((CHAR*)packet_ptr->nx_packet_prepend_ptr, "true", packet_ptr->nx_packet_length);
            set_led_state(arg);
            http_status = 200;
        }

        if ((status = nx_azure_iot_hub_client_direct_method_message_response(&iothub_client,
                 http_status,
                 context_ptr,
                 context_length,
                 (UCHAR*)http_response,
                 strlen(http_response),
                 NX_WAIT_FOREVER)))
        {
            printf("Direct method response failed!: error code = 0x%08x\r\n", status);
            break;
        }

        nx_packet_release(packet_ptr);
    }
}

static void sample_device_twin_thread_entry(ULONG parameter)
{
    NX_PACKET* packet_ptr;
    UINT status;

    jsmn_parser parser;
    jsmntok_t tokens[16];
    INT token_count;

    NX_PARAMETER_NOT_USED(parameter);

    if ((status = nx_azure_iot_hub_client_device_twin_enable(&iothub_client)))
    {
        printf("device twin enabled failed!: error code = 0x%08x\r\n", status);
        return;
    }

    // Request and parse the device twin properties
    if ((status = nx_azure_iot_hub_client_device_twin_properties_request(&iothub_client, NX_WAIT_FOREVER)))
    {
        printf("device twin document request failed!: error code = 0x%08x\r\n", status);
        return;
    }

    if ((status = nx_azure_iot_hub_client_device_twin_properties_receive(&iothub_client, &packet_ptr, NX_WAIT_FOREVER)))
    {
        printf("device twin document receive failed!: error code = 0x%08x\r\n", status);
        return;
    }

    /* Loop to receive device twin message. */
    while (true)
    {
        printf("Receive device twin properties: ");
        printf_packet(packet_ptr);

        const CHAR* json_str = (CHAR*)packet_ptr->nx_packet_prepend_ptr;
        const ULONG json_len = packet_ptr->nx_packet_length;

        jsmn_init(&parser);
        token_count = jsmn_parse(&parser, json_str, json_len, tokens, 16);

        findJsonInt(json_str, tokens, token_count, "telemetryInterval", &telemetry_interval);
        tx_thread_wait_abort(&sample_telemetry_thread);

        nx_packet_release(packet_ptr);

        // Wait for a desired property update
        if ((status = nx_azure_iot_hub_client_device_twin_desired_properties_receive(
                 &iothub_client, &packet_ptr, NX_WAIT_FOREVER)))
        {
            printf("Receive desired property receive failed!: error code = "
                   "0x%08x\r\n",
                status);
            break;
        }
    }
}
