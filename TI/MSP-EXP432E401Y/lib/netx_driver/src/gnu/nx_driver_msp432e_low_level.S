@/**************************************************************************/
@/*                                                                        */
@/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
@/*                                                                        */
@/*       This software is licensed under the Microsoft Software License   */
@/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
@/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
@/*       and in the root directory of this software.                      */
@/*                                                                        */
@/**************************************************************************/
@
@
@/**************************************************************************/
@/**************************************************************************/
@/**                                                                       */ 
@/** NetX Component                                                        */ 
@/**                                                                       */
@/**   Ethernet device driver for TI MSP432E family micro processors       */
@/**                                                                       */
@/**************************************************************************/
@/**************************************************************************/
@
@#define TX_SOURCE_CODE
@
@

    .global  nx_driver_msp432e_ethernet_isr

    .text
    .align 4
    .syntax unified
@/**************************************************************************/ 
@/*                                                                        */ 
@/*  FUNCTION                                               RELEASE        */ 
@/*                                                                        */ 
@/*    __nx_driver_msp432e_ethernet_isr                    Initial         */ 
@/*                                                          6.0           */
@/*  AUTHOR                                                                */
@/*                                                                        */
@/*   David Vescovi                                                        */
@/*                                                                        */
@/*  DESCRIPTION                                                           */ 
@/*                                                                        */ 
@/*    This function is responsible for fielding the Etherent interrupts   */ 
@/*    of the TI MSP432E.                                                  */ 
@/*                                                                        */ 
@/*  INPUT                                                                 */ 
@/*                                                                        */ 
@/*    None                                                                */ 
@/*                                                                        */ 
@/*  OUTPUT                                                                */ 
@/*                                                                        */ 
@/*    None                                                                */ 
@/*                                                                        */ 
@/*  CALLS                                                                 */ 
@/*                                                                        */ 
@/*    nx_driver_msp432e_ethernet_isr        NetX driver ethernet ISR      */ 
@/*                                                                        */ 
@/*  CALLED BY                                                             */ 
@/*                                                                        */ 
@/*    Interrupt                             Ethernet peripheral interrupt */ 
@/*                                                                        */ 
@/*  RELEASE HISTORY                                                       */ 
@/*                                                                        */ 
@/*    DATE              NAME                      DESCRIPTION             */
@/*                                                                        */
@/*  02-28-2020     David Vescovi               Initial Version 6.0        */
@/*                                                                        */
@/**************************************************************************/
    .global  EMAC0_IRQHandler
    .global  __nx_driver_msp432e_ethernet_isr
    .thumb_func
EMAC0_IRQHandler: 
    .thumb_func
__nx_driver_msp432e_ethernet_isr:
    PUSH    {r0, lr}
#ifdef TX_ENABLE_EXECUTION_CHANGE_NOTIFY
    BL      _tx_execution_isr_enter             @ Call the ISR enter function
#endif

    BL      nx_driver_msp432e_ethernet_isr

#ifdef TX_ENABLE_EXECUTION_CHANGE_NOTIFY
    BL      _tx_execution_isr_exit              @ Call the ISR exit function
#endif
    POP     {r0, lr}
    BX      LR
