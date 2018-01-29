/**************************************************************************************************

 @File         : sendout_config.h
 @Author       :
 @Brief        : Contains configuration for sendOut component
 Date          : 02-Nov-2017
 E-mail        : xxx.xxx@movidius.com
 @copyright All code copyright Movidius Ltd 2017, all rights reserved.
            For License Warranty see: common/license.txt

 Description :
    This file contains exported configuration for sendOut component

 **************************************************************************************************/

#ifndef SENDOUT_CONFIG_H
#define SENDOUT_CONFIG_H

/**************************************************************************************************
 ~~~ Includes
 **************************************************************************************************/
#include "mv_types.h"

#ifdef OUTPUT_UNIT_IS_HDMI
    #ifdef MV0212
#include "MV0212.h"
    #else
#include "Board182Api.h"
    #endif
#include "LcdApi.h"
#include "DrvI2cMaster.h"
#include "DrvADV7513.h"
#endif

#ifdef OUTPUT_UNIT_IS_MIPI
#include <DrvMipiDefines.h>
#include "DrvMss.h"
#endif

#ifdef OUTPUT_UNIT_IS_USB
#include "OsDrvUsbPhy.h"
#include "usbpumpdebug.h"
#include "usbpump_application_rtems_api.h"
#endif

/**************************************************************************************************
 ~~~  Specific #defines and types (typedef,enum,struct)
 **************************************************************************************************/

#ifndef APP_CONFIGURATION
    #warning SEND OUT: USE DEFAULT DEFINITIONS

    #ifdef OUTPUT_UNIT_IS_HDMI

        #define SEND_OUT_MAX_WIDTH          (4300)
        #define SEND_OUT_MAX_HEIGHT         (3200)        
        
    #endif

    #if defined(OUTPUT_UNIT_IS_USB) || defined(OUTPUT_UNIT_IS_MIPI)
        // For guzziPcApp it is ok to report some bigger size, but for real UVC camera you should be very accurate.
        // Please update *.urc file as well
        #define SEND_OUT_MAX_WIDTH          (4300)
        #define SEND_OUT_MAX_HEIGHT         (3200)

        #define USE_STATIC_HEADER
        #define SEND_OUT_HEADER_HEIGHT      (2)
    #endif
#else
//    #warning SEND OUT: USE APPLICATION DEFINITIONS
    #ifdef OUTPUT_UNIT_IS_HDMI
        #define SEND_OUT_MAX_WIDTH          (3842)
        #define SEND_OUT_MAX_HEIGHT         (2200)
    #endif

    #if defined(OUTPUT_UNIT_IS_USB) || defined(OUTPUT_UNIT_IS_MIPI)
        // For guzziPcApp it is ok to report some bigger size, but for real UVC camera you should be very accurate.
        // Please update *.urc file as well
        #define SEND_OUT_MAX_WIDTH          (4300)
        #define SEND_OUT_MAX_HEIGHT         (3200)

        #define USE_STATIC_HEADER
        #define SEND_OUT_HEADER_HEIGHT      (2)
    #endif
#endif


typedef struct
{
#ifdef OUTPUT_UNIT_IS_HDMI
    ADV7513ContfigMode_t advCfgMode;
    I2CM_Device **i2c_dev_hndl;
    LCDDisplayCfg *lcdCfg;
#else
    void *dummy;
#endif
} HdmiCfg_t;

typedef struct
{
#ifdef OUTPUT_UNIT_IS_MIPI
    eDrvMipiCtrlNo ctrlNo;
    drvMssDeviceType mss_device;
    int num_lanes;
    int tx_clock;
    u32 ref_clock_kHz;
    int use_irq;
#else
    void *dummy;
#endif
} MipiCfg_t;

// type of callback for usb input control
typedef void (*UsbSend_InputCtrlCallback_Type)(uint8_t *);

typedef struct
{
#ifdef OUTPUT_UNIT_IS_USB
    osDrvUsbPhyParam_t *phyParamInit;
    USBPUMP_APPLICATION_RTEMS_CONFIGURATION *dataPumpCfgInit;
    UsbSend_InputCtrlCallback_Type cb_function;
#else
    void *dummy;
#endif
} UsbCfg_t;


/**************************************************************************************************
 ~~~  Exported Functions
 **************************************************************************************************/

#endif // SENDOUT_CONFIG_H
