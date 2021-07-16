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

#ifndef SL_WFX_HOST_CFG_H
#define SL_WFX_HOST_CFG_H

#ifdef EFR32MG12P432F1024GL125
/* Configure reset pin*/
#define SL_WFX_HOST_CFG_RESET_PORT          gpioPortD
#define SL_WFX_HOST_CFG_RESET_PIN           10
/* Configure wake up pin*/
#define SL_WFX_HOST_CFG_WUP_PORT            gpioPortD
#define SL_WFX_HOST_CFG_WUP_PIN             8

#ifdef  SL_WFX_USE_SPI
#define SL_WFX_HOST_CFG_SPI_USART_PORT      gpioPortA
#define SL_WFX_HOST_CFG_SPI_USART_CS_PIN    9
#define SL_WFX_HOST_CFG_SPI_USART_CS_LOC    1

#define SL_WFX_HOST_CFG_SPI_USART_TX_PIN    6
#define SL_WFX_HOST_CFG_SPI_USART_TX_LOC    1

#define SL_WFX_HOST_CFG_SPI_USART_RX_PIN    7
#define SL_WFX_HOST_CFG_SPI_USART_RX_LOC    1

#define SL_WFX_HOST_CFG_SPI_USART_CLK_PIN   8
#define SL_WFX_HOST_CFG_SPI_USART_CLK_LOC   1

#define SL_WFX_HOST_CFG_SPI_USART           USART2

#define SL_WFX_HOST_CFG_SPI_WIRQPORT        gpioPortB                       /* SPI IRQ port*/
#define SL_WFX_HOST_CFG_SPI_WIRQPIN         6                               /* SPI IRQ pin */
#define SL_WFX_HOST_CFG_SPI_IRQ             SL_WFX_HOST_CFG_SPI_WIRQPIN
#else
#error "SPI only interface available and SL_WFX_USE_SPI not defined"
#endif    /* SL_WFX_USE_SPI */
#endif    /* EFR32MG12P432F1024GL125 */

#endif    /* SL_WFX_HOST_CFG_H */
