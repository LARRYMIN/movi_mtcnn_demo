
/**************************************************************************************************

 @File         : sendout_config.c
 @Author       :
 @Brief        : configuration of sendOut component
 Date          : 02-Nov-2017
 E-mail        : xxx.xxx@movidius.com
 @copyright All code copyright Movidius Ltd 2017, all rights reserved.
            For License Warranty see: common/license.txt

 Description :
    This file is used for configuration of sendOut component

 **************************************************************************************************/

/**************************************************************************************************
 ~~~ Included types first then APIs from other modules
 **************************************************************************************************/
#include "mv_types.h"

#include "sendOutApi.h"
#ifdef OUTPUT_UNIT_IS_HDMI
#include "LcdCEA1080p60.h"
#endif
/**************************************************************************************************
 ~~~  Specific #defines
**************************************************************************************************/
#ifdef OUTPUT_UNIT_IS_MIPI
#define MIPI_TX_NUM_LANES       2
#define MIPI_TX_CLOCK           400
#define MIPI_TX_USE_IRQ         1
#endif

#ifdef OUTPUT_UNIT_IS_USB
#ifndef DISABLE_LEON_DCACHE
# define USBPUMP_MDK_CACHE_ENABLE        1
#else
# define USBPUMP_MDK_CACHE_ENABLE        0
#endif
#endif
/**************************************************************************************************
 ~~~  Global variables
**************************************************************************************************/
HdmiCfg_t hdmiInitCfg =
{
#ifdef OUTPUT_UNIT_IS_HDMI
    NULL, // not used for MV0182/MV0212 boards
    NULL, // not used for MV0182/MV0212 boards
    &lcdSpec1080p60
#else
    NULL
#endif
};

MipiCfg_t mipiInitCfg =
{
#ifdef OUTPUT_UNIT_IS_MIPI
    .ctrlNo = MIPI_CTRL_5,
    .mss_device = DRV_MSS_LCD,
    .num_lanes = MIPI_TX_NUM_LANES,
    .tx_clock = MIPI_TX_CLOCK,
    .ref_clock_kHz = 12000,
    .use_irq = MIPI_TX_USE_IRQ
#else
    NULL
#endif
};

#ifdef OUTPUT_UNIT_IS_USB
osDrvUsbPhyParam_t initParam =
{
        .enableOtgBlock    = USB_PHY_OTG_DISABLED,
        .useExternalClock  = 0, // USB_PHY_USE_EXT_CLK,
        .fSel              = USB_REFCLK_MHZ,
        .refClkSel0        = USB_SUPER_SPEED_CLK_CONFIG,
        .forceHsOnly       = USB_PHY_HS_ONLY_OFF
};

USBPUMP_APPLICATION_RTEMS_CONFIGURATION sg_DataPump_AppConfig =
    USBPUMP_APPLICATION_RTEMS_CONFIGURATION_INIT_V1(
        /* nEventQueue */             64,
        /* pMemoryPool */             NULL,
        /* nMemoryPool */             0,
        /* DataPumpTaskPriority */    100,
        /* DebugTaskPriority */       200,
        /* UsbInterruptPriority */    10,
        /* pDeviceSerialNumber */     NULL,
        /* pUseBusPowerFn */          NULL,
        /* fCacheEnabled */           USBPUMP_MDK_CACHE_ENABLE,
        /* DebugMask */               UDMASK_ANY | UDMASK_ERRORS );

extern void app_guzzi_command_execute(uint8_t *in_command);

#endif

UsbCfg_t usbInitCfg =
{
#ifdef OUTPUT_UNIT_IS_USB
    &initParam,
    &sg_DataPump_AppConfig,
	&app_guzzi_command_execute
#else
    NULL
#endif
};

SendOutInitCfg_t sendOut_initCfg =
{
    &hdmiInitCfg,
    &mipiInitCfg,
    &usbInitCfg
};
/**************************************************************************************************
 ~~~ Imported Function Declaration
**************************************************************************************************/

/**************************************************************************************************
 ~~~ Local File function declarations
**************************************************************************************************/

/**************************************************************************************************
 ~~~ Functions Implementation
**************************************************************************************************/
