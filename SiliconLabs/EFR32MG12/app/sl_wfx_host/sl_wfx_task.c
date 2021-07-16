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

#include "sl_wfx.h"
#include "sl_wfx_task.h"
#include "nx_api.h"

#define SL_WFX_PROCESS_THREAD_PRIORITY        1
#define SL_WFX_PROCESS_THREAD_STACK_SIZE      2048

static TX_THREAD              sl_wfx_process_thread;
static UCHAR                  sl_wfx_process_thread_stack[SL_WFX_PROCESS_THREAD_STACK_SIZE];
static TX_EVENT_FLAGS_GROUP   sl_wfx_event;
static TX_MUTEX               sl_wfx_tx_mutex;
static sl_wfx_packet_queue_t  sl_wfx_tx_queue_context;

static void        sl_wfx_process_thread_entry(ULONG thread_input);
static sl_status_t sl_wfx_tx_process (void);
static sl_status_t sl_wfx_rx_process (uint16_t control_register);
static void        sl_wfx_packet_queue_initialize(void);
static UINT        sl_wfx_packet_queue_is_empty(void);

/**************************************************************************//**
 * Init wfx process thread
 *****************************************************************************/
void sl_wfx_process_thread_start(void)
{
  UINT    status;

  /* Create thread for process received/sent frame from/to WF200 */
  status = tx_thread_create(&sl_wfx_process_thread,
                            "wfx process thread",
                            sl_wfx_process_thread_entry,
                            0,
                            sl_wfx_process_thread_stack,
                            SL_WFX_PROCESS_THREAD_STACK_SIZE,
                            SL_WFX_PROCESS_THREAD_PRIORITY,
                            SL_WFX_PROCESS_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    printf("[Error] Create wfx process thread fail\r\n");
  }
}

/**************************************************************************//**
 * Wfx process thread entry
 *****************************************************************************/
static void sl_wfx_process_thread_entry(ULONG thread_input)
{
  uint16_t    control_register = 0;
  ULONG       event_flag;
  UINT        status;

  (void)thread_input;

  /* Create wfx event group */
  status = tx_event_flags_create(&sl_wfx_event, "wfx event group");
  if (status != TX_SUCCESS) {
    printf("OS error: sl_wfx_process_thread_entry: sl_wfx_event\n");
    return;
  }

  /* Create wfx tx mutex */
  status = tx_mutex_create(&sl_wfx_tx_mutex, "wfx tx mutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    printf("OS error: sl_wfx_process_thread_entry: sl_wfx_tx_mutex");
    return;
  }

  /* Initialize tx packet queue */
  sl_wfx_packet_queue_initialize();

  while (1) {
    /* Wait for WF200 events */
    status = tx_event_flags_get(&sl_wfx_event,
                                SL_WFX_RX_PACKET_AVAILABLE | SL_WFX_TX_PACKET_AVAILABLE,
                                TX_OR_CLEAR,
                                &event_flag,
                                TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      printf("tx_event_flags_get error: %d\n", status);
      continue;
    }

    if (event_flag & SL_WFX_TX_PACKET_AVAILABLE) {
      /* Process tx packets */
      sl_wfx_tx_process();
    }

    if (event_flag & SL_WFX_RX_PACKET_AVAILABLE) {
      /* Process received frames from WF200 */
      sl_wfx_rx_process(control_register);
    }
  }
}

/**************************************************************************//**
 * Wfx process receive frame
 *****************************************************************************/
static sl_status_t sl_wfx_rx_process(uint16_t control_register)
{
  sl_status_t result;

  result = sl_wfx_receive_frame(&control_register);
  if (result != SL_STATUS_OK) {
    printf("sl_wfx_receive_frame error: %ld\n", result);
  } else {
    /* If a packet is still available in the WF200, set an RX event */
    if ((control_register & SL_WFX_CONT_NEXT_LEN_MASK) != 0) {
      sl_wfx_process_notify(SL_WFX_RX_PACKET_AVAILABLE);
    }
  }
  return result;
}

/**************************************************************************//**
 * Wfx process tx queue
 *****************************************************************************/
static sl_status_t sl_wfx_tx_process(void)
{
  sl_status_t             result;
  sl_wfx_send_frame_req_t *wfx_tx_buffer;
  ULONG                   wfx_tx_length;
  NX_PACKET               *packet_index;
  NX_PACKET               *packet_ptr;

  /* Lock the tx packet queue to prevent queue is changed while sending to WF200 */
  sl_wfx_tx_lock();

  /* While packets are available in the tx packet queue, send them */
  while (sl_wfx_packet_queue_is_empty() != NX_TRUE) {
    /* Pop a transmit packet from tx packet queue */
    packet_ptr = sl_wfx_packet_dequeue();
    if (packet_ptr == NX_NULL) {
      break;
    }

    /* Process packet chain */
    for (packet_index = packet_ptr; packet_index != NX_NULL; packet_index = packet_index->nx_packet_next) {
      /* Send sub-packet to WF200 */
      wfx_tx_buffer = (sl_wfx_send_frame_req_t*) (packet_index->nx_packet_prepend_ptr - sizeof(sl_wfx_send_frame_req_t));
      wfx_tx_length = (packet_index->nx_packet_append_ptr - packet_index->nx_packet_prepend_ptr);

      /* Call FMAC function to transfer data to WF200 */
      result = sl_wfx_send_ethernet_frame(wfx_tx_buffer,
                                          wfx_tx_length,
                                          SL_WFX_STA_INTERFACE,
                                          WFM_PRIORITY_BE0);

      /* If the packet is not successfully sent, set that tx packet is available and return */
      if (result != SL_STATUS_OK) {
        printf("Ethernet frame send error!\n");
        sl_wfx_process_notify(SL_WFX_TX_PACKET_AVAILABLE);

        /* Unlock tx packet queue then return the error code */
        sl_wfx_tx_unlock();
        return result;
      }
    }

    /* Remove the Ethernet header */
    packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr + SL_ETHERNET_SIZE;

    /* Adjust the packet length */
    packet_ptr->nx_packet_length = packet_ptr->nx_packet_length - SL_ETHERNET_SIZE;

    /* Now that the Ethernet frame has been removed, release the packet */
    nx_packet_transmit_release(packet_ptr);
  }

  /* Unlock tx packet queue */
  sl_wfx_tx_unlock();

  return result;
}

/**************************************************************************//**
 * Initialize tx packet queue
 *****************************************************************************/
static void sl_wfx_packet_queue_initialize(void)
{
  sl_wfx_tx_queue_context.head_ptr = NX_NULL;
  sl_wfx_tx_queue_context.tail_ptr = NX_NULL;
}

/**************************************************************************//**
 * Check the status of tx queue
 *****************************************************************************/
static UINT sl_wfx_packet_queue_is_empty(void)
{
  /* Tx packet queue is not empty if the head pointer is not NULL */
  if (sl_wfx_tx_queue_context.head_ptr != NX_NULL) {
    return NX_FALSE;
  } else {
    /* If the tx packet queue is empty, mark tail pointer to NULL */
    sl_wfx_tx_queue_context.tail_ptr = NX_NULL;
  }

  return NX_TRUE;
}

/**************************************************************************//**
 * Push a packet to tx packet queue
 *****************************************************************************/
void sl_wfx_packet_enqueue(NX_PACKET *packet_ptr)
{
  /* Set the packet's next pointer to NULL */
  packet_ptr->nx_packet_queue_next = NX_NULL;

  /* Determine if there is anything on the tx packet queue */
  if (sl_wfx_tx_queue_context.tail_ptr != NX_NULL) {
    /* If tx packet queue is not empty, add the new packet to the tail */
    sl_wfx_tx_queue_context.tail_ptr->nx_packet_queue_next = packet_ptr;

    /* Update the tail pointer */
    sl_wfx_tx_queue_context.tail_ptr = packet_ptr;
  } else {
    /* If tx packet queue is empty, setup head & tail pointers */
    sl_wfx_tx_queue_context.head_ptr = packet_ptr;
    sl_wfx_tx_queue_context.tail_ptr = packet_ptr;
  }
}

/**************************************************************************//**
 * Pop a packet from tx packet queue
 *****************************************************************************/
NX_PACKET* sl_wfx_packet_dequeue(void)
{
  NX_PACKET   *packet_ptr = NX_NULL;

  /* Pickup the head pointer of the tx packet queue */
  packet_ptr = sl_wfx_tx_queue_context.head_ptr;

  /* Determine if there is anything on the tx packet queue */
  if (packet_ptr != NX_NULL) {
    /* Update the head pointer */
    sl_wfx_tx_queue_context.head_ptr = packet_ptr->nx_packet_queue_next;

    /* Clear the next pointer in the packet */
    packet_ptr->nx_packet_queue_next = NX_NULL;
  } else {
    /* If tx packet queue is empty, mark tail pointer to NULL */
    sl_wfx_tx_queue_context.tail_ptr = NX_NULL;
  }

  /* Return the packet pointer - NULL if there are no packets queued */
  return(packet_ptr);
}

/**************************************************************************//**
 * Lock wfx tx mutex
 *****************************************************************************/
void sl_wfx_tx_lock(void)
{
  UINT status;

  status = tx_mutex_get(&sl_wfx_tx_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    printf("tx_mutex_get error %d\n", status);
  }
}

/**************************************************************************//**
 * Unlock wfx tx mutex
 *****************************************************************************/
void sl_wfx_tx_unlock(void)
{
  UINT status;

  status = tx_mutex_put(&sl_wfx_tx_mutex);
  if (status != TX_SUCCESS) {
    printf("tx_mutex_put error %d\n", status);
  }
}

/**************************************************************************//**
 * Wfx set ethernet event
 *****************************************************************************/
UINT sl_wfx_process_notify(sl_wfx_process_event_t event_flag)
{
  UINT status = TX_SUCCESS;

  status = tx_event_flags_set(&sl_wfx_event, event_flag, TX_OR);
  if (status != TX_SUCCESS) {
    printf("tx_event_flags_set error: %d\n", status);
  }
  return status;
}
