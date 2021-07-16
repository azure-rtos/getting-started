/***************************************************************************//**
 * @file
 * @brief IO Stream SWO ITM Functions
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include <stdbool.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "os.h"
#endif

#include "sli_iostream_swo_itm_8.h"
#include "sl_iostream.h"
#include "sl_enum.h"
#include "sl_status.h"
#include "em_assert.h"
#include "em_gpio.h"
#include "em_cmu.h"

#include "sl_atomic.h"

#define LOW_BYTE(n)                     ((uint8_t)((n) & 0xFF))
#define HIGH_BYTE(n)                    ((uint8_t)(LOW_BYTE((n) >> 8)))

/*******************************************************************************
 *************************   LOCAL DATA TYPES   ********************************
 ******************************************************************************/

#if defined(SL_CATALOG_KERNEL_PRESENT)
typedef struct {
  OS_MUTEX lock;
} swo_itm_8_context_t;
#endif

/*******************************************************************************
 ******************************   VARIABLES   **********************************
 ******************************************************************************/

static uint32_t swo_itm_initialized = 0;

#if defined(SL_CATALOG_KERNEL_PRESENT)
static swo_itm_8_context_t context;
#endif

/*******************************************************************************
 *********************   LOCAL FUNCTION PROTOTYPES   ***************************
 ******************************************************************************/

static void swo_itm_send_byte(uint8_t byte);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize swo stimulus 8
 ******************************************************************************/
void *sli_iostream_swo_itm_8_init(void)
{
  bool init;
  uint32_t tpiu_prescaler_val;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  void *return_context = &context;
#else
  void *return_context = NULL;
#endif

  sl_atomic_load(init, swo_itm_initialized);
  if (init) {
    return return_context;
  }

  sl_atomic_store(swo_itm_initialized, true);

#if defined(_CMU_HFBUSCLKEN0_GPIO_MASK)
  CMU->HFBUSCLKEN0 |= CMU_HFBUSCLKEN0_GPIO;
#endif
#if defined(_CMU_HFPERCLKEN0_GPIO_MASK)
  CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_GPIO;
#endif

  // Enable Serial wire output pin
#if defined (_GPIO_ROUTE_MASK)
  GPIO->ROUTE |= GPIO_ROUTE_SWOPEN;
#endif
#if defined (_GPIO_ROUTEPEN_MASK)
  GPIO->ROUTEPEN |= GPIO_ROUTEPEN_SWVPEN;
#endif

#if defined(_EFM32_GECKO_FAMILY) || defined(_EFM32_TINY_FAMILY)
  // Set location 1
  GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC1;
  // Enable output on pin
  GPIO->P[2].MODEH &= ~(_GPIO_P_MODEH_MODE15_MASK);
  GPIO->P[2].MODEH |= GPIO_P_MODEH_MODE15_PUSHPULL;
  // Set TPIU prescaler to 16 (14 MHz / 16 = 875 kHz SWO speed)
  tpiu_prescaler_val = 15;
#elif defined(_SILICON_LABS_32B_SERIES_0) \
  && (defined(_EFM32_GIANT_FAMILY) || defined(_EFM32_WONDER_FAMILY))
  // Set location 0
  GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC0;
  // Enable output on pin
  GPIO->P[5].MODEL &= ~(_GPIO_P_MODEL_MODE2_MASK);
  GPIO->P[5].MODEL |= GPIO_P_MODEL_MODE2_PUSHPULL;
  // Set TPIU prescaler to 16 (14 MHz / 16 = 875 kHz SWO speed)
  tpiu_prescaler_val = 16 - 1;
#elif defined(_SILICON_LABS_32B_SERIES_1)
  // Set location 0
  GPIO->ROUTELOC0 = (GPIO->ROUTELOC0 & ~(_GPIO_ROUTELOC0_SWVLOC_MASK)) | GPIO_ROUTELOC0_SWVLOC_LOC0;
  // Enable output on pin
  GPIO->P[5].MODEL &= ~(_GPIO_P_MODEL_MODE2_MASK);
  GPIO->P[5].MODEL |= 4 << 8;
#if defined(EMDK1001_FPGA)
  // Set TPIU prescaler to get a 875 kHz swo speed
  tpiu_prescaler_val = SystemCoreClockGet() / 875000 - 1;
#else
  // Set TPIU prescaler to 22 (19 MHz / 22 = 863.63 kHz SWO speed)
  tpiu_prescaler_val = 22 - 1;
#endif

#elif (_SILICON_LABS_32B_SERIES == 2)

  // Enable output on pin
#if defined (_CMU_CLKEN0_MASK)
  CMU->CLKEN0 |= CMU_CLKEN0_GPIO;
#endif
  GPIO->P[GPIO_SWV_PORT].MODEL &= ~(_GPIO_P_MODEL_MODE0_MASK << (GPIO_SWV_PIN * 4));
  GPIO->P[GPIO_SWV_PORT].MODEL |= _GPIO_P_MODEL_MODE0_PUSHPULL << (GPIO_SWV_PIN * 4);
  GPIO->TRACEROUTEPEN |= GPIO_TRACEROUTEPEN_SWVPEN;
#if defined(HFRCOEM23_PRESENT)
  // Select HFRCOEM23 as source for TRACECLK
  CMU_ClockSelectSet(cmuClock_TRACECLK, cmuSelect_HFRCOEM23);
#endif
  // Set TPIU prescaler to get a 863.63 kHz SWO speed
  tpiu_prescaler_val = CMU_ClockFreqGet(cmuClock_TRACECLK) / 863630 - 1;

#else
  #error Unknown device family!
#endif

#if (_SILICON_LABS_32B_SERIES != 2)
  // Enable debug clock AUXHFRCO
  CMU->OSCENCMD = CMU_OSCENCMD_AUXHFRCOEN;

  while (!(CMU->STATUS & CMU_STATUS_AUXHFRCORDY)) ;
#endif

  // Enable trace in core debug
  CoreDebug->DHCSR |= 1;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  // Enable Cycle Counter
  DWT->CTRL = 0x3FF;

  // Set TPIU Prescaler
  TPI->ACPR = tpiu_prescaler_val;

  // Set protocol to NRZ
  TPI->SPPR = 2;

  // Disable continuous formatting
  TPI->FFCR = 0x100;

  // Unlock ITM and output data
  ITM->LAR = 0xC5ACCE55;
  ITM->TCR = 0x10009;

  // ITM Channel 8 is used
  ITM->TER |= (1UL << 8);

#if defined(SL_CATALOG_KERNEL_PRESENT)
  RTOS_ERR err;

// Create OS Mutex for lock
  OSMutexCreate(&context.lock, "vuart lock", &err);
  EFM_ASSERT(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
#endif

  return return_context;
}

/***************************************************************************//**
 * write data on vuart interface (WSTK-port 4900)
 ******************************************************************************/
sl_status_t sli_iostream_swo_itm_8_write(void *context,
                                         const void *buffer,
                                         size_t buffer_length,
                                         sl_iostream_swo_itm_8_msg_type_t type,
                                         uint8_t *seq_nbr)
{
  uint8_t *buf = (uint8_t *)buffer;
  uint32_t packet_length;
  uint32_t i;
  uint8_t  output_byte;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  swo_itm_8_context_t *vuart_context = (swo_itm_8_context_t *)context;
  RTOS_ERR err;
  if (OSRunning) {
    // Bypass lock if we print before the kernel is running
    OSMutexPend(&vuart_context->lock, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if (RTOS_ERR_CODE_GET(err) != RTOS_ERR_NONE) {
      return SL_STATUS_FAIL;
    }
  }
#else
  (void)context;
#endif

  // Full length is 2 square braces, 1 byte length and 2 byte crc
  packet_length = ( (uint32_t)buffer_length) + 9;

  for ( i = 0; i < packet_length; ++i ) {
    if ( i == 0 ) {
      // Frame start
      output_byte = '[';
    } else if ( i == 1 ) {
      // Including special byte, type and sequence number
      output_byte = buffer_length + 4;
    } else if ( i == 2 ) {
      // Special EDM byte
      output_byte = 0xD1;
    } else if ( i == 3 ) {
      // VUART TX Type byte 1
      output_byte = LOW_BYTE((uint16_t)type);
    } else if ( i == 4 ) {
      // VUART TX Type byte 2
      output_byte = HIGH_BYTE((uint16_t)type);
    } else if ( i == 5 ) {
      // Sequence number
      output_byte = *seq_nbr++;
    } else if ( i == packet_length - 3 ) {
      // CRC first byte
      // Ignored by FW - so we also skip it
      output_byte = 0x5A;
    } else if ( i == packet_length - 2 ) {
      // CRC second byte
      // Ignored by FW - so we also skip it
      output_byte = 0x5A;
    } else if ( i == packet_length - 1 ) {
      // Frame end
      output_byte = ']';
    } else {
      // Data
      output_byte = buf[i - 6];
    }

    swo_itm_send_byte(output_byte);
  }

    #if defined(SL_CATALOG_KERNEL_PRESENT)
  if (OSRunning) {
    // Bypass lock if we print before the kernel is running
    OSMutexPost(&vuart_context->lock, OS_OPT_POST_NONE, &err);
    EFM_ASSERT(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
  }
    #endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * send 1 bytes on swo itm stimulus 8
 ******************************************************************************/
static void swo_itm_send_byte(uint8_t byte)
{
  if ((ITM->TCR & ITM_TCR_ITMENA_Msk)) {
    do {
      // Some versions of JLink (erroneously) disable SWO when debug connections
      // are closed. Setting the following again here fixes that problem.
      // Testing indicates that it's not actually necessary to set ITM->TER even
      // if it's cleared. This doesn't match the documentation, however, so it's
      // safer to leave it in place unless we can find something to support
      // making the change.
      CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
      ITM->TER |= (1UL << 8);
    } while (ITM->PORT[8].u32 == 0);
    ITM->PORT[8].u8 = byte;
  }

  return;
}
