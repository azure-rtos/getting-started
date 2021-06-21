/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** NetX Component                                                        */
/**                                                                       */
/**   Ethernet driver for TI MSP432E family of microprocessors            */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

/* Indicate that driver source is being compiled.  */

#define NX_DRIVER_SOURCE


/****** DRIVER SPECIFIC ****** Start of part/vendor specific include area.  Include driver-specific include file here!  */

/* Determine if the driver uses IP deferred processing or direct ISR processing.  */

#define NX_DRIVER_ENABLE_DEFERRED                /* Define this to enable deferred ISR processing.  */

/* Include driver specific include file.  */
#include "nx_driver_msp432e.h"


/****** DRIVER SPECIFIC ****** End of part/vendor specific include file area!  */


/* Define the driver information structure that is only available within this file.  */

static NX_DRIVER_INFORMATION   nx_driver_information;


/****** DRIVER SPECIFIC ****** Start of part/vendor specific data area.  Include hardware-specific data here!  */

/* Define driver specific ethernet hardware address.  */

#ifndef NX_DRIVER_ETHERNET_MAC
UCHAR   _nx_driver_hardware_address[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x56};  
#else
UCHAR   _nx_driver_hardware_address[] = NX_DRIVER_ETHERNET_MAC;  
#endif

tEMACDMADescriptor nx_driver_information_dma_rx_descriptors_area[NX_DRIVER_RX_DESCRIPTORS];
tEMACDMADescriptor nx_driver_information_dma_tx_descriptors_area[NX_DRIVER_TX_DESCRIPTORS]; 

/****** DRIVER SPECIFIC ****** End of part/vendor specific data area!  */


/* Define the routines for processing each driver entry request.  The contents of these routines will change with
   each driver. However, the main driver entry function will not change, except for the entry function name.  */
   
static VOID         _nx_driver_interface_attach(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_initialize(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_enable(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_disable(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_packet_send(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_multicast_join(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_multicast_leave(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_get_status(NX_IP_DRIVER *driver_req_ptr);
#ifdef NX_ENABLE_INTERFACE_CAPABILITY
static VOID         _nx_driver_capability_get(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_capability_set(NX_IP_DRIVER *driver_req_ptr);
#endif /* NX_ENABLE_INTERFACE_CAPABILITY */
#ifdef NX_DRIVER_ENABLE_DEFERRED
static VOID         _nx_driver_deferred_processing(NX_IP_DRIVER *driver_req_ptr);
#endif /* NX_DRIVER_ENABLE_DEFERRED */
static VOID         _nx_driver_transfer_to_netx(NX_IP *ip_ptr, NX_PACKET *packet_ptr);

/* Define the prototypes for the hardware implementation of this driver. The contents of these routines are
   driver-specific.  */

static UINT         _nx_driver_hardware_initialize(NX_IP_DRIVER *driver_req_ptr); 
static UINT         _nx_driver_hardware_enable(NX_IP_DRIVER *driver_req_ptr); 
static UINT         _nx_driver_hardware_disable(NX_IP_DRIVER *driver_req_ptr); 
static UINT         _nx_driver_hardware_packet_send(NX_PACKET *packet_ptr); 
static UINT         _nx_driver_hardware_multicast_join(NX_IP_DRIVER *driver_req_ptr);
static UINT         _nx_driver_hardware_multicast_leave(NX_IP_DRIVER *driver_req_ptr);
static UINT         _nx_driver_hardware_get_status(NX_IP_DRIVER *driver_req_ptr);
static VOID         _nx_driver_hardware_packet_transmitted(VOID);
static VOID         _nx_driver_hardware_packet_received(VOID);
#ifdef NX_ENABLE_INTERFACE_CAPABILITY
static UINT         _nx_driver_hardware_capability_set(NX_IP_DRIVER *driver_req_ptr);
#endif /* NX_ENABLE_INTERFACE_CAPABILITY */


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    nx_driver_msp432e                                   PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This is the entry point of the NetX Ethernet Driver. This driver    */ 
/*    function is responsible for initializing the Ethernet controller,   */ 
/*    enabling or disabling the controller as need, preparing             */ 
/*    a packet for transmission, and getting status information.          */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        The driver request from the   */ 
/*                                            IP layer.                   */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_interface_attach           Process attach request        */ 
/*    _nx_driver_initialize                 Process initialize request    */ 
/*    _nx_driver_enable                     Process link enable request   */ 
/*    _nx_driver_disable                    Process link disable request  */ 
/*    _nx_driver_packet_send                Process send packet requests  */ 
/*    _nx_driver_multicast_join             Process multicast join request*/ 
/*    _nx_driver_multicast_leave            Process multicast leave req   */ 
/*    _nx_driver_get_status                 Process get status request    */ 
/*    _nx_driver_deferred_processing        Drive deferred processing     */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    IP layer                                                            */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  03-04-2021     David Vescovi            Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
/****** DRIVER SPECIFIC ****** Start of part/vendor specific global driver entry function name.  */
VOID  nx_driver_msp432e(NX_IP_DRIVER *driver_req_ptr)
/****** DRIVER SPECIFIC ****** End of part/vendor specific global driver entry function name.  */
{
    
    /* Default to successful return.  */
    driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    
    /* Process according to the driver request type in the IP control 
       block.  */
    switch (driver_req_ptr -> nx_ip_driver_command)
    {
        
    case NX_LINK_INTERFACE_ATTACH:
       
        /* Process link interface attach requests.  */
        _nx_driver_interface_attach(driver_req_ptr);
        break;
        
    case NX_LINK_INITIALIZE:
    {

        /* Process link initialize requests.  */
        _nx_driver_initialize(driver_req_ptr);
        break;
    }
        
    case NX_LINK_ENABLE:
    {
    
        /* Process link enable requests.  */
        _nx_driver_enable(driver_req_ptr);
        break;
    }
        
    case NX_LINK_DISABLE:
    {
    
        /* Process link disable requests.  */
        _nx_driver_disable(driver_req_ptr);
        break;
    } 
        
        
    case NX_LINK_ARP_SEND:
    case NX_LINK_ARP_RESPONSE_SEND:
    case NX_LINK_PACKET_BROADCAST:
    case NX_LINK_RARP_SEND:
    case NX_LINK_PACKET_SEND:
    {
            
        /* Process packet send requests.  */
        _nx_driver_packet_send(driver_req_ptr);
        break;
    }
        
        
    case NX_LINK_MULTICAST_JOIN:
    {
            
        /* Process multicast join requests.  */
        _nx_driver_multicast_join(driver_req_ptr);
        break;
    }
    
        
    case NX_LINK_MULTICAST_LEAVE:
    {
            
        /* Process multicast leave requests.  */
        _nx_driver_multicast_leave(driver_req_ptr);
        break;
    }
        
    case NX_LINK_GET_STATUS:
    {
            
        /* Process get status requests.  */
        _nx_driver_get_status(driver_req_ptr);
        break;
    }
#ifdef NX_DRIVER_ENABLE_DEFERRED
    case NX_LINK_DEFERRED_PROCESSING:
    { 

        /* Process driver deferred requests.  */
        
        /* Process a device driver function on behave of the IP thread. */
        _nx_driver_deferred_processing(driver_req_ptr);
        break;
    }
#endif
#ifdef NX_ENABLE_INTERFACE_CAPABILITY
    case NX_INTERFACE_CAPABILITY_GET:
    {

        /* Process get capability requests.  */
        _nx_driver_capability_get(driver_req_ptr);
        break;
    }
    
    case NX_INTERFACE_CAPABILITY_SET:
    {

        /* Process set capability requests.  */
        _nx_driver_capability_set(driver_req_ptr);
        break;
    }
#endif /* NX_ENABLE_INTERFACE_CAPABILITY */
    default:
            
        /* Invalid driver request.  */
        
        /* Return the unhandled command status.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_UNHANDLED_COMMAND;

        /* Default to successful return.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_interface_attach                         PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the interface attach request.              */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_interface_attach(NX_IP_DRIVER *driver_req_ptr)
{


    /* Setup the driver's interface.  This example is for a simple one-interface
       Ethernet driver. Additional logic is necessary for multiple port devices.  */
    nx_driver_information.nx_driver_information_interface =  driver_req_ptr -> nx_ip_driver_interface;

#ifdef NX_ENABLE_INTERFACE_CAPABILITY
    driver_req_ptr -> nx_ip_driver_interface -> nx_interface_capability_flag = NX_DRIVER_CAPABILITY;
#endif /* NX_ENABLE_INTERFACE_CAPABILITY */

    /* Return successful status.  */
    driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_initialize                               PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the initialize request.  The processing    */
/*    in this function is generic. All ethernet controller logic is to    */ 
/*    be placed in _nx_driver_hardware_initialize.                        */  
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_initialize        Process initialize request    */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_initialize(NX_IP_DRIVER *driver_req_ptr)
{

NX_IP           *ip_ptr;
NX_INTERFACE    *interface_ptr;
UINT            status;
        
        
    /* Setup the IP pointer from the driver request.  */
    ip_ptr =  driver_req_ptr -> nx_ip_driver_ptr;
    
    /* Setup interface pointer.  */
    interface_ptr = driver_req_ptr -> nx_ip_driver_interface;

    /* Initialize the driver's information structure.  */

    /* Default IP pointer to NULL.  */
    nx_driver_information.nx_driver_information_ip_ptr =               NX_NULL;

    /* Setup the driver state to not initialized.  */
    nx_driver_information.nx_driver_information_state =                NX_DRIVER_STATE_NOT_INITIALIZED;

    /* Setup the default packet pool for the driver's received packets.  */
    nx_driver_information.nx_driver_information_packet_pool_ptr = ip_ptr -> nx_ip_default_packet_pool;

    /* Clear the deferred events for the driver.  */
    nx_driver_information.nx_driver_information_deferred_events =       0;

    /* Call the hardware-specific ethernet controller initialization.  */
    status =  _nx_driver_hardware_initialize(driver_req_ptr);

    /* Determine if the request was successful.  */
    if (status == NX_SUCCESS)
    {

        /* Successful hardware initialization.  */

        /* Setup driver information to point to IP pointer.  */
        nx_driver_information.nx_driver_information_ip_ptr = driver_req_ptr -> nx_ip_driver_ptr;
            
        /* Setup the link maximum transfer unit. */
        interface_ptr -> nx_interface_ip_mtu_size =  NX_DRIVER_ETHERNET_MTU - NX_DRIVER_ETHERNET_FRAME_SIZE;
            
        /* Setup the physical address of this IP instance.  Increment the 
           physical address lsw to simulate multiple nodes hanging on the
           ethernet.  */
        interface_ptr -> nx_interface_physical_address_msw =  
                (ULONG)((_nx_driver_hardware_address[0] << 8) | (_nx_driver_hardware_address[1]));
        interface_ptr -> nx_interface_physical_address_lsw =  
                (ULONG)((_nx_driver_hardware_address[2] << 24) | (_nx_driver_hardware_address[3] << 16) | 
                        (_nx_driver_hardware_address[4] << 8) | (_nx_driver_hardware_address[5]));
                
        /* Indicate to the IP software that IP to physical mapping
           is required.  */
        interface_ptr -> nx_interface_address_mapping_needed =  NX_TRUE;
            
        /* Move the driver's state to initialized.  */
        nx_driver_information.nx_driver_information_state = NX_DRIVER_STATE_INITIALIZED;

        /* Indicate successful initialize.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
    else
    {
        
        /* Initialization failed.  Indicate that the request failed.  */
        driver_req_ptr -> nx_ip_driver_status =   NX_DRIVER_ERROR;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_enable                                   PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the initialize request. The processing     */
/*    in this function is generic. All ethernet controller logic is to    */ 
/*    be placed in _nx_driver_hardware_enable.                            */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_enable            Process enable request        */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_enable(NX_IP_DRIVER *driver_req_ptr)
{

UINT            status;

    /* See if we can honor the NX_LINK_ENABLE request.  */
    if (nx_driver_information.nx_driver_information_state < NX_DRIVER_STATE_INITIALIZED)
    {

        /* Mark the request as not successful.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;                      
        return;
    }                
        
    /* Check if it is enabled by someone already */
    if (nx_driver_information.nx_driver_information_state >=  NX_DRIVER_STATE_LINK_ENABLED)
    {

        /* Yes, the request has already been made.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_ALREADY_ENABLED;
        return;
    }

    /* Call hardware specific enable.  */
    status =  _nx_driver_hardware_enable(driver_req_ptr);

    /* Was the hardware enable successful?  */
    if (status == NX_SUCCESS)
    {

        /* Update the driver state to link enabled.  */
        nx_driver_information.nx_driver_information_state = NX_DRIVER_STATE_LINK_ENABLED;

        /* Mark request as successful.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
        
        /* Mark the IP instance as link up.  */        
        driver_req_ptr -> nx_ip_driver_interface -> nx_interface_link_up = NX_TRUE;
    }
    else
    {
        
        /* Enable failed.  Indicate that the request failed.  */
        driver_req_ptr -> nx_ip_driver_status =   NX_DRIVER_ERROR;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_disable                                  PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the disable request. The processing        */
/*    in this function is generic. All ethernet controller logic is to    */ 
/*    be placed in _nx_driver_hardware_disable.                           */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_disable           Process disable request       */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_disable(NX_IP_DRIVER *driver_req_ptr)
{

NX_IP           *ip_ptr;
UINT            status;


    /* Setup the IP pointer from the driver request.  */
    ip_ptr =  driver_req_ptr -> nx_ip_driver_ptr;

    /* Check if the link is enabled.  */
    if (nx_driver_information.nx_driver_information_state !=  NX_DRIVER_STATE_LINK_ENABLED)
    {

        /* The link is not enabled, so just return an error.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
        return;
    }

    /* Call hardware specific disable.  */
    status =  _nx_driver_hardware_disable(driver_req_ptr);

    /* Was the hardware disable successful?  */
    if (status == NX_SUCCESS)
    {
    
        /* Mark the IP instance as link down.  */        
        ip_ptr -> nx_ip_driver_link_up =  NX_FALSE;

        /* Update the driver state back to initialized.  */
        nx_driver_information.nx_driver_information_state =  NX_DRIVER_STATE_INITIALIZED;
        
        /* Mark request as successful.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
    else
    {

        /* Disable failed, return an error.  */    
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_packet_send                              PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the packet send request. The processing    */
/*    in this function is generic. All ethernet controller packet send    */ 
/*    logic is to be placed in _nx_driver_hardware_packet_send.           */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_packet_send       Process packet send request   */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_packet_send(NX_IP_DRIVER *driver_req_ptr)
{

NX_PACKET       *packet_ptr;
ULONG           *ethernet_frame_ptr;
UINT            status;


    /* Check to make sure the link is up.  */
    if (nx_driver_information.nx_driver_information_state != NX_DRIVER_STATE_LINK_ENABLED)
    {

        /* Inidate an unsuccessful packet send.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;

        /* Link is not up, simply free the packet.  */
        nx_packet_transmit_release(driver_req_ptr -> nx_ip_driver_packet);
        return;
    }                
    
    /* Process driver send packet.  */
    
    /* Place the ethernet frame at the front of the packet.  */
    packet_ptr =  driver_req_ptr -> nx_ip_driver_packet;
        
    /* Adjust the prepend pointer.  */
    packet_ptr -> nx_packet_prepend_ptr =  
            packet_ptr -> nx_packet_prepend_ptr - NX_DRIVER_ETHERNET_FRAME_SIZE;
        
    /* Adjust the packet length.  */
    packet_ptr -> nx_packet_length = packet_ptr -> nx_packet_length + NX_DRIVER_ETHERNET_FRAME_SIZE;
            
    /* Setup the ethernet frame pointer to build the ethernet frame.  Backup another 2
      * bytes to get 32-bit word alignment.  */
    ethernet_frame_ptr =  (ULONG *) (packet_ptr -> nx_packet_prepend_ptr - 2);
        
    /* Set up the hardware addresses in the Ethernet header. */
    *ethernet_frame_ptr       =  driver_req_ptr -> nx_ip_driver_physical_address_msw;
    *(ethernet_frame_ptr + 1) =  driver_req_ptr -> nx_ip_driver_physical_address_lsw;
        
    *(ethernet_frame_ptr + 2) =  (driver_req_ptr -> nx_ip_driver_interface -> nx_interface_physical_address_msw << 16) |
        (driver_req_ptr -> nx_ip_driver_interface -> nx_interface_physical_address_lsw >> 16);
    *(ethernet_frame_ptr + 3) =  (driver_req_ptr -> nx_ip_driver_interface -> nx_interface_physical_address_lsw << 16);

    /* Set up the frame type field in the Ethernet harder. */
    if ((driver_req_ptr -> nx_ip_driver_command == NX_LINK_ARP_SEND)||
        (driver_req_ptr -> nx_ip_driver_command == NX_LINK_ARP_RESPONSE_SEND))
    {

        *(ethernet_frame_ptr + 3) |= NX_DRIVER_ETHERNET_ARP;
    }
    else if(driver_req_ptr -> nx_ip_driver_command == NX_LINK_RARP_SEND)
    {

        *(ethernet_frame_ptr + 3) |= NX_DRIVER_ETHERNET_RARP;        
    }

#ifdef FEATURE_NX_IPV6
    else if(packet_ptr -> nx_packet_ip_version == NX_IP_VERSION_V6)
    {

        *(ethernet_frame_ptr + 3) |= NX_DRIVER_ETHERNET_IPV6;
    }
#endif /* FEATURE_NX_IPV6 */

    else
    {

        *(ethernet_frame_ptr + 3) |= NX_DRIVER_ETHERNET_IP;
    }

    /* Endian swapping if NX_LITTLE_ENDIAN is defined.  */
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr));
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr + 1));
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr + 2));
    NX_CHANGE_ULONG_ENDIAN(*(ethernet_frame_ptr + 3));
        
    /* Determine if the packet exceeds the driver's MTU.  */
    if (packet_ptr -> nx_packet_length > NX_DRIVER_ETHERNET_MTU)
    {
    
        /* This packet exceeds the size of the driver's MTU. Simply throw it away! */

        /* Remove the Ethernet header.  */
        NX_DRIVER_ETHERNET_HEADER_REMOVE(packet_ptr);
        
        /* Indicate an unsuccessful packet send.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;

        /* Link is not up, simply free the packet.  */
        nx_packet_transmit_release(packet_ptr);
        return;
    }

    /* Transmit the packet through the Ethernet controller low level access routine. */
    status = _nx_driver_hardware_packet_send(packet_ptr);

    /* Determine if there was an error.  */
    if (status != NX_SUCCESS)
    {

        /* Driver's hardware send packet routine failed to send the packet.  */

        /* Remove the Ethernet header.  */
        NX_DRIVER_ETHERNET_HEADER_REMOVE(packet_ptr);
        
        /* Indicate an unsuccessful packet send.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;

        /* Link is not up, simply free the packet.  */
        nx_packet_transmit_release(packet_ptr);
    }
    else
    {

        /* Set the status of the request.  */    
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_multicast_join                           PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the multicast join request. The processing */
/*    in this function is generic. All ethernet controller multicast join */ 
/*    logic is to be placed in _nx_driver_hardware_multicast_join.        */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_multicast_join    Process multicast join request*/ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_multicast_join(NX_IP_DRIVER *driver_req_ptr)
{

UINT        status;


    if (nx_driver_information.nx_driver_information_state >= NX_DRIVER_STATE_INITIALIZED)
    {
        
    /* Call hardware specific multicast join function. */
    status =  _nx_driver_hardware_multicast_join(driver_req_ptr);
    }
    else
    {
        
        status =  NX_NOT_ENABLED;
    }
    
    /* Determine if there was an error.  */
    if (status != NX_SUCCESS)
    {

        /* Indicate an unsuccessful request.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
    }
    else
    {

        /* Indicate the request was successful.   */    
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_multicast_leave                          PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the multicast leave request. The           */ 
/*    processing in this function is generic. All ethernet controller     */ 
/*    multicast leave logic is to be placed in                            */ 
/*    _nx_driver_hardware_multicast_leave.                                */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_multicast_leave   Process multicast leave req   */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_multicast_leave(NX_IP_DRIVER *driver_req_ptr)
{

UINT        status;

    if (nx_driver_information.nx_driver_information_state >= NX_DRIVER_STATE_INITIALIZED)
    {
        
    /* Call hardware specific multicast leave function. */
    status =  _nx_driver_hardware_multicast_leave(driver_req_ptr);
    }
    else
    {
        
        status =  NX_NOT_ENABLED;
    }    
    
    /* Determine if there was an error.  */
    if (status != NX_SUCCESS)
    {

        /* Indicate an unsuccessful request.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
    }
    else
    {

        /* Indicate the request was successful.   */    
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_get_status                               PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the get status request. The processing     */
/*    in this function is generic. All ethernet controller get status     */ 
/*    logic is to be placed in _nx_driver_hardware_get_status.            */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_hardware_get_status        Process get status request    */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_get_status(NX_IP_DRIVER *driver_req_ptr)
{

UINT        status;


    /* Call hardware specific get status function. */
    status =  _nx_driver_hardware_get_status(driver_req_ptr);
    
    /* Determine if there was an error.  */
    if (status != NX_SUCCESS)
    {

        /* Indicate an unsuccessful request.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
    }
    else
    {

        /* Indicate the request was successful.   */    
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
}



#ifdef NX_ENABLE_INTERFACE_CAPABILITY
/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_capability_get                         PORTABLE C        */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the get capability request.                */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  01-01-2014     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_capability_get(NX_IP_DRIVER *driver_req_ptr)
{
    
    /* Return the capability of the Ethernet controller.  */
    *(driver_req_ptr -> nx_ip_driver_return_ptr) = NX_DRIVER_CAPABILITY;
    
    /* Return the success status.  */
    driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_capability_set                         PORTABLE C        */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the set capability request.                */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  01-01-2014     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_capability_set(NX_IP_DRIVER *driver_req_ptr)
{

UINT        status;


    /* Call hardware specific get status function. */
    status =  _nx_driver_hardware_capability_set(driver_req_ptr);
    
    /* Determine if there was an error.  */
    if (status != NX_SUCCESS)
    {

        /* Indicate an unsuccessful request.  */
        driver_req_ptr -> nx_ip_driver_status =  NX_DRIVER_ERROR;
    }
    else
    {

        /* Indicate the request was successful.   */    
        driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;
    }
}
#endif /* NX_ENABLE_INTERFACE_CAPABILITY */



#ifdef NX_DRIVER_ENABLE_DEFERRED
/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_deferred_processing                      PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    XC, Microsoft Corporation                                           */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing the deferred ISR action within the context */ 
/*    of the IP thread.                                                   */
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver command from the IP    */ 
/*                                            thread                      */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_packet_transmitted        Clean up after transmission    */
/*    _nx_driver_packet_received           Process a received packet      */
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Driver entry function                                               */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_deferred_processing(NX_IP_DRIVER *driver_req_ptr)
{

TX_INTERRUPT_SAVE_AREA

ULONG       deferred_events;

    
    /* Disable interrupts.  */
    TX_DISABLE

    /* Pickup deferred events.  */
    deferred_events =  nx_driver_information.nx_driver_information_deferred_events;
    nx_driver_information.nx_driver_information_deferred_events =  0;

    /* Restore interrupts.  */
    TX_RESTORE
        
    /* Check for a transmit complete event.  */
    if(deferred_events & NX_DRIVER_DEFERRED_PACKET_TRANSMITTED)
    {
        
        /* Process transmitted packet(s).  */
        _nx_driver_hardware_packet_transmitted();
    }    
    
    /* Check for received packet.  */
    if(deferred_events & NX_DRIVER_DEFERRED_PACKET_RECEIVED)
    {

        /* Process received packet(s).  */
        _nx_driver_hardware_packet_received();
    }

    /* Mark request as successful.  */    
    driver_req_ptr->nx_ip_driver_status =  NX_SUCCESS;
}
#endif /* NX_DRIVER_ENABLE_DEFERRED */


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_transfer_to_netx                         PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing incoming packets.  This routine would      */
/*    be called from the driver-specific receive packet processing        */ 
/*    function _nx_driver_hardware_packet_received.                       */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    ip_ptr                                Pointer to IP protocol block  */ 
/*    packet_ptr                            Packet pointer                */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    Error indication                                                    */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_ip_packet_receive                 NetX IP packet receive        */ 
/*    _nx_ip_packet_deferred_receive        NetX IP packet receive        */ 
/*    _nx_arp_packet_deferred_receive       NetX ARP packet receive       */ 
/*    _nx_rarp_packet_deferred_receive      NetX RARP packet receive      */ 
/*    _nx_packet_release                    Release packet                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_hardware_packet_received   Driver packet receive function*/ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID _nx_driver_transfer_to_netx(NX_IP *ip_ptr, NX_PACKET *packet_ptr)
{

USHORT    packet_type;


    packet_ptr -> nx_packet_ip_interface = nx_driver_information.nx_driver_information_interface;

    /* Pickup the packet header to determine where the packet needs to be
       sent.  */
    packet_type =  (USHORT)(((UINT) (*(packet_ptr -> nx_packet_prepend_ptr+12))) << 8) | 
        ((UINT) (*(packet_ptr -> nx_packet_prepend_ptr+13)));

    /* Route the incoming packet according to its ethernet type.  */
    if (packet_type == NX_DRIVER_ETHERNET_IP || packet_type == NX_DRIVER_ETHERNET_IPV6)
    {
        /* Note:  The length reported by some Ethernet hardware includes 
           bytes after the packet as well as the Ethernet header.  In some 
           cases, the actual packet length after the Ethernet header should 
           be derived from the length in the IP header (lower 16 bits of
           the first 32-bit word).  */

        /* Clean off the Ethernet header.  */
        packet_ptr -> nx_packet_prepend_ptr =  
            packet_ptr -> nx_packet_prepend_ptr + NX_DRIVER_ETHERNET_FRAME_SIZE;
 
        /* Adjust the packet length.  */
        packet_ptr -> nx_packet_length =  
            packet_ptr -> nx_packet_length - NX_DRIVER_ETHERNET_FRAME_SIZE;

        /* Route to the ip receive function.  */
#ifdef NX_DRIVER_ENABLE_DEFERRED
        _nx_ip_packet_deferred_receive(ip_ptr, packet_ptr);
#else
        _nx_ip_packet_receive(ip_ptr, packet_ptr);
#endif /* NX_DRIVER_ENABLE_DEFERRED */
    }
    else if (packet_type == NX_DRIVER_ETHERNET_ARP)
    {

        /* Clean off the Ethernet header.  */
        packet_ptr -> nx_packet_prepend_ptr =  
            packet_ptr -> nx_packet_prepend_ptr + NX_DRIVER_ETHERNET_FRAME_SIZE;

        /* Adjust the packet length.  */
        packet_ptr -> nx_packet_length =  
            packet_ptr -> nx_packet_length - NX_DRIVER_ETHERNET_FRAME_SIZE;

        /* Route to the ARP receive function.  */
        _nx_arp_packet_deferred_receive(ip_ptr, packet_ptr);
    }
    else if (packet_type == NX_DRIVER_ETHERNET_RARP)
    {

        /* Clean off the Ethernet header.  */
        packet_ptr -> nx_packet_prepend_ptr =  
            packet_ptr -> nx_packet_prepend_ptr + NX_DRIVER_ETHERNET_FRAME_SIZE;

        /* Adjust the packet length.  */
        packet_ptr -> nx_packet_length =  
            packet_ptr -> nx_packet_length - NX_DRIVER_ETHERNET_FRAME_SIZE;

        /* Route to the RARP receive function.  */
        _nx_rarp_packet_deferred_receive(ip_ptr, packet_ptr);
    }
    else
    {
        /* Invalid ethernet header... release the packet.  */
        nx_packet_release(packet_ptr);
    }
}            


/****** DRIVER SPECIFIC ****** Start of part/vendor specific internal driver functions.  */


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_initialize                      PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific initialization.           */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver request pointer        */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    ETH_BSP_Config                        Configure Ethernet            */ 
/*    ETH_MACAddressConfig                  Setup MAC address             */ 
/*    ETH_DMARxDescReceiveITConfig          Enable receive descriptors    */ 
/*    nx_packet_allocate                    Allocate receive packet(s)    */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_initialize                 Driver initialize processing  */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-01-2018     Yuxin Zhou               Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_initialize(NX_IP_DRIVER *driver_req_ptr)
{
  
NX_PACKET *packet_ptr;
UINT i;
uint16_t ui16Val;

    /* Default to successful return.  */
    driver_req_ptr -> nx_ip_driver_status =  NX_SUCCESS;

    /* Set MAC hardware address */
    /* set in low level stat up from eeprom... */
    EMACAddrGet(EMAC0_BASE, 0, &(_nx_driver_hardware_address[0]));
    
    nx_driver_information.nx_driver_information_dma_rx_descriptors = nx_driver_information_dma_rx_descriptors_area;
    nx_driver_information.nx_driver_information_dma_tx_descriptors = nx_driver_information_dma_tx_descriptors_area;

    /* Setup indices.  */
    nx_driver_information.nx_driver_information_receive_current_index = 0; 
    nx_driver_information.nx_driver_information_transmit_current_index = 0; 
    nx_driver_information.nx_driver_information_transmit_release_index = 0;

    /* Clear the number of buffers in use counter.  */
    nx_driver_information.nx_driver_information_number_of_transmit_buffers_in_use = 0;

    /* Make sure there are receive packets... otherwise, return an error.  */
    if (nx_driver_information.nx_driver_information_packet_pool_ptr == NULL)
    {
    
        /* There must be receive packets. If not, return an error!  */
        return(NX_DRIVER_ERROR);
    }

    /* Fill each DMATxDesc descriptor with the right values.  */   
    for(i = 0; i < NX_DRIVER_TX_DESCRIPTORS; i++)
    {
       nx_driver_information.nx_driver_information_dma_tx_descriptors[i].ui32Count = 0;
       nx_driver_information.nx_driver_information_dma_tx_descriptors[i].pvBuffer1 = 0;
       nx_driver_information.nx_driver_information_dma_tx_descriptors[i].DES3.pLink =
               ((i == (NX_DRIVER_TX_DESCRIPTORS - 1)) ?
               &nx_driver_information.nx_driver_information_dma_tx_descriptors[0] : &nx_driver_information.nx_driver_information_dma_tx_descriptors[i + 1]);
       nx_driver_information.nx_driver_information_dma_tx_descriptors[i].ui32CtrlStatus = DES0_TX_CTRL_INTERRUPT |
               DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_IP_ALL_CKHSUMS;
       nx_driver_information.nx_driver_information_transmit_packets[i] = NX_NULL;       
    }


    for(i = 0; i < NX_DRIVER_RX_DESCRIPTORS; i++)
    {
        /* Allocate a packet for the receive buffers.  */
        if (nx_packet_allocate(nx_driver_information.nx_driver_information_packet_pool_ptr, &packet_ptr, 
                               NX_RECEIVE_PACKET, NX_NO_WAIT) == NX_SUCCESS)
        {
            /* Adjust the new packet for alignment and assign it to the BD.  */
            packet_ptr -> nx_packet_prepend_ptr += 2;
            nx_driver_information.nx_driver_information_dma_rx_descriptors[i].ui32Count = DES1_RX_CTRL_CHAINED;
            nx_driver_information.nx_driver_information_dma_rx_descriptors[i].ui32Count |= ((packet_ptr -> nx_packet_data_end - packet_ptr -> nx_packet_data_start) <<
                                                                                            DES1_RX_CTRL_BUFF1_SIZE_S); 
            nx_driver_information.nx_driver_information_dma_rx_descriptors[i].pvBuffer1 = packet_ptr->nx_packet_prepend_ptr;
            nx_driver_information.nx_driver_information_dma_rx_descriptors[i].ui32CtrlStatus = DES0_RX_CTRL_OWN;
            nx_driver_information.nx_driver_information_dma_rx_descriptors[i].DES3.pLink =
                ((i == (NX_DRIVER_RX_DESCRIPTORS - 1)) ?
                &nx_driver_information.nx_driver_information_dma_rx_descriptors[0] : &nx_driver_information.nx_driver_information_dma_rx_descriptors[i + 1]);

            nx_driver_information.nx_driver_information_receive_packets[i] =  packet_ptr;
        }
        else
        {
            /* Cannot allocate packets from the packet pool. */
            return(NX_DRIVER_ERROR);
        }
    }

    /* Save the size of one rx buffer.  */
    nx_driver_information.nx_driver_information_rx_buffer_size = packet_ptr -> nx_packet_data_end - packet_ptr -> nx_packet_data_start;
        
    /* Clear the number of buffers in use counter.  */
    nx_driver_information.nx_driver_information_multicast_count = 0;

    //
    // Set the descriptor pointers in the hardware.
    //
    EMACTxDMADescriptorListSet(EMAC0_BASE, &nx_driver_information.nx_driver_information_dma_tx_descriptors[0]);
    EMACRxDMADescriptorListSet(EMAC0_BASE, &nx_driver_information.nx_driver_information_dma_rx_descriptors[0]);


    /* Clear any stray PHY interrupts that may be set. */
    ui16Val = EMACPHYRead(EMAC0_BASE, 0, EPHY_MISR1);
    ui16Val = EMACPHYRead(EMAC0_BASE, 0, EPHY_MISR2);

    /* Configure and enable the link status change interrupt in the PHY. */
    ui16Val = EMACPHYRead(EMAC0_BASE, 0, EPHY_SCR);
    ui16Val |= (EPHY_SCR_INTEN_EXT | EPHY_SCR_INTOE_EXT);
    EMACPHYWrite(EMAC0_BASE, 0, EPHY_SCR, ui16Val);
    EMACPHYWrite(EMAC0_BASE, 0, EPHY_MISR1, (EPHY_MISR1_LINKSTATEN |
                 EPHY_MISR1_SPEEDEN | EPHY_MISR1_DUPLEXMEN | EPHY_MISR1_ANCEN));

    /* Read the PHY interrupt status to clear any stray events. */
    ui16Val = EMACPHYRead(EMAC0_BASE, 0, EPHY_MISR1);

    /**
     * Set MAC filtering options.  We receive all broadcast and mui32ticast
     * packets along with those addressed specifically for us.
     */
    EMACFrameFilterSet(EMAC0_BASE, (EMAC_FRMFILTER_HASH_AND_PERFECT |
                       EMAC_FRMFILTER_PASS_MULTICAST));


    /* Clear any pending MAC interrupts. */
    EMACIntClear(EMAC0_BASE, EMACIntStatus(EMAC0_BASE, false));

    /* Enable the Ethernet MAC transmitter and receiver. */
    EMACTxEnable(EMAC0_BASE);
    EMACRxEnable(EMAC0_BASE);

    /* Enable the Ethernet RX and TX interrupt source. */
    EMACIntEnable(EMAC0_BASE, (EMAC_INT_RECEIVE | EMAC_INT_TRANSMIT |
                  EMAC_INT_TX_STOPPED | EMAC_INT_RX_NO_BUFFER |
                  EMAC_INT_RX_STOPPED | EMAC_INT_PHY));

    /* Enable the Ethernet interrupt. */
//    IntEnable(INT_EMAC0);

    /* Enable all processor interrupts. */
//    IntMasterEnable();

    /* Tell the PHY to start an auto-negotiation cycle. */
    EMACPHYWrite(EMAC0_BASE, 0, EPHY_BMCR, (EPHY_BMCR_ANEN |
                 EPHY_BMCR_RESTARTAN));

    /* Return success!  */
    return(NX_SUCCESS);
} 


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_enable                          PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific link enable requests.     */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver request pointer        */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_enable                     Driver link enable processing */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  03-04-2021        David Vescovi            Initial Version 1.0        */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_enable(NX_IP_DRIVER *driver_req_ptr)
{

    /* Enable the Ethernet interrupt. */
    IntEnable(INT_EMAC0);

    /* Return success!  */
    return(NX_SUCCESS);
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_disable                         PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific link disable requests.    */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver request pointer        */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_disable                    Driver link disable processing*/ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  03-04-2021        David Vescovi            Initial Version 1.0        */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_disable(NX_IP_DRIVER *driver_req_ptr)
{

    /* Disable the Ethernet interrupt. */
    IntDisable(INT_EMAC0);

    /* Return success!  */
    return(NX_SUCCESS);
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_packet_send                     PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific packet send requests.     */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    packet_ptr                            Pointer to packet to send     */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    [_nx_driver_transmit_packet_enqueue]  Optional internal transmit    */ 
/*                                            packet queue routine        */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_packet_send                Driver packet send processing */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-28-2021        David Vescovi            Initial Version 1.0        */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_packet_send(NX_PACKET *packet_ptr)
{

ULONG           curIdx;
#ifndef NX_DISABLE_PACKET_CHAIN
NX_PACKET       *pktIdx;
#endif /* NX_DISABLE_PACKET_CHAIN */
ULONG            bd_count = 0;


    /* Pick up the first BD. */
    curIdx = nx_driver_information.nx_driver_information_transmit_current_index;
    /* Check if it is a free descriptor.  */
    if ((nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus & DES0_TX_CTRL_OWN) || nx_driver_information.nx_driver_information_transmit_packets[curIdx])
    {
        /* Buffer is still owned by device.  */
        return(NX_DRIVER_ERROR);
    }
    
    /* Find the Buffer, set the Buffer pointer.  */
    nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].pvBuffer1 = packet_ptr->nx_packet_prepend_ptr;
    
    /* Set the buffer size.  */
    nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32Count = packet_ptr -> nx_packet_length;
    
    /* Set the first Descriptor's bit.  */
    nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus = (DES0_TX_CTRL_FIRST_SEG | DES0_TX_CTRL_IP_ALL_CKHSUMS);
    

#ifndef NX_DISABLE_PACKET_CHAIN
    nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus |= DES0_TX_CTRL_CHAINED;

    /* Find next packet.  */
    for (pktIdx = packet_ptr -> nx_packet_next;
         pktIdx != NX_NULL;
         pktIdx = pktIdx -> nx_packet_next)
    {
        
        /* Move to next descriptor.  */
        curIdx = (curIdx + 1) & (NX_DRIVER_TX_DESCRIPTORS - 1);
    
        /* Check if it is a free descriptor.  */
        if ((nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus & DES0_TX_CTRL_OWN) || nx_driver_information.nx_driver_information_transmit_packets[curIdx])
        {
            
            /* No more descriptor available, return driver error status.  */
            return(NX_DRIVER_ERROR);
        }
        
        /* Set the buffer pointer.  */
        nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].pvBuffer1 = pktIdx->nx_packet_prepend_ptr;
        
        /* Set the buffer size.  */
        nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32Count = pktIdx->nx_packet_append_ptr - pktIdx->nx_packet_prepend_ptr;
        
        nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus = (DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_IP_ALL_CKHSUMS);

        /* Increment the BD count.  */
        bd_count++;

    }
#endif /* NX_DISABLE_PACKET_CHAIN */
    
    /* Set the last Descriptor's LS & IC & OWN bit.  */
    nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus |= (DES0_TX_CTRL_OWN | DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_INTERRUPT);

    /* Save the pkt pointer to release.  */
    nx_driver_information.nx_driver_information_transmit_packets[curIdx] = packet_ptr;
   
    /* Set the current index to the next descriptor.  */
    nx_driver_information.nx_driver_information_transmit_current_index = (curIdx + 1) & (NX_DRIVER_TX_DESCRIPTORS - 1);

    /* Increment the transmit buffers in use count.  */
    nx_driver_information.nx_driver_information_number_of_transmit_buffers_in_use += bd_count + 1;
    
    /* Set OWN bit to indicate BDs are ready.  */
    for (; bd_count > 0; bd_count--)
    {
        
        /* Set OWN bit in reverse order, move to prevous BD.  */
        curIdx = (curIdx - 1) & (NX_DRIVER_TX_DESCRIPTORS - 1);
        
        /* Set this BD's OWN bit.  */
        nx_driver_information.nx_driver_information_dma_tx_descriptors[curIdx].ui32CtrlStatus |= DES0_TX_CTRL_OWN;
    }

    __DSB();
    
    /* Tell the transmitter to start (in case it had stopped). */
    EMACTxDMAPollDemand(EMAC0_BASE);

    return(NX_SUCCESS);
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_multicast_join                  PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific multicast join requests.  */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver request pointer        */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_multicast_join             Driver multicast join         */ 
/*                                            processing                  */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-28-2021        David Vescovi            Initial Version 1.0        */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_multicast_join(NX_IP_DRIVER *driver_req_ptr)
{


    /* Increase the multicast count.  */
    nx_driver_information.nx_driver_information_multicast_count++;
    
    /* Return success.  */
    return(NX_SUCCESS);
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_multicast_leave                 PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific multicast leave requests. */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver request pointer        */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_multicast_leave            Driver multicast leave        */ 
/*                                            processing                  */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-07-2015        Yuxin Zhou            Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_multicast_leave(NX_IP_DRIVER *driver_req_ptr)
{
    
    /* Decrease the multicast count.  */
    nx_driver_information.nx_driver_information_multicast_count--;
    
    /* If multicast count reachs zero, disable multicast frame reception.  */
    if (nx_driver_information.nx_driver_information_multicast_count == 0)
    {

    }
    
    /* Return success.  */
    return(NX_SUCCESS);
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_get_status                      PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific get status requests.      */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                        Driver request pointer        */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_get_status                 Driver get status processing  */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-07-2015        Yuxin Zhou            Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT  _nx_driver_hardware_get_status(NX_IP_DRIVER *driver_req_ptr)
{

    /* Return success.  */
    return(NX_SUCCESS);
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_packet_transmitted              PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes packets transmitted by the ethernet         */ 
/*    controller.                                                         */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    nx_packet_transmit_release            Release transmitted packet    */ 
/*    [_nx_driver_transmit_packet_dequeue]  Optional transmit packet      */ 
/*                                            dequeue                     */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_deferred_processing        Deferred driver processing    */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  03-11-2021        David Vescovi          Initial Version 5.0          */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_hardware_packet_transmitted(VOID)
{

ULONG numOfBuf =  nx_driver_information.nx_driver_information_number_of_transmit_buffers_in_use;
ULONG idx =       nx_driver_information.nx_driver_information_transmit_release_index;
    
    /* Loop through buffers in use.  */
    while (numOfBuf--)
    {

        /* If no packet, just examine the next packet.  */
        if (nx_driver_information.nx_driver_information_transmit_packets[idx] == NX_NULL) 
        {

            /* BD's not in use.  */
            nx_driver_information.nx_driver_information_dma_tx_descriptors[idx].ui32CtrlStatus = 0;

            /* No packet in use, skip to next.  */
            idx = (idx + 1) & (NX_DRIVER_TX_DESCRIPTORS - 1);
            continue;
        }

        /* Determine if the packet has been transmitted.  */
        if (!(nx_driver_information.nx_driver_information_dma_tx_descriptors[idx].ui32CtrlStatus & DES0_TX_CTRL_OWN))
        {

            /* Yes, packet has been transmitted.  */
            
            /* Remove the Ethernet header and release the packet.  */
            NX_DRIVER_ETHERNET_HEADER_REMOVE(nx_driver_information.nx_driver_information_transmit_packets[idx]);

            /* Release the packet.  */
            nx_packet_transmit_release(nx_driver_information.nx_driver_information_transmit_packets[idx]);

            /* Clear the entry in the in-use array.  */
            nx_driver_information.nx_driver_information_transmit_packets[idx] = NX_NULL;

            /* Update the transmit relesae index and number of buffers in use.  */
            idx = (idx + 1) & (NX_DRIVER_TX_DESCRIPTORS - 1);
            nx_driver_information.nx_driver_information_number_of_transmit_buffers_in_use = numOfBuf;
            nx_driver_information.nx_driver_information_transmit_release_index = idx;
        }
        else
        {

            /* Get out of the loop!  */
            break;
        }
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_packet_received                 PORTABLE C      */ 
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes packets received by the ethernet            */ 
/*    controller.                                                         */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_driver_transfer_to_netx           Transfer packet to NetX       */ 
/*    nx_packet_allocate                    Allocate receive packets      */ 
/*    nx_packet_release                     Release receive packets       */
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_deferred_processing        Deferred driver processing    */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-07-2015        Yuxin Zhou            Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static VOID  _nx_driver_hardware_packet_received(VOID)
{
NX_PACKET     *packet_ptr;
ULONG          bd_count = 0;
INT            i;
ULONG          idx;
ULONG          temp_idx;
ULONG          first_idx = nx_driver_information.nx_driver_information_receive_current_index;
NX_PACKET     *received_packet_ptr = nx_driver_information.nx_driver_information_receive_packets[first_idx];


    /* Find out the BDs that owned by CPU.  */
    for (first_idx = idx = nx_driver_information.nx_driver_information_receive_current_index;
         !(nx_driver_information.nx_driver_information_dma_rx_descriptors[idx].ui32CtrlStatus & DES0_RX_CTRL_OWN);
         idx = (idx + 1) & (NX_DRIVER_RX_DESCRIPTORS - 1))
    {
        
        /* Is the BD marked as the end of a frame?  */
        if (nx_driver_information.nx_driver_information_dma_rx_descriptors[idx].ui32CtrlStatus & DES0_RX_STAT_LAST_DESC)
        {
            /* check for error */
            if (!(nx_driver_information.nx_driver_information_dma_rx_descriptors[idx].ui32CtrlStatus & DES0_RX_STAT_ERR))
            {

                /* Yes, this BD is the last BD in the frame, set the last NX_PACKET's nx_packet_next to NULL.  */
                nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_next = NX_NULL;
                
                /* Store the length of the packet in the first NX_PACKET.  */
                nx_driver_information.nx_driver_information_receive_packets[first_idx] -> nx_packet_length = ((nx_driver_information.nx_driver_information_dma_rx_descriptors[idx].ui32CtrlStatus & DES0_RX_STAT_FRAME_LENGTH_M) >> DES0_RX_STAT_FRAME_LENGTH_S) - 4;

                /* Adjust nx_packet_append_ptr with the size of the data in this buffer.  */
                nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_append_ptr = nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_prepend_ptr
                                                                                                        + nx_driver_information.nx_driver_information_receive_packets[first_idx] -> nx_packet_length
                                                                                                        - bd_count * nx_driver_information.nx_driver_information_rx_buffer_size;
  
                /* Is there only one BD for the current frame?  */
                if (idx != first_idx)
                {
                    
                    /* No, this BD is not the first BD of the frame, frame data starts at the aligned address.  */
//                    nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_prepend_ptr -= 2;
                }
            }
            else
            {
                nx_driver_information.ErrorDS++;
            }

            /* Allocate new NX_PACKETs for BDs.  */
            for (i = bd_count; i >= 0; i--)
            {

                temp_idx = (first_idx + i) & (NX_DRIVER_RX_DESCRIPTORS - 1);

                /* Allocate a new packet from the packet pool.  */
                if (nx_packet_allocate(nx_driver_information.nx_driver_information_packet_pool_ptr, &packet_ptr, 
                                            NX_RECEIVE_PACKET, NX_NO_WAIT) == NX_SUCCESS)
                {
                    /* Adjust the new packet for alignment and assign it to the BD.  */
                    packet_ptr -> nx_packet_prepend_ptr += 2;
/*                    packet_ptr -> nx_packet_prepend_ptr = packet_ptr -> nx_packet_prepend_ptr + 2; */
                    nx_driver_information.nx_driver_information_dma_rx_descriptors[temp_idx].pvBuffer1 = packet_ptr->nx_packet_prepend_ptr;
                    nx_driver_information.nx_driver_information_dma_rx_descriptors[temp_idx].ui32CtrlStatus = DES0_RX_CTRL_OWN;
                    nx_driver_information.nx_driver_information_receive_packets[temp_idx] = packet_ptr;
                }
                else
                {
                    
                    /* Allocation failed, get out of the loop.  */
                    break;
                }
            }
            
            if (i >= 0)
            {
                
                /* At least one packet allocation was failed, release the received packet.  */
                nx_packet_release(nx_driver_information.nx_driver_information_receive_packets[temp_idx] -> nx_packet_next);
                
                for (; i >= 0; i--)
                {
                    
                    /* Free up the BD to ready state. */
                    temp_idx = (first_idx + i) & (NX_DRIVER_RX_DESCRIPTORS - 1);
                    nx_driver_information.nx_driver_information_dma_rx_descriptors[temp_idx].ui32CtrlStatus = DES0_RX_CTRL_OWN;
                    nx_driver_information.nx_driver_information_receive_packets[temp_idx] -> nx_packet_prepend_ptr = nx_driver_information.nx_driver_information_receive_packets[temp_idx] -> nx_packet_data_start + 2;
                }
            }
            else
            {
                
                /* Everything is OK, transfer the packet to NetX.  */
                _nx_driver_transfer_to_netx(nx_driver_information.nx_driver_information_ip_ptr, received_packet_ptr);
            }
            
            /* Set the first BD index for the next packet.  */
            first_idx = (idx + 1) & (NX_DRIVER_RX_DESCRIPTORS - 1);
            
            /* Update the current receive index.  */
            nx_driver_information.nx_driver_information_receive_current_index = first_idx;
            
            received_packet_ptr = nx_driver_information.nx_driver_information_receive_packets[first_idx];
            
            bd_count = 0;

        }
        else
        {
            
            /* This BD is not the last BD of a frame. It is a intermediate descriptor.  */

            nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_next = nx_driver_information.nx_driver_information_receive_packets[(idx + 1) & (NX_DRIVER_RX_DESCRIPTORS - 1)];
            
            nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_append_ptr = nx_driver_information.nx_driver_information_receive_packets[idx] -> nx_packet_data_end;
            
            bd_count++;
        }
    }
}


#ifdef NX_ENABLE_INTERFACE_CAPABILITY
/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _nx_driver_hardware_capability_set                  SAME54/IAR      */  
/*                                                           5.0          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processes hardware-specific capability set requests.  */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    driver_req_ptr                         Driver request pointer       */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    status                                [NX_SUCCESS|NX_DRIVER_ERROR]  */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _nx_driver_capability_set             Capability set processing     */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-07-2015        Yuxin Zhou            Initial Version 5.0           */ 
/*                                                                        */ 
/**************************************************************************/ 
static UINT _nx_driver_hardware_capability_set(NX_IP_DRIVER *driver_req_ptr)
{
    
    return NX_SUCCESS;
}
#endif /* NX_ENABLE_INTERFACE_CAPABILITY */



void
msp432e4if_process_phy_interrupt(void)
{
    uint16_t ui16Val, ui16Status;
    uint32_t ui32Config, ui32Mode, ui32RxMaxFrameSize;

    /* Read the PHY interrupt status.  This clears all interrupt sources.
     * Note that we are only enabling sources in EPHY_MISR1 so we don't
     * read EPHY_MISR2.
     */
    ui16Val = EMACPHYRead(EMAC0_BASE, 0, EPHY_MISR1);

    /* Read the current PHY status. */
    ui16Status = EMACPHYRead(EMAC0_BASE, 0, EPHY_STS);

    /* If EEE mode support is requested then read the value of the Link
     * partners status
     */

	/* Has the link status changed? */
    if(ui16Val & EPHY_MISR1_LINKSTAT)
    {
        /* Is link up or down now? */
        if(ui16Status & EPHY_STS_LINK)
        {
            /* Tell stack the link is up. */

            /* if the link has been advertised as EEE capable then configure
             * the MAC register for LPI timers and manually set the PHY link
             * status bit
             */

            /* In this case we drop through since we may need to reconfigure
             * the MAC depending upon the speed and half/fui32l-duplex settings.
             */
        }
        else
        {
            /* Tell stack the link is down */

            /* if the link has been advertised as EEE capable then clear the
             * MAC register LPI timers and manually clear the PHY link status
             * bit
             */
        }
    }

    /* Has the speed or duplex status changed? */
    if(ui16Val & (EPHY_MISR1_SPEED | EPHY_MISR1_SPEED | EPHY_MISR1_ANC))
    {
        /* Get the current MAC configuration. */
        EMACConfigGet(EMAC0_BASE, &ui32Config, &ui32Mode,
                        &ui32RxMaxFrameSize);

        /* What speed is the interface running at now?
         */
        if(ui16Status & EPHY_STS_SPEED)
        {
            /* 10Mbps is selected */
            ui32Config &= ~EMAC_CONFIG_100MBPS;
        }
        else
        {
            /* 100Mbps is selected */
            ui32Config |= EMAC_CONFIG_100MBPS;
        }

        /* Are we in fui32l- or half-duplex mode? */
        if(ui16Status & EPHY_STS_DUPLEX)
        {
            /* Fui32l duplex. */
            ui32Config |= EMAC_CONFIG_FULL_DUPLEX;
        }
        else
        {
            /* Half duplex. */
            ui32Config &= ~EMAC_CONFIG_FULL_DUPLEX;
        }

        /* Reconfigure the MAC */
        EMACConfigSet(EMAC0_BASE, ui32Config, ui32Mode, ui32RxMaxFrameSize);
    }
}


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    nx_driver_msp432e_ethernet_isr                       PORTABLE C     */ 
/*                                                                        */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    Yuxin Zhou, Microsoft Corporation                                   */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function processing incoming packets.  This routine is         */
/*    be called from the receive packet ISR and assumes that the          */ 
/*    interrupt is saved/restored around the call by ThreadX.             */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _nx_ip_driver_deferred_processing     IP receive packet processing  */ 
/*                                                                        */
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    ISR                                                                 */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  02-28-2021        David Vescovi            Initial Version 1.0        */ 
/*                                                                        */ 
/**************************************************************************/ 
VOID  nx_driver_msp432e_ethernet_isr(VOID)
{
    uint32_t ui32Status;

    //
    // Read and Clear the interrupt.
    //
    ui32Status = EMACIntStatus(EMAC0_BASE, true);

    //
    // If the PMT mode exit status bit is set then enable the MAC transmit
    // and receive paths, read the PMT status to clear the interrupt and
    // clear the interrupt flag.
    //
    if(ui32Status & EMAC_INT_POWER_MGMNT)
    {
        MAP_EMACTxEnable(EMAC0_BASE);
        MAP_EMACRxEnable(EMAC0_BASE);

        EMACPowerManagementStatusGet(EMAC0_BASE);

        ui32Status &= ~(EMAC_INT_POWER_MGMNT);
    }

    /* Update our debug interrupt counters. */
    if(ui32Status & EMAC_INT_NORMAL_INT)
    {
        nx_driver_information.NormalInts++;
    }

    if(ui32Status & EMAC_INT_ABNORMAL_INT)
    {
        nx_driver_information.AbnormalInts++;
    }

    /* Is this an interrupt from the PHY? */
    if(ui32Status & EMAC_INT_PHY)
    {
        msp432e4if_process_phy_interrupt();
    }

    /* Receive packet interrupt.  */
    if(ui32Status & (EMAC_INT_RECEIVE | EMAC_INT_RX_NO_BUFFER |
       EMAC_INT_RX_STOPPED))
    {
#ifdef NX_DRIVER_ENABLE_DEFERRED
        /* Set the receive packet interrupt.  */
        nx_driver_information.nx_driver_information_deferred_events |= NX_DRIVER_DEFERRED_PACKET_RECEIVED;
        _nx_ip_driver_deferred_processing(nx_driver_information.nx_driver_information_ip_ptr);
#else
        /* Process received packet(s).  */
        _nx_driver_hardware_packet_received();
#endif
    }

    /* Interrupt on transmit completion.  */
    if(ui32Status & EMAC_INT_TRANSMIT)
    {
#ifdef NX_DRIVER_ENABLE_DEFERRED
        /* Set the transmit complete bit.  */
        nx_driver_information.nx_driver_information_deferred_events |= NX_DRIVER_DEFERRED_PACKET_TRANSMITTED;
        _nx_ip_driver_deferred_processing(nx_driver_information.nx_driver_information_ip_ptr);
#else
        /* Process transmitted packet(s).  */
        _nx_driver_hardware_packet_transmitted();
#endif
    }
  
    //
    // If the interrupt really came from the Ethernet
    // clear it.
    //
    if(ui32Status)
    {
        MAP_EMACIntClear(EMAC0_BASE, ui32Status);
    }
}

/****** DRIVER SPECIFIC ****** Start of part/vendor specific internal driver functions.  */
