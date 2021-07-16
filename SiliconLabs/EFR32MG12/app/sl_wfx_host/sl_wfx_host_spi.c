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

#ifdef  SL_WFX_USE_SPI

#include "sl_wfx.h"
#include "sl_wfx_host_api.h"
#include "sl_wfx_host_cfg.h"
#include "sl_wfx_host_gpio.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "em_cmu.h"
#include "em_ldma.h"
#include "em_bus.h"
#include "spidrv.h"
#include "tx_api.h"

#define SL_WFX_USART           SL_WFX_HOST_CFG_SPI_USART
#define SL_WFX_USART_PORT      SL_WFX_HOST_CFG_SPI_USART_PORT
#define SL_WFX_USART_CS_PIN    SL_WFX_HOST_CFG_SPI_USART_CS_PIN
#define SL_WFX_USART_TX_PIN    SL_WFX_HOST_CFG_SPI_USART_TX_PIN
#define SL_WFX_USART_RX_PIN    SL_WFX_HOST_CFG_SPI_USART_RX_PIN
#define SL_WFX_USART_CLK_PIN   SL_WFX_HOST_CFG_SPI_USART_CLK_PIN
#define SL_WFX_USART_CS_LOC    SL_WFX_HOST_CFG_SPI_USART_CS_LOC
#define SL_WFX_USART_TX_LOC    SL_WFX_HOST_CFG_SPI_USART_TX_LOC
#define SL_WFX_USART_RX_LOC    SL_WFX_HOST_CFG_SPI_USART_RX_LOC
#define SL_WFX_USART_CLK_LOC   SL_WFX_HOST_CFG_SPI_USART_CLK_LOC

static TX_SEMAPHORE            sl_wfx_spi_dma_semaphore;
static SPIDRV_HandleData_t     sl_wfx_spi_handle_data;
static SPIDRV_Handle_t         sl_wfx_spi_handle      = &sl_wfx_spi_handle_data;
static SPIDRV_Init_t           sl_wfx_init_data       = {
  SL_WFX_USART,                     /* USART port                       */
  SL_WFX_USART_TX_LOC,              /* USART Tx port location number    */
  SL_WFX_USART_RX_LOC,              /* USART Rx port location number    */
  SL_WFX_USART_CLK_LOC,             /* USART Clk port location number   */
  SL_WFX_USART_CS_LOC,              /* USART Cs port location number    */
  36000000,                         /* Bitrate                          */
  8,                                /* Frame length                     */
  0,                                /* Dummy tx value for rx only funcs */
  spidrvMaster,                     /* SPI mode                         */
  spidrvBitOrderMsbFirst,           /* Bit order on bus                 */
  spidrvClockMode0,                 /* SPI clock/phase mode             */
  spidrvCsControlApplication,       /* CS controlled by the driver      */
  spidrvSlaveStartImmediate         /* Slave start transfers immediately*/
};

static void sl_wfx_spi_dma_complete_callback(struct SPIDRV_HandleData *handle,
                                             Ecode_t transferStatus,
                                             int itemsTransferred);

/**************************************************************************//**
 * Initialize SPI peripheral
 *****************************************************************************/
sl_status_t sl_wfx_host_init_bus(void)
{
  /* Init host gpio for control wf200 */
  sl_wfx_host_gpio_setup();

  /* Init spi dma semaphore */
  tx_semaphore_create(&sl_wfx_spi_dma_semaphore, "SPI DMA complete", 0);

  /* Tx, Rx, Clk, Cs pin configuraion */
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(SL_WFX_USART_PORT, SL_WFX_USART_TX_PIN, gpioModePushPull, 0);
  GPIO_PinModeSet(SL_WFX_USART_PORT, SL_WFX_USART_RX_PIN, gpioModeInput, 0);
  GPIO_PinModeSet(SL_WFX_USART_PORT, SL_WFX_USART_CLK_PIN, gpioModePushPull, 0);
  GPIO_PinModeSet(SL_WFX_USART_PORT, SL_WFX_USART_CS_PIN, gpioModePushPull, 1);

  /* SPI driver Init */
  SPIDRV_Init(sl_wfx_spi_handle, &sl_wfx_init_data);

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * De-initialize SPI peripheral and DMAs
 *****************************************************************************/
sl_status_t sl_wfx_host_deinit_bus(void)
{
  SPIDRV_DeInit(sl_wfx_spi_handle);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Assert chip select.
 *****************************************************************************/
sl_status_t sl_wfx_host_spi_cs_assert()
{
  GPIO_PinOutClear(SL_WFX_USART_PORT, SL_WFX_USART_CS_PIN);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * De-assert chip select.
 *****************************************************************************/
sl_status_t sl_wfx_host_spi_cs_deassert()
{
  GPIO_PinOutSet(SL_WFX_USART_PORT, SL_WFX_USART_CS_PIN);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * WFX SPI transfer implementation
 *****************************************************************************/
sl_status_t sl_wfx_host_spi_transfer_no_cs_assert(sl_wfx_host_bus_transfer_type_t type,
                                                  uint8_t *header,
                                                  uint16_t header_length,
                                                  uint8_t *buffer,
                                                  uint16_t buffer_length)
{
  const bool is_read = (type == SL_WFX_BUS_READ);

  /* Wait for tx buffer transmit completely */
  while (!(SL_WFX_USART->STATUS & USART_STATUS_TXBL)) {
  }

  /* Clear buffer Rx and Tx */
  SL_WFX_USART->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX;

  if (header_length > 0) {
    for (uint8_t *buffer_ptr = header; header_length > 0; --header_length, ++buffer_ptr) {
      sl_wfx_init_data.port->TXDATA = (uint32_t)(*buffer_ptr);

      while (!(sl_wfx_init_data.port->STATUS & USART_STATUS_TXC)) {
      }
    }
    while (!(sl_wfx_init_data.port->STATUS & USART_STATUS_TXBL)) {
    }
  }

  if (buffer_length > 0) {
    if (is_read) {
      SPIDRV_MReceive(sl_wfx_spi_handle,
                      buffer,
                      buffer_length,
                      sl_wfx_spi_dma_complete_callback);
    } else {
      SPIDRV_MTransmit(sl_wfx_spi_handle,
                       buffer,
                       buffer_length,
                       sl_wfx_spi_dma_complete_callback);
    }
    tx_semaphore_get(&sl_wfx_spi_dma_semaphore, TX_WAIT_FOREVER);
  }

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Enable WFX interrupt
 *****************************************************************************/
sl_status_t sl_wfx_host_enable_platform_interrupt(void)
{
  GPIO_ExtIntConfig(SL_WFX_HOST_CFG_SPI_WIRQPORT,
                    SL_WFX_HOST_CFG_SPI_WIRQPIN,
                    SL_WFX_HOST_CFG_SPI_IRQ,
                    true,
                    false,
                    true);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Disable WFX interrupt
 *****************************************************************************/
sl_status_t sl_wfx_host_disable_platform_interrupt(void)
{
  GPIO_IntDisable(1 << SL_WFX_HOST_CFG_SPI_IRQ);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * SPI DMA complete callback
 *****************************************************************************/
static void sl_wfx_spi_dma_complete_callback(struct SPIDRV_HandleData *handle,
                                             Ecode_t transferStatus,
                                             int itemsTransferred)
{
  (void)handle;
  (void)transferStatus;
  (void)itemsTransferred;
  tx_semaphore_put(&sl_wfx_spi_dma_semaphore);
}

#endif
