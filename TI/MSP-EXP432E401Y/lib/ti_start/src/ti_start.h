#ifndef TI_START_H_INCLUDED
#define TI_START_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "ti/devices/msp432e4/driverlib/driverlib.h"
#include "stdio_redirect/uartstdio.h"

/* External declaration for system initialization function  */
extern void SystemInit(void);

/**
 * Initializes MCU, drivers and middleware in the project
 **/
void ti_start_init(void);

#ifdef __cplusplus
}
#endif
#endif
