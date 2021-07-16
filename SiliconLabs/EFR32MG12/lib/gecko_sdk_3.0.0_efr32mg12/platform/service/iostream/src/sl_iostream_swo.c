/***************************************************************************//**
 * @file
 * @brief IO Stream SWO Component.
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

#include "em_cmu.h"
#include "sl_iostream.h"
#include "sl_iostream_swo.h"
#include "sl_status.h"

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "common/include/rtos_err.h"
#include "common/source/rtos/rtos_utils_priv.h"
#include "kernel/include/os.h"
#endif

#if defined(GPIO_ROUTE_SWOPEN)     \
  || defined(GPIO_ROUTEPEN_SWVPEN) \
  || defined(GPIO_TRACEROUTEPEN_SWVPEN)

/*******************************************************************************
 *************************   LOCAL DATA TYPES   ********************************
 ******************************************************************************/

#if defined(SL_CATALOG_KERNEL_PRESENT)
typedef struct {
  OS_MUTEX lock;
} swo_stream_context_t;
#endif

/*******************************************************************************
 *********************   LOCAL FUNCTION PROTOTYPES   ***************************
 ******************************************************************************/

static sl_status_t swo_write(void  *context,
                             const void *buffer,
                             size_t buffer_length);

static void swo_setup(void);

static void swo_desetup(void);

/*******************************************************************************
 ******************************   VARIABLES   **********************************
 ******************************************************************************/

// Public stream variable for SWO io component
static sl_iostream_t sl_iostream_swo = {
  .write   = swo_write,
  .read    = NULL,
  .context = NULL
};

sl_iostream_t *sl_iostream_swo_handle = &sl_iostream_swo;

sl_iostream_instance_info_t sl_iostream_instance_swo_info = {
  .handle = &sl_iostream_swo,
  .name = "swo",
  .type = SL_IOSTREAM_TYPE_SWO,
  .periph_id = 0,
  .init = sl_iostream_swo_init,
};

#if defined(SL_CATALOG_KERNEL_PRESENT)
// Internal context variable for SWO io component
swo_stream_context_t swo_context;
#endif

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize SWO stream component.
 *
 * @return  Status Code:
 *            - SL_STATUS_OK
 ******************************************************************************/
sl_status_t sl_iostream_swo_init(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  RTOS_ERR err;

  // Create OS Mutex for lock
  OSMutexCreate(&swo_context.lock, "SWO Component Lock", &err);
  EFM_ASSERT(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);

  // Save context reference in SWO stream object
  sl_iostream_swo.context = &swo_context;
#endif

  // Configure SWO module
  swo_setup();

  // Set default IO Stream to SWO
  sl_iostream_set_system_default(&sl_iostream_swo);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * De-initialize SWO stream component.
 *
 * @return  Status Code:
 *            - SL_STATUS_OK
 ******************************************************************************/
sl_status_t sl_iostream_swo_deinit(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  RTOS_ERR err;

  // Acquire Mutex
  OSMutexPend(&swo_context.lock, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
  if (RTOS_ERR_CODE_GET(err) != RTOS_ERR_NONE) {
    return SL_STATUS_FAIL;
  }

  // Delete Mutex
  OSMutexDel(&swo_context.lock, OS_OPT_DEL_ALWAYS, &err);
  EFM_ASSERT(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
#endif

  // De-configure SWO module
  swo_desetup();

  return SL_STATUS_OK;
}

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * SWO stream component write function.
 ******************************************************************************/
static sl_status_t swo_write(void *context,
                             const void *buffer,
                             size_t buffer_length)
{
  uint8_t *buf = (uint8_t *)buffer;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  swo_stream_context_t *swo_context = (swo_stream_context_t *)context;
  RTOS_ERR              err;
  if (OSRunning) {
    // Bypass lock if we print before the kernel is running
    OSMutexPend(&swo_context->lock, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if (RTOS_ERR_CODE_GET(err) != RTOS_ERR_NONE) {
      return SL_STATUS_FAIL;
    }
  }
#else
  (void)context;
#endif

  // Write buffer
  for (size_t i = 0; i < buffer_length; i++) {
    (void)ITM_SendChar((uint32_t)buf[i]);
  }

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (OSRunning) {
    // Bypass lock if we print before the kernel is running
    OSMutexPost(&swo_context->lock, OS_OPT_POST_NONE, &err);
    EFM_ASSERT(RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE);
  }
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure SWO - serial wire output
 ******************************************************************************/
static void swo_setup(void)
{
  uint32_t tpiu_prescaler_val;

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

  // ITM Channel 0 is used for print-style output
  ITM->TER |= (1UL << 0);
}

/***************************************************************************//**
 * De-configure SWO - serial wire output
 ******************************************************************************/
static void swo_desetup(void)
{
  // ITM Channel 0 is used for print-style output
  ITM->TER |= (0UL << 0);

  // Unlock ITM and disabled trace
  ITM->LAR = 0xC5ACCE55;
  ITM->TCR = 0x0;

  // Disable Cycle Counter
  DWT->CTRL = 0;

  // Disable trace in core debug
  CoreDebug->DHCSR &= ~1;
  CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;

  // Enable Serial wire output pin
#if defined (_GPIO_ROUTE_MASK)
  GPIO->ROUTE &= ~GPIO_ROUTE_SWOPEN;
#endif
#if defined (_GPIO_ROUTEPEN_MASK)
  GPIO->ROUTEPEN &= ~GPIO_ROUTEPEN_SWVPEN;
#endif
}

#endif
