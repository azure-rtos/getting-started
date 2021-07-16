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
#include "em_cmu.h"
#include "em_gpio.h"
#include "sl_wfx_host_cfg.h"
#include "tx_api.h"
#include "sl_wfx_task.h"

static void sl_wfx_gpio_unified_irq(void);

/**************************************************************************//**
 * Host GPIO setup 
 *****************************************************************************/
void sl_wfx_host_gpio_setup(void)
{
  /* Enable GPIO clock */
  CMU_ClockEnable(cmuClock_GPIO, true);

  /* Configure WF200 reset pin */
  GPIO_PinModeSet(SL_WFX_HOST_CFG_RESET_PORT, SL_WFX_HOST_CFG_RESET_PIN,
                  gpioModePushPull,
                  0);

  /* Configure WF200 WUP pin */
  GPIO_PinModeSet(SL_WFX_HOST_CFG_WUP_PORT, SL_WFX_HOST_CFG_WUP_PIN,
                  gpioModePushPull,
                  0);

  /* GPIO used as IRQ */
  GPIO_PinModeSet(SL_WFX_HOST_CFG_SPI_WIRQPORT, SL_WFX_HOST_CFG_SPI_WIRQPIN,
                  gpioModeInputPull,
                  0);

  /* Enable low-frequency crystal oscillator clock */
  CMU_OscillatorEnable(cmuOsc_LFXO, true, true);

  /* Reset and enable associated CPU interrupt vector */
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
  NVIC_ClearPendingIRQ(GPIO_EVEN_IRQn);
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);
}

/**************************************************************************//**
 * GPIO unified handler
 *****************************************************************************/
static void sl_wfx_gpio_unified_irq(void)
{
  /*  Get and clear all pending GPIO interrupts */
  uint32_t interrupt_mask = GPIO_IntGet();

  GPIO_IntClear(interrupt_mask);

  /* Act on interrupts */
  if (interrupt_mask & (1 << SL_WFX_HOST_CFG_SPI_WIRQPIN)) {
    /* Notify wfx process thread */
    sl_wfx_process_notify(SL_WFX_RX_PACKET_AVAILABLE);
  }
}

/**************************************************************************//**
 * GPIO even interrupt handler.
 *****************************************************************************/
void GPIO_EVEN_IRQHandler(void)
{
  sl_wfx_gpio_unified_irq();
}

/**************************************************************************//**
 * GPIO odd interrupt handler.
 *****************************************************************************/
void GPIO_ODD_IRQHandler(void)
{
  sl_wfx_gpio_unified_irq();
}
