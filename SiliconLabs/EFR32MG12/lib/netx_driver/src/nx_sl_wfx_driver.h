/***************************************************************************//**
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef NX_SL_WFX_DRIVER_H
#define NX_SL_WFX_DRIVER_H

#include "sl_wfx.h"
#include "nx_api.h"
#include "nx_arp.h"
#include "nx_rarp.h"

#define NX_SL_WFX_MAX_SSID_LENGTH        32
#define NX_SL_WFX_MAX_PASSWORD_LENGTH    63

typedef struct nx_sl_wfx_wifi_info_s {
  CHAR ssid[NX_SL_WFX_MAX_SSID_LENGTH + 1];
  CHAR password[NX_SL_WFX_MAX_PASSWORD_LENGTH + 1];
  sl_wfx_security_mode_t mode;
} nx_sl_wfx_wifi_info_t;

void nx_sl_wfx_driver_entry(NX_IP_DRIVER *driver_req_ptr);
void nx_sl_driver_receive_callback(sl_wfx_received_ind_t *rx_buffer);

#endif /* NX_SL_WFX_DRIVER_H */
