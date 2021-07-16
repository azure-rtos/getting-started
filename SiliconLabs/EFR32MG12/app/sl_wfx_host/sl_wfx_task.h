/***************************************************************************//**
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef SL_WFX_TASK_H
#define SL_WFX_TASK_H

#include "nx_api.h"

#define SL_ETHERNET_SIZE            14

/* Wi-Fi events */
typedef enum {
  SL_WFX_TX_PACKET_AVAILABLE        = (1 << 0),
  SL_WFX_RX_PACKET_AVAILABLE        = (1 << 1)
} sl_wfx_process_event_t;

/* NX Packet Queue */
typedef struct {
  NX_PACKET       *head_ptr;
  NX_PACKET       *tail_ptr;
}sl_wfx_packet_queue_t;

void        sl_wfx_process_thread_start(void);
void        sl_wfx_packet_enqueue(NX_PACKET* packet);
NX_PACKET*  sl_wfx_packet_dequeue(void);
void        sl_wfx_tx_lock(void);
void        sl_wfx_tx_unlock(void);
UINT        sl_wfx_process_notify(sl_wfx_process_event_t event_flag);

#endif /* SL_WFX_TASK_H */
