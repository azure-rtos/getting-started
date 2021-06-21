#include "ti_start.h"

#define EMAC_PHY_CONFIG (EMAC_PHY_TYPE_INTERNAL | EMAC_PHY_INT_MDIX_EN |      \
                         EMAC_PHY_AN_100B_T_FULL_DUPLEX)


//*****************************************************************************
//
// System clock rate in Hz.
//
//*****************************************************************************
uint32_t g_ui32SysClock;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// Configure the UART and its pins.
//
//*****************************************************************************
void
ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
    MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
    MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);
}


//*****************************************************************************
//
// Configure the EMAC.
//
//*****************************************************************************
void
ConfigureEMAC(void)
{
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MACArray[8];

    //
    // Configure the hardware MAC address for Ethernet Controller filtering of
    // incoming packets.  The MAC address will be stored in the non-volatile
    // USER0 and USER1 registers.
    //
    MAP_FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
        //
        // We should never get here.  This is an error if the MAC address has
        // not been programmed into the device.  Exit the program.
        // Let the user know there is no MAC address
        //
        printf("No MAC programmed!\n");
        while(1)
        {
        }
    }

    //
    // Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
    // address needed to program the hardware registers, then program the MAC
    // address into the Ethernet Controller registers.
    //
    pui8MACArray[0] = ((ui32User0 >>  0) & 0xff);
    pui8MACArray[1] = ((ui32User0 >>  8) & 0xff);
    pui8MACArray[2] = ((ui32User0 >> 16) & 0xff);
    pui8MACArray[3] = ((ui32User1 >>  0) & 0xff);
    pui8MACArray[4] = ((ui32User1 >>  8) & 0xff);
    pui8MACArray[5] = ((ui32User1 >> 16) & 0xff);


    //
    // Enable the ethernet peripheral.
    //
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EMAC0);
    MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EMAC0);

    //
    // Enable the internal PHY if it's present and we're being
    // asked to use it.
    //
    if((EMAC_PHY_CONFIG & EMAC_PHY_TYPE_MASK) == EMAC_PHY_TYPE_INTERNAL)
    {
        //
        // We've been asked to configure for use with the internal
        // PHY.  Is it present?
        //
        if(MAP_SysCtlPeripheralPresent(SYSCTL_PERIPH_EPHY0))
        {
            //
            // Yes - enable and reset it.
            //
            MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EPHY0);
            MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EPHY0);
        }
        else
        {
            //
            // Internal PHY is not present on this part so hang here.
            //
            while(1)
            {
            }
        }
    }

    //
    // Wait for the MAC to come out of reset.
    //
    while(!MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_EMAC0))
    {
    }

    //
    // Configure for use with whichever PHY the user requires.
    //
    EMACPHYConfigSet(EMAC0_BASE, EMAC_PHY_CONFIG);

    //
    // Reset the MAC.
    //
    //MAP_EMACReset(EMAC0_BASE);

    //
    // Initialize the MAC and set the DMA mode.
    //
    MAP_EMACInit(EMAC0_BASE, g_ui32SysClock,
                 EMAC_BCONFIG_MIXED_BURST | EMAC_BCONFIG_PRIORITY_FIXED,
                 4, 4, 0);

    //
    // Set MAC configuration options.
    //
    MAP_EMACConfigSet(EMAC0_BASE, (EMAC_CONFIG_FULL_DUPLEX |
                               //EMAC_CONFIG_100MBPS |
                                   EMAC_CONFIG_CHECKSUM_OFFLOAD |
                                   EMAC_CONFIG_7BYTE_PREAMBLE |
                                   EMAC_CONFIG_IF_GAP_96BITS |
                                   EMAC_CONFIG_USE_MACADDR0 |
                                   EMAC_CONFIG_SA_FROM_DESCRIPTOR |
                                   EMAC_CONFIG_BO_LIMIT_1024),
                      (EMAC_MODE_RX_STORE_FORWARD |
                       EMAC_MODE_TX_STORE_FORWARD |
                       EMAC_MODE_TX_THRESHOLD_64_BYTES |
                       EMAC_MODE_RX_THRESHOLD_64_BYTES), 0);

    //
    // Program the hardware with its MAC address (for filtering).
    //
    MAP_EMACAddrSet(EMAC0_BASE, 0, (uint8_t *)&pui8MACArray);
}


/**
 * Initializes MCU, drivers and middleware in the project
 **/
void ti_start_init(void)
{
    //
    // Run from the PLL at 120 MHz.
    //
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |
                SYSCTL_CFG_VCO_480), 120000000);

    ConfigureUART();

    ConfigureEMAC();
}
