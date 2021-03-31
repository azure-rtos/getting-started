/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#ifndef _MQTT_H
#define _MQTT_H

#include "tx_api.h"
#include "nx_api.h"
#include "nxd_dns.h"
#include "device_config.h"

UINT azure_iot_mqtt_entry(NX_IP* ip_ptr, NX_PACKET_POOL* pool_ptr, NX_DNS* dns_ptr, ULONG (*sntp_time_get)(VOID), DevConfig_IoT_Info_t* device_info);

#endif // _MQTT_H
