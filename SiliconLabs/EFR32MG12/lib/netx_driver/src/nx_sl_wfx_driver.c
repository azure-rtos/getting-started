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

#include "nx_sl_wfx_driver.h"
#include "sl_wfx_task.h"

/* Define the Link MTU. Note this is not the same as the IP MTU. The Link MTU
   includes the addition of the Physical Network header (usually Ethernet). This
   should be larger than the IP instance MTU by the size of the physical header. */
#define NX_LINK_MTU             1514
#define NX_ETHERNET_SIZE        SL_ETHERNET_SIZE
#define NX_ETHERNET_IP          0x0800
#define NX_ETHERNET_ARP         0x0806
#define NX_ETHERNET_RARP        0x8035
#define NX_ETHERNET_IPV6        0x86DD
#define NX_DRIVER_ERROR         0x5A

/* Maximum retry number for connecting to Wifi access point */
#define NX_DRIVER_JOIN_MAX_CNT  10

static sl_wfx_context_t nx_sl_wfx_context;
static NX_PACKET_POOL   *nx_sl_pool_ptr = NULL;
static NX_IP            *nx_sl_ip_ptr   = NULL;

/* Define the routines for processing each driver entry request */
static UINT nx_sl_driver_initialize(NX_IP_DRIVER *driver_req_ptr);
static UINT nx_sl_driver_uninitialize(NX_IP_DRIVER *driver_req_ptr);
static UINT nx_sl_driver_enable(NX_IP_DRIVER *driver_req_ptr);
static UINT nx_sl_driver_disable(NX_IP_DRIVER *driver_req_ptr);
static UINT nx_sl_driver_packet_send(NX_IP_DRIVER *driver_req_ptr);

/* Define the prototypes for the hardware implementation of this driver */
static UINT nx_sl_driver_hardware_initialize(void);
static UINT nx_sl_driver_hardware_enable(CHAR *ssid,
                                         CHAR *password,
                                         sl_wfx_security_mode_t mode);
static UINT nx_sl_driver_packet_enqueue(NX_PACKET *packet_ptr);
static void nx_sl_driver_transfer_to_netx(NX_IP *ip_ptr,
                                          NX_PACKET *packet_ptr);

/**************************************************************************//**
 * Network driver entry function
 *****************************************************************************/
void nx_sl_wfx_driver_entry(NX_IP_DRIVER *driver_req_ptr)
{
  UINT error_code = NX_SUCCESS;

  /* Default to successful return */
  driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;

  /* Process according to the driver request type in the IP control block */
  switch (driver_req_ptr->nx_ip_driver_command)
  {
    case NX_LINK_INTERFACE_ATTACH:
      /* Process driver link attach. Unsupported feature */
      break;

    case NX_LINK_INTERFACE_DETACH:
      /* Process driver link detach. Unsupported feature */
      break;

    case NX_LINK_INITIALIZE:
      /* Process driver link initialize */
      error_code = nx_sl_driver_initialize(driver_req_ptr);
      break;

    case NX_LINK_UNINITIALIZE:
      /* Process driver link uninitialize */
      error_code = nx_sl_driver_uninitialize(driver_req_ptr);
      break;

    case NX_LINK_ENABLE:
      /* Process driver link enable */
      error_code = nx_sl_driver_enable(driver_req_ptr);
      break;

    case NX_LINK_DISABLE:
      /* Process driver link disable */
      error_code = nx_sl_driver_disable(driver_req_ptr);
      break;

    case NX_LINK_PACKET_SEND:
    case NX_LINK_PACKET_BROADCAST:
    case NX_LINK_ARP_SEND:
    case NX_LINK_ARP_RESPONSE_SEND:
    case NX_LINK_RARP_SEND:
      /* Process packet send requests */
      error_code = nx_sl_driver_packet_send(driver_req_ptr);
      break;

    case NX_LINK_MULTICAST_JOIN:
      /* Process driver multicast join. Unsupported feature */
      break;

    case NX_LINK_MULTICAST_LEAVE:
      /* Process driver multicast leave. Unsupported feature */
      break;

    case NX_LINK_GET_STATUS:
      /* Process driver get link status. Unsupported feature */
      break;

    case NX_LINK_GET_SPEED:
      /* Return the link's line speed in the supplied return pointer. Unsupported feature */
      *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
      break;

    case NX_LINK_GET_DUPLEX_TYPE:
      /* Return the link's line speed in the supplied return pointer. Unsupported feature */
      *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
      break;

    case NX_LINK_GET_ERROR_COUNT:
      /* Return the link's line speed in the supplied return pointer. Unsupported feature */
      *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
      break;

    case NX_LINK_GET_RX_COUNT:
      /* Return the link's line speed in the supplied return pointer. Unsupported feature */
      *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
      break;

    case NX_LINK_GET_TX_COUNT:
      /* Return the link's line speed in the supplied return pointer. Unsupported feature */
      *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
      break;

    case NX_LINK_GET_ALLOC_ERRORS:
      /* Return the link's line speed in the supplied return pointer. Unsupported feature */
      *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
      break;

    case NX_LINK_DEFERRED_PROCESSING:
      /* Process driver link deferred processing. Unsupported feature */
      break;

    case NX_LINK_SET_PHYSICAL_ADDRESS:
      /* Process driver link set physical address. Unsupported feature */
      break;

    default:
      /* Return the unhandled command status */
      driver_req_ptr->nx_ip_driver_status = NX_UNHANDLED_COMMAND;
      break;
  }

  if (error_code != NX_SUCCESS) {
    driver_req_ptr->nx_ip_driver_status = NX_NOT_SUCCESSFUL;
  }
}

/**************************************************************************//**
 * Receive incoming packet from WF200
 *****************************************************************************/
void nx_sl_driver_receive_callback(sl_wfx_received_ind_t *rx_buffer)
{
  NX_PACKET *packet_ptr;
  UCHAR     *packet_buffer;
  UINT      status;

  /* Allocate a NX_PACKET to be passed to the IP stack */
  status = nx_packet_allocate(nx_sl_pool_ptr, &packet_ptr, NX_IP_PACKET, TX_WAIT_FOREVER);
  if (status != NX_SUCCESS) {
    printf("nx_sl_driver_receive_callback: unable to allocate memory for receive packet\n");
    return;
  }

  packet_buffer = (UCHAR *)&(rx_buffer->body.frame[rx_buffer->body.frame_padding]);
  /* Setup the ethernet frame pointer to build the ethernet frame. Backup another 2
     bytes to get 32-bit word alignment. */
  packet_buffer = (UCHAR *)(packet_buffer - 2);
  status = nx_packet_data_append(packet_ptr,
                                 packet_buffer,
                                 rx_buffer->body.frame_length + 2,
                                 nx_sl_pool_ptr,
                                 TX_WAIT_FOREVER);
  if (status != NX_SUCCESS) {
    printf("nx_sl_driver_receive_callback: packet append error\n");
    return;
  }
  /* Clean off the offset */
  packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr + 2;

  /* Adjust the packet length */
  packet_ptr->nx_packet_length = packet_ptr->nx_packet_length - 2;

  nx_sl_driver_transfer_to_netx(nx_sl_ip_ptr, packet_ptr);
}

/**************************************************************************//**
 * Handle the driver initialize request
 *****************************************************************************/
static UINT nx_sl_driver_initialize(NX_IP_DRIVER *driver_req_ptr)
{
  UINT  interface_index;
  ULONG mac_addr_lsw, mac_addr_msw;
  UINT  error_code;

  /* Setup the IP pointer from the driver request */
  nx_sl_ip_ptr = driver_req_ptr->nx_ip_driver_ptr;

  /* Obtain the index number of the network interface */
  interface_index = driver_req_ptr->nx_ip_driver_interface->nx_interface_index;

  /* Setup the default packet pool for the driver's received packets */
  nx_sl_pool_ptr = nx_sl_ip_ptr->nx_ip_default_packet_pool;

  /* Initialize the Ethernet controller */
  error_code = nx_sl_driver_hardware_initialize();

  if (error_code != NX_SUCCESS) {
    return error_code;
  }

  /* Once the Ethernet controller is initialized, the driver needs to
     configure the NetX Interface Control block, as outlined below */

  /* The nx_interface_ip_mtu_size should be the MTU for the IP payload.
     For regular Ethernet, the IP MTU is 1500 */
  nx_ip_interface_mtu_set(nx_sl_ip_ptr, interface_index, (NX_LINK_MTU - NX_ETHERNET_SIZE));

  /* Set the physical address (MAC address) of this IP instance */
  mac_addr_msw = (ULONG)((nx_sl_wfx_context.mac_addr_0.octet[0] << 8)
                         + nx_sl_wfx_context.mac_addr_0.octet[1]);
  mac_addr_lsw = (ULONG)((nx_sl_wfx_context.mac_addr_0.octet[2] << 24)
                         + (nx_sl_wfx_context.mac_addr_0.octet[3] << 16)
                         + (nx_sl_wfx_context.mac_addr_0.octet[4] << 8)
                         + (nx_sl_wfx_context.mac_addr_0.octet[5]));

  nx_ip_interface_physical_address_set(nx_sl_ip_ptr, interface_index,
                                       mac_addr_msw, mac_addr_lsw, NX_FALSE);

  /* Indicate to the IP software that IP to physical mapping is required */
  nx_ip_interface_address_mapping_configure(nx_sl_ip_ptr, interface_index, NX_TRUE);
  return error_code;
}

/**************************************************************************//**
 * Initialize WF200 hardware
 *****************************************************************************/
static UINT nx_sl_driver_hardware_initialize(void)
{
  UINT        status = NX_SUCCESS;
  sl_status_t result;

  /* Create wifi receiving thread */
  sl_wfx_process_thread_start();

  /* Init WF200 */
  result = sl_wfx_init(&nx_sl_wfx_context);
  switch(result) {
    case SL_STATUS_OK:
      nx_sl_wfx_context.state = SL_WFX_STARTED;
      printf("WF200 Firmware version %d.%d.%d\r\n",
    		 nx_sl_wfx_context.firmware_major,
    		 nx_sl_wfx_context.firmware_minor,
    		 nx_sl_wfx_context.firmware_build);
      printf("WF200 initialization successful\r\n");
      break;

    case SL_STATUS_WIFI_INVALID_KEY:
      printf("Failed to init WF200: Firmware keyset invalid\r\n");
      break;

    case SL_STATUS_WIFI_FIRMWARE_DOWNLOAD_TIMEOUT:
      printf("Failed to init WF200: Firmware download timeout\r\n");
      break;

    case SL_STATUS_TIMEOUT:
      printf("Failed to init WF200: Poll for value timeout\r\n");
      break;

    case SL_STATUS_FAIL:
      printf("Failed to init WF200: Error\r\n");
      break;

    default :
      printf("Failed to init WF200: Unknown error\r\n");
  }

  if (result != SL_STATUS_OK) {
    status = NX_DRIVER_ERROR;
  }
  return status;
}

/**************************************************************************//**
 * Handle the link enable request
 *****************************************************************************/
static UINT nx_sl_driver_enable(NX_IP_DRIVER *driver_req_ptr)
{
  UINT                  error_code;
  nx_sl_wfx_wifi_info_t *wifi_info_ptr = NULL;

  /* Get the wifi connection info from application */
  wifi_info_ptr = (nx_sl_wfx_wifi_info_t *)driver_req_ptr->nx_ip_driver_interface->nx_interface_additional_link_info;

  if (wifi_info_ptr != NULL) {
    error_code = nx_sl_driver_hardware_enable(wifi_info_ptr->ssid,
                                              wifi_info_ptr->password,
                                              wifi_info_ptr->mode);
  } else {
    printf("[Error] Wifi ssid and password not set\r\n");
    error_code = NX_DRIVER_ERROR;
  }

  if (error_code == NX_SUCCESS) {
    driver_req_ptr->nx_ip_driver_interface->nx_interface_link_up = NX_TRUE;
  }

  return error_code;
}

/**************************************************************************//**
 * Connect WF200 to the Access Point (AP)
 *****************************************************************************/
static UINT nx_sl_driver_hardware_enable(CHAR* ssid, CHAR* password, sl_wfx_security_mode_t mode)
{
  UINT retry_count = 0;
  UINT error_code  = NX_SUCCESS;

  printf("Connecting to SSID: %s\n", ssid);

  /* Try connect access point */
  do {
    /* Send join command to access an access point */
    sl_wfx_send_join_command((uint8_t*) ssid,
                             strlen(ssid),
                             NULL,
                             0,
                             mode,
                             0,
                             0,
                             (uint8_t*) password,
                             strlen(password),
                             NULL, 0);
    retry_count++;
    if (retry_count > NX_DRIVER_JOIN_MAX_CNT) {
      error_code = NX_DRIVER_ERROR;
      break;
    }
    /* Wait to check connected flag */
    tx_thread_sleep(300);
  } while ((nx_sl_wfx_context.state & SL_WFX_STA_INTERFACE_CONNECTED) == 0);

  return error_code;
}

/**************************************************************************//**
 * Handle the driver packet send request
 *****************************************************************************/
static UINT nx_sl_driver_packet_send(NX_IP_DRIVER *driver_req_ptr)
{
  NX_PACKET    *packet_ptr;
  ULONG        *ethernet_frame_ptr;
  NX_INTERFACE *interface_ptr;

  /* Setup interface pointer */
  interface_ptr = driver_req_ptr->nx_ip_driver_interface;

  /* Place the ethernet frame at the front of the packet */
  packet_ptr = driver_req_ptr->nx_ip_driver_packet;

  /* Adjust the prepend pointer */
  packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr - NX_ETHERNET_SIZE;

  /* Adjust the packet length */
  packet_ptr->nx_packet_length = packet_ptr->nx_packet_length + NX_ETHERNET_SIZE;

  /* Setup the ethernet frame pointer to build the ethernet frame. Backup another 2
     bytes to get 32-bit word alignment. */
  /*lint -e{927} -e{826} suppress cast of pointer to pointer, since it is necessary */
  ethernet_frame_ptr = (ULONG *)(packet_ptr->nx_packet_prepend_ptr - 2);

  /* Build the ethernet frame */
  *ethernet_frame_ptr       = driver_req_ptr->nx_ip_driver_physical_address_msw;
  *(ethernet_frame_ptr + 1) = driver_req_ptr->nx_ip_driver_physical_address_lsw;
  *(ethernet_frame_ptr + 2) = (interface_ptr->nx_interface_physical_address_msw << 16)
                               | (interface_ptr->nx_interface_physical_address_lsw >> 16);
  *(ethernet_frame_ptr + 3) = (interface_ptr->nx_interface_physical_address_lsw << 16);

  if (driver_req_ptr->nx_ip_driver_command == NX_LINK_ARP_SEND) {
      *(ethernet_frame_ptr + 3) |= NX_ETHERNET_ARP;
  } else if (driver_req_ptr->nx_ip_driver_command == NX_LINK_ARP_RESPONSE_SEND) {
      *(ethernet_frame_ptr + 3) |= NX_ETHERNET_ARP;
  } else if (driver_req_ptr->nx_ip_driver_command == NX_LINK_RARP_SEND) {
      *(ethernet_frame_ptr + 3) |= NX_ETHERNET_RARP;
  } else if (packet_ptr->nx_packet_ip_version == 4) {
      *(ethernet_frame_ptr + 3) |= NX_ETHERNET_IP;
  } else {
      *(ethernet_frame_ptr + 3) |= NX_ETHERNET_IPV6;
  }

  /* Endian swapping if NX_LITTLE_ENDIAN is defined */
  NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr));
  NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr + 1));
  NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr + 2));
  NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr + 3));

  /* Push the packet to the tx packet queue and notify the wfx process thread */
  return (nx_sl_driver_packet_enqueue(packet_ptr));
}

/**************************************************************************//**
 * Push packet to the tx packet queue and notity the wfx process thread
 *****************************************************************************/
static UINT nx_sl_driver_packet_enqueue(NX_PACKET *packet_ptr)
{
  UINT status = NX_SUCCESS;

  /* Wait for tx packet queue is available if wfx process thread is using it */
  sl_wfx_tx_lock();

  /* Push packet to the tx packet queue */
  sl_wfx_packet_enqueue(packet_ptr);

  /* Notify the wfx process thread */
  status = sl_wfx_process_notify(SL_WFX_TX_PACKET_AVAILABLE);

  /* Release tx packet queue for wfx process thread to use */
  sl_wfx_tx_unlock();

  return status;
}

/**************************************************************************//**
 * Handle the driver uninitialize request
 *****************************************************************************/
static UINT nx_sl_driver_uninitialize(NX_IP_DRIVER *driver_req_ptr)
{
  (void)driver_req_ptr;

  /* Zero out the driver instance */
  memset(&nx_sl_wfx_context, 0, sizeof(sl_wfx_context_t));
  return NX_SUCCESS;
}

/**************************************************************************//**
 * Handle the driver disable request
 *****************************************************************************/
static UINT nx_sl_driver_disable(NX_IP_DRIVER *driver_req_ptr)
{
  sl_status_t result = SL_STATUS_OK;
  UINT        error_code;

  result = sl_wfx_send_disconnect_command();
  if (result == SL_STATUS_OK) {
    driver_req_ptr->nx_ip_driver_interface->nx_interface_link_up = NX_FALSE;
    error_code = NX_SUCCESS;
  } else {
    error_code = NX_DRIVER_ERROR;
  }

  return error_code;
}

/**************************************************************************//**
 * Transfer packet received from WF200 to IP stack
 *****************************************************************************/
static void nx_sl_driver_transfer_to_netx(NX_IP *ip_ptr, NX_PACKET *packet_ptr)
{
  UINT packet_type;

  /* Pickup the packet header to determine where the packet needs to be sent */
  packet_type = (((UINT)(*(packet_ptr->nx_packet_prepend_ptr + 12))) << 8)
                  | ((UINT)(*(packet_ptr->nx_packet_prepend_ptr + 13)));
  /* Setup interface pointer */
  packet_ptr->nx_packet_address.nx_packet_interface_ptr = &ip_ptr->nx_ip_interface[0];

  /* Route the incoming packet according to its ethernet type */
  if ((packet_type == NX_ETHERNET_IP) || (packet_type == NX_ETHERNET_IPV6)) {

    /* Note: The length reported by some Ethernet hardware includes bytes after the packet
       as well as the Ethernet header. In some cases, the actual packet length after the
       Ethernet header should be derived from the length in the IP header (lower 16 bits of
       the first 32-bit word). */

    /* Clean off the Ethernet header */
    packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr + NX_ETHERNET_SIZE;

    /* Adjust the packet length */
    packet_ptr->nx_packet_length = packet_ptr->nx_packet_length - NX_ETHERNET_SIZE;

    /* Route to the ip receive function */
    _nx_ip_packet_receive(ip_ptr, packet_ptr);
  }
#ifndef NX_DISABLE_IPV4
  else if (packet_type == NX_ETHERNET_ARP) {
    /* Clean off the Ethernet header */
    packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr + NX_ETHERNET_SIZE;

    /* Adjust the packet length */
    packet_ptr->nx_packet_length = packet_ptr->nx_packet_length - NX_ETHERNET_SIZE;

    /* Route to the ARP receive function */
    _nx_arp_packet_receive(ip_ptr, packet_ptr);
  } else if (packet_type == NX_ETHERNET_RARP) {
    /* Clean off the Ethernet header */
    packet_ptr->nx_packet_prepend_ptr = packet_ptr->nx_packet_prepend_ptr + NX_ETHERNET_SIZE;

    /* Adjust the packet length */
    packet_ptr->nx_packet_length = packet_ptr->nx_packet_length - NX_ETHERNET_SIZE;
    /* Route to the RARP receive function */
    _nx_rarp_packet_receive(ip_ptr, packet_ptr);
  }
#endif /* !NX_DISABLE_IPV4 */
  else {
    /* Invalid ethernet header... release the packet */
    nx_packet_release(packet_ptr);
  }
}
