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

#include "em_gpio.h"
#include "em_usart.h"
#include "em_core.h"
#if defined(SLEXP802X)
#include "brd8022a_pds.h"
#include "brd8023a_pds.h"
#else
#error "WFX200 EXP board type must be specified"
#endif
#include "sl_wfx_wf200_C0.h"
#include "sl_wfx.h"
#include "sl_udelay.h"
#include "sl_wfx_host_gpio.h"
#include "tx_api.h"
#include "nx_sl_wfx_driver.h"
#include <stdio.h>

#define SL_WFX_MAX_SCAN_RESULTS     50
#define SL_WFX_BYTE_POOL_SIZE       4096

typedef struct __attribute__((__packed__)) sl_wfx_scan_result_list_s {
  sl_wfx_ssid_def_t ssid_def;
  uint8_t mac[SL_WFX_MAC_ADDR_SIZE];
  uint16_t channel;
  sl_wfx_security_mode_bitmask_t security_mode;
  uint16_t rcpi;
} sl_wfx_scan_result_list_t;

typedef struct sl_wfx_host_context_s {
  uint32_t      wf200_firmware_download_progress;
  TX_BYTE_POOL  buf_pool;
  uint8_t       waited_event_id;
  uint8_t       posted_event_id;
} sl_wfx_host_context_t;

static sl_wfx_host_context_t     sl_wfx_host_context;
static TX_SEMAPHORE              sl_wfx_confirmation_semaphore;
static TX_MUTEX                  sl_wfx_host_mutex;
static UCHAR                     sl_wfx_memory_area[SL_WFX_BYTE_POOL_SIZE];
static sl_wfx_scan_result_list_t sl_wfx_scan_list[SL_WFX_MAX_SCAN_RESULTS];
static uint8_t                   sl_wfx_scan_count = 0;

/* WF200 host callbacks */
void sl_wfx_connect_callback(uint8_t *mac, uint32_t status);
void sl_wfx_disconnect_callback(uint8_t *mac, uint16_t reason);
void sl_wfx_start_ap_callback(uint32_t status);
void sl_wfx_stop_ap_callback(void);
void sl_wfx_scan_result_callback(sl_wfx_scan_result_ind_body_t *scan_result);
void sl_wfx_scan_complete_callback(uint32_t status);
void sl_wfx_generic_status_callback(sl_wfx_generic_ind_t *frame);
void sl_wfx_client_connected_callback(uint8_t *mac);
void sl_wfx_ap_client_disconnected_callback(uint32_t status, uint8_t *mac);
void sl_wfx_ap_client_rejected_callback(uint32_t status, uint8_t *mac);

static void sl_wfx_host_print_indication_error(uint8_t *indication_error,
                                               uint16_t header_length);

/**************************************************************************//**
 * WFX FMAC driver host interface initialization
 *****************************************************************************/
sl_status_t sl_wfx_host_init(void)
{
  UINT status;

  sl_wfx_host_context.wf200_firmware_download_progress = 0;

  /* Init memory pool for fmac */
  status = tx_byte_pool_create(&(sl_wfx_host_context.buf_pool),
                               "SL WFX Host Buffers",
                               sl_wfx_memory_area,
                               SL_WFX_BYTE_POOL_SIZE);
  if (status != TX_SUCCESS) {
    printf("wfx_host_setup_memory_pools: unable to set up memory pools for wfx\n");
    return SL_STATUS_ALLOCATION_FAILED;
  }

  status = tx_semaphore_create(&sl_wfx_confirmation_semaphore,
                               "wfx confirmation semaphore",
                               0);
  if (status != TX_SUCCESS) {
    printf("OS error: sl_wfx_host_init: sl_wfx_confirmation_semaphore");
    return SL_STATUS_FAIL;
  }

  status = tx_mutex_create(&sl_wfx_host_mutex, "wfx host mutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    printf("OS error: sl_wfx_host_init: sl_wfx_host_mutex");
    return SL_STATUS_FAIL;
  }
 
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Get firmware data
 *****************************************************************************/
sl_status_t sl_wfx_host_get_firmware_data(const uint8_t **data,
                                          uint32_t data_size)
{
  *data = &sl_wfx_firmware[sl_wfx_host_context.wf200_firmware_download_progress];
  sl_wfx_host_context.wf200_firmware_download_progress += data_size;
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Get firmware size
 *****************************************************************************/
sl_status_t sl_wfx_host_get_firmware_size(uint32_t *firmware_size)
{
  *firmware_size = sizeof(sl_wfx_firmware);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Get PDS data
 *****************************************************************************/
sl_status_t sl_wfx_host_get_pds_data(const char **pds_data, uint16_t index)
{
#ifdef SLEXP802X
  /* Manage dynamically the PDS in function of the chip connected */
  if (strncmp("WFM200", (char *)sl_wfx_context->wfx_opn, 6) == 0) {
    *pds_data = pds_table_brd8023a[index];
  } else {
    *pds_data = pds_table_brd8022a[index];
  }
#else
#error "WFX200 EXP board type must be specified"
#endif
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Get PDS size
 *****************************************************************************/
sl_status_t sl_wfx_host_get_pds_size(uint16_t *pds_size)
{
#ifdef SLEXP802X
  /* Manage dynamically the PDS in function of the chip connected */
  if (strncmp("WFM200", (char *)sl_wfx_context->wfx_opn, 6) == 0) {
    *pds_size = SL_WFX_ARRAY_COUNT(pds_table_brd8023a);
  } else {
    *pds_size = SL_WFX_ARRAY_COUNT(pds_table_brd8022a);
  }
  return SL_STATUS_OK;
#else
#error "WFX200 EXP board type must be specified"
#endif
}

/**************************************************************************//**
 * Deinit host interface
 *****************************************************************************/
sl_status_t sl_wfx_host_deinit(void)
{
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Allocate buffer
 *****************************************************************************/
sl_status_t sl_wfx_host_allocate_buffer(void **buffer,
                                        sl_wfx_buffer_type_t type,
                                        uint32_t buffer_size)
{
  UINT status;

  (void)type;
  status = tx_byte_allocate(&(sl_wfx_host_context.buf_pool),
                            (VOID **) buffer,
                            buffer_size,
                            20);
  if (status != TX_SUCCESS) {
    printf("OS error: sl_wfx_host_allocate_buffer\r\n");
    return SL_STATUS_ALLOCATION_FAILED;
  }

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Free host buffer
 *****************************************************************************/
sl_status_t sl_wfx_host_free_buffer(void* buffer, sl_wfx_buffer_type_t type)
{
  UINT status;

  (void)type;
  status = tx_byte_release(buffer);
  if (status != TX_SUCCESS) {
    printf("OS error: sl_wfx_host_free_buffer\r\n");
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Set reset pin low
 *****************************************************************************/
sl_status_t sl_wfx_host_hold_in_reset(void)
{
  GPIO_PinOutClear(SL_WFX_HOST_CFG_RESET_PORT, SL_WFX_HOST_CFG_RESET_PIN);

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Set wakeup pin status
 *****************************************************************************/
sl_status_t sl_wfx_host_set_wake_up_pin(uint8_t state)
{
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();
  if (state > 0) {
   GPIO_PinOutSet(SL_WFX_HOST_CFG_WUP_PORT, SL_WFX_HOST_CFG_WUP_PIN);
  } else {
   GPIO_PinOutClear(SL_WFX_HOST_CFG_WUP_PORT, SL_WFX_HOST_CFG_WUP_PIN);
  }
  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Reset chip
 *****************************************************************************/
sl_status_t sl_wfx_host_reset_chip(void)
{
  /* Pull it low for at least 1 ms to issue a reset sequence */
  GPIO_PinOutClear(SL_WFX_HOST_CFG_RESET_PORT, SL_WFX_HOST_CFG_RESET_PIN);
  /* Delay for 10ms */
  sl_udelay_wait(10000);

  /* Hold pin high to get chip out of reset */
  GPIO_PinOutSet(SL_WFX_HOST_CFG_RESET_PORT, SL_WFX_HOST_CFG_RESET_PIN);
  /* Delay for 3ms */
  sl_udelay_wait(3000);

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Wait for wake up
 *****************************************************************************/
sl_status_t sl_wfx_host_wait_for_wake_up(void)
{
  sl_udelay_wait(2000);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Host wait millisecond
 *****************************************************************************/
sl_status_t sl_wfx_host_wait(uint32_t wait_ms)
{
  sl_udelay_wait(wait_ms * 1000);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Setup waited event
 *****************************************************************************/
sl_status_t sl_wfx_host_setup_waited_event(uint8_t event_id)
{
  sl_wfx_host_context.waited_event_id = event_id;
  sl_wfx_host_context.posted_event_id = 0;

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Wait for confirmation
 *****************************************************************************/
sl_status_t sl_wfx_host_wait_for_confirmation(uint8_t confirmation_id,
                                              uint32_t timeout_ms,
                                              void **event_payload_out)
{
  UINT status;

  while (timeout_ms > 0u) {
    timeout_ms--;
    status = tx_semaphore_get(&sl_wfx_confirmation_semaphore, 1);
    if (status == TX_SUCCESS) {
      if (confirmation_id == sl_wfx_host_context.posted_event_id) {
        if (event_payload_out != NULL) {
          *event_payload_out = sl_wfx_context->event_payload_buffer;
        }
        return SL_STATUS_OK;
      }
    } else if (status != TX_NO_INSTANCE) {
      printf("OS error: sl_wfx_host_wait_for_confirmation\r\n");
    }
  }

  return SL_STATUS_TIMEOUT;
}

/**************************************************************************//**
 * Called when the driver needs to lock its access
 *****************************************************************************/
sl_status_t sl_wfx_host_lock(void)
{
  UINT status;

  /* Check status */
  status = tx_mutex_get(&sl_wfx_host_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Called when the driver needs to unlock its access
 *****************************************************************************/
sl_status_t sl_wfx_host_unlock(void)
{
  UINT status;

  /* Check status */
  status = tx_mutex_put(&sl_wfx_host_mutex);
  if (status != TX_SUCCESS) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Called when the driver needs to post an event
 *****************************************************************************/
sl_status_t sl_wfx_host_post_event(sl_wfx_generic_message_t *event_payload)
{
  switch (event_payload->header.id ) {
    /******** INDICATION ********/
    case SL_WFX_CONNECT_IND_ID:
    {
      sl_wfx_connect_ind_t *connect_indication = (sl_wfx_connect_ind_t*)event_payload;
      sl_wfx_connect_callback(connect_indication->body.mac,
                              connect_indication->body.status);
      break;
    }
    case SL_WFX_DISCONNECT_IND_ID:
    {
      sl_wfx_disconnect_ind_t* disconnect_indication = (sl_wfx_disconnect_ind_t*)event_payload;
      sl_wfx_disconnect_callback(disconnect_indication->body.mac,
                                 disconnect_indication->body.reason);
      break;
    }
    case SL_WFX_START_AP_IND_ID:
    {
      sl_wfx_start_ap_ind_t* start_ap_indication = (sl_wfx_start_ap_ind_t*)event_payload;
      sl_wfx_start_ap_callback(start_ap_indication->body.status);
      break;
    }
    case SL_WFX_STOP_AP_IND_ID:
    {
      sl_wfx_stop_ap_callback();
      break;
    }
    case SL_WFX_RECEIVED_IND_ID:
    {
      sl_wfx_received_ind_t* ethernet_frame = (sl_wfx_received_ind_t*)event_payload;
      if (ethernet_frame->body.frame_type == 0) {
        nx_sl_driver_receive_callback(ethernet_frame);
      }
      break;
    }
    case SL_WFX_SCAN_RESULT_IND_ID:
    {
      sl_wfx_scan_result_ind_t* scan_result = (sl_wfx_scan_result_ind_t*)event_payload;
      sl_wfx_scan_result_callback(&scan_result->body);
      break;
    }
    case SL_WFX_SCAN_COMPLETE_IND_ID:
    {
      sl_wfx_scan_complete_ind_t* scan_complete = (sl_wfx_scan_complete_ind_t*)event_payload;
      sl_wfx_scan_complete_callback(scan_complete->body.status);
      break;
    }
    case SL_WFX_AP_CLIENT_CONNECTED_IND_ID:
    {
      sl_wfx_ap_client_connected_ind_t* client_connected_indication = (sl_wfx_ap_client_connected_ind_t*)event_payload;
      sl_wfx_client_connected_callback(client_connected_indication->body.mac);
      break;
    }
    case SL_WFX_AP_CLIENT_REJECTED_IND_ID:
    {
      sl_wfx_ap_client_rejected_ind_body_t* ap_client_rejected = (sl_wfx_ap_client_rejected_ind_body_t*)event_payload;
      sl_wfx_ap_client_rejected_callback(ap_client_rejected->reason,
                                         ap_client_rejected->mac);
      break;
    }
    case SL_WFX_AP_CLIENT_DISCONNECTED_IND_ID:
    {
      sl_wfx_ap_client_disconnected_ind_t* ap_client_disconnected_indication = (sl_wfx_ap_client_disconnected_ind_t*)event_payload;
      sl_wfx_ap_client_disconnected_callback(ap_client_disconnected_indication->body.reason,
                                             ap_client_disconnected_indication->body.mac);
      break;
    }
    case SL_WFX_GENERIC_IND_ID:
    {
      sl_wfx_generic_ind_t* generic_status = (sl_wfx_generic_ind_t*)event_payload;
      sl_wfx_generic_status_callback(generic_status);
      break;
    }
    case SL_WFX_EXCEPTION_IND_ID:
    {
      sl_wfx_exception_ind_t *firmware_exception = (sl_wfx_exception_ind_t*)event_payload;
      printf("firmware exception %lu\r\n", firmware_exception->body.reason);
      sl_wfx_host_print_indication_error((uint8_t*)firmware_exception,
                                         firmware_exception->header.length);
      break;
    }
    case SL_WFX_ERROR_IND_ID:
    {
      sl_wfx_error_ind_t *firmware_error = (sl_wfx_error_ind_t*)event_payload;
      printf("firmware error %lu\r\n", firmware_error->body.type);
      sl_wfx_host_print_indication_error((uint8_t*)firmware_error,
                                         firmware_error->header.length);
      break;
    }
    /******** CONFIRMATION ********/
    case SL_WFX_SEND_FRAME_CNF_ID:
    {
      break;
    }
  }

  if (sl_wfx_host_context.waited_event_id == event_payload->header.id) {
    /* Post the event in the queue */
    memcpy(sl_wfx_context->event_payload_buffer,
           (void*) event_payload,
           event_payload->header.length);
    sl_wfx_host_context.posted_event_id = event_payload->header.id;
    tx_semaphore_put(&sl_wfx_confirmation_semaphore);
  }

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Called when the driver needs to transmit a frame
 *****************************************************************************/
sl_status_t sl_wfx_host_transmit_frame(void *frame, uint32_t frame_len)
{
  return sl_wfx_data_write(frame, frame_len);
}

/**************************************************************************//**
 * Callback for individual scan result
 *****************************************************************************/
void sl_wfx_scan_result_callback(sl_wfx_scan_result_ind_body_t *scan_result)
{
  sl_wfx_scan_count++;
  printf(
    "# %2d %2d  %03d %02X:%02X:%02X:%02X:%02X:%02X  %s",
    sl_wfx_scan_count,
    scan_result->channel,
    ((int16_t)(scan_result->rcpi - 220) / 2),
    scan_result->mac[0], scan_result->mac[1],
    scan_result->mac[2], scan_result->mac[3],
    scan_result->mac[4], scan_result->mac[5],
    scan_result->ssid_def.ssid);
  /*Report one AP information*/
  printf("\r\n");
  if (sl_wfx_scan_count <= SL_WFX_MAX_SCAN_RESULTS) {
    sl_wfx_scan_list[sl_wfx_scan_count - 1].ssid_def = scan_result->ssid_def;
    sl_wfx_scan_list[sl_wfx_scan_count - 1].channel = scan_result->channel;
    sl_wfx_scan_list[sl_wfx_scan_count - 1].security_mode = scan_result->security_mode;
    sl_wfx_scan_list[sl_wfx_scan_count - 1].rcpi = scan_result->rcpi;
    memcpy(sl_wfx_scan_list[sl_wfx_scan_count - 1].mac, scan_result->mac, 6);
  }
}

/**************************************************************************//**
 * Callback for scan complete
 *****************************************************************************/
void sl_wfx_scan_complete_callback(uint32_t status)
{
  (void)status;
  sl_wfx_scan_count = 0;
  printf("Scan WIFI complete\r\n");
}

/**************************************************************************//**
 * Callback when station connects
 *****************************************************************************/
void sl_wfx_connect_callback(uint8_t *mac, uint32_t status)
{
  (void)mac;

  switch(status) {
    case WFM_STATUS_SUCCESS:
      printf("Connected\r\n");
      sl_wfx_context->state |= SL_WFX_STA_INTERFACE_CONNECTED;
      break;
    case WFM_STATUS_NO_MATCHING_AP:
      printf("Connection failed, access point not found\r\n");
      break;
    case WFM_STATUS_CONNECTION_ABORTED:
      printf("Connection aborted\r\n");
      break;
    case WFM_STATUS_CONNECTION_TIMEOUT:
      printf("Connection timeout\r\n");
      break;
    case WFM_STATUS_CONNECTION_REJECTED_BY_AP:
      printf("Connection rejected by the access point\r\n");
      break;
    case WFM_STATUS_CONNECTION_AUTH_FAILURE:
      printf("Connection authentication failure\r\n");
      break;
    default:
      printf("Connection attempt error\r\n");
      break;
  }
}

/**************************************************************************//**
 * Callback for station disconnect
 *****************************************************************************/
void sl_wfx_disconnect_callback(uint8_t *mac, uint16_t reason)
{
  (void)mac;
  printf("Disconnected %d\r\n", reason);
  sl_wfx_context->state &= ~SL_WFX_STA_INTERFACE_CONNECTED;
}

/**************************************************************************//**
 * Callback for AP started
 *****************************************************************************/
void sl_wfx_start_ap_callback(uint32_t status)
{
  if (status == 0) {
    printf("AP started\r\n");
    sl_wfx_context->state |= SL_WFX_AP_INTERFACE_UP;
  } else {
    printf("AP start failed\r\n");
  }
}

/**************************************************************************//**
 * Callback for AP stopped
 *****************************************************************************/
void sl_wfx_stop_ap_callback(void)
{
  printf("SoftAP stopped\r\n");
  sl_wfx_context->state &= ~SL_WFX_AP_INTERFACE_UP;
}

/**************************************************************************//**
 * Callback for client connect to AP
 *****************************************************************************/
void sl_wfx_client_connected_callback(uint8_t *mac)
{
  printf("Client connected, MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**************************************************************************//**
 * Callback for client rejected from AP
 *****************************************************************************/
void sl_wfx_ap_client_rejected_callback(uint32_t status, uint8_t *mac)
{
  printf("Client rejected, reason: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
         (int)status, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**************************************************************************//**
 * Callback for AP client disconnect
 *****************************************************************************/
void sl_wfx_ap_client_disconnected_callback(uint32_t status, uint8_t *mac)
{
  printf("Client disconnected, reason: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
         (int)status, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**************************************************************************//**
 * Callback for generic status received
 *****************************************************************************/
void sl_wfx_generic_status_callback(sl_wfx_generic_ind_t *frame)
{
  (void)frame;
  printf("Generic status received\r\n");
}

/**************************************************************************//**
 * Called when the driver is considering putting the WFx in sleep mode
 *****************************************************************************/
sl_status_t sl_wfx_host_sleep_grant(sl_wfx_host_bus_transfer_type_t type,
                                    sl_wfx_register_address_t address,
                                    uint32_t length)
{
  (void)type;
  (void)address;
  (void)length;

  return SL_STATUS_WIFI_SLEEP_GRANTED;
}

#if SL_WFX_DEBUG_MASK
/**************************************************************************//**
 * Print log
 *****************************************************************************/
void sl_wfx_host_log(const char *string, ...)
{
  va_list valist;

  va_start(valist, string);
  vprintf(string, valist);
  va_end(valist);
}
#endif

/**************************************************************************//**
 * Print indication error
 *****************************************************************************/
static void sl_wfx_host_print_indication_error(uint8_t *indication_error,
                                               uint16_t header_length)
{
  for (uint16_t i = 0; i < header_length; i += 16) {
    printf("hif: %.8x:", i);
    for (uint8_t j = 0; (j < 16) && ((i + j) < header_length); j++) {
      printf(" %.2x", *indication_error);
      indication_error++;
    }
    printf("\r\n");
  }
}
