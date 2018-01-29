/******************************************************************************

 @File         : initSystem.c
 @Author       : MT
 @Brief        : IPIPE Hw configuration on Los side
 Date          : 02 - Sep - 2014
 E-mail        : xxx.xx@movidius.com
 Copyright     : � Movidius Srl 2013, � Movidius Ltd 2014

 Description :


******************************************************************************/


/**************************************************************************************************
 ~~~ Included types first then APIs from other modules
 **************************************************************************************************/
#include <OsDrvCpr.h>
#include <DrvGpio.h>
#include <OsDrvTimer.h>
#include <DrvShaveL2Cache.h>
#include "initSystem.h"
#include <brdDefines.h>
#include <OsDrvCmxDma.h>
#include <OsDrvSvu.h>
#include <OsDrvCmxDma.h>
#include <DrvDdr.h>
#include <OsDrvShaveL2Cache.h>
#include "brdGpioCfgs/brdMv0182GpioDefaults.h"


/**************************************************************************************************
 ~~~  Specific #defines and types (typedef,enum,struct)
**************************************************************************************************/
#define CMX_CONFIG_SLICE_7_0       (0x11111111)
#define CMX_CONFIG_SLICE_15_8      (0x11111111)
#define L2CACHE_CFG                (SHAVE_L2CACHE_NORMAL_MODE)

#define CLOCKS_MIPICFG (AUX_CLK_MASK_MIPI_ECFG | AUX_CLK_MASK_MIPI_CFG)
#define REM_CLOCKS        (    AUX_CLK_MASK_I2S0  |     \
                                AUX_CLK_MASK_I2S1  |     \
                                AUX_CLK_MASK_I2S2  )

#define DESIRED_USB_FREQ_KHZ        (20000)
#if (DEFAULT_APP_CLOCK_KHZ%DESIRED_USB_FREQ_KHZ)
#error "Can not achieve USB Reference frequency. Aborting."
#endif

#define SHAVES_USED                (12)

/**************************************************************************************************
 ~~~  Global Data (Only if absolutely necessary)
**************************************************************************************************/
CmxRamLayoutCfgType __attribute__((section(".cmx.ctrl"))) __cmx_config = {
        CMX_CONFIG_SLICE_7_0, CMX_CONFIG_SLICE_15_8};

rtems_id fathomSemId;
/**************************************************************************************************
 ~~~  Static Local Data
 **************************************************************************************************/
tyAuxClkDividerCfg auxClkAllOn[] =
{
    {AUX_CLK_MASK_UART, CLK_SRC_REFCLK0, 96, 625},   // Give the UART an SCLK that allows it to generate an output baud rate of of 115200 Hz (the uart divides by 16)
    {
        .auxClockEnableMask     = REM_CLOCKS,                // all rest of the necessary clocks
        .auxClockSource         = CLK_SRC_SYS_CLK,            //
        .auxClockDivNumerator   = 1,                          //
        .auxClockDivDenominator = 1,                          //
    },
    {
        .auxClockEnableMask     = AUX_CLK_MASK_LCD ,          // LCD clock
        .auxClockSource         = CLK_SRC_SYS_CLK_DIV2,
        .auxClockDivNumerator   = 1,
        .auxClockDivDenominator = 1
    },
    {
        .auxClockEnableMask     = AUX_CLK_MASK_MEDIA,         // SIPP Clock
        .auxClockSource         = CLK_SRC_SYS_CLK ,      //
        .auxClockDivNumerator   = 1,                          //
        .auxClockDivDenominator = 1,                          //
    },
    //TODO: Disable CIF for release application?
    {
        .auxClockEnableMask     = (AUX_CLK_MASK_CIF0 | AUX_CLK_MASK_CIF1),  // CIFs Clock
        .auxClockSource         = CLK_SRC_SYS_CLK,            //
        .auxClockDivNumerator   = 1,                          //
        .auxClockDivDenominator = 1,                          //
    },
    {
        .auxClockEnableMask     = CLOCKS_MIPICFG,             // MIPI CFG + ECFG Clock
        .auxClockSource         = CLK_SRC_SYS_CLK     ,       //
        .auxClockDivNumerator   = 1,                          //
        .auxClockDivDenominator = (uint32_t)(DEFAULT_APP_CLOCK_KHZ/24000),                         //
    },
    {
        .auxClockEnableMask = AUX_CLK_MASK_MIPI_TX0 | AUX_CLK_MASK_MIPI_TX1,
        .auxClockSource = CLK_SRC_REFCLK0,
        .auxClockDivNumerator = 1,
        .auxClockDivDenominator = 1
    }, // ref clocks for MIPI PLL,
    {
        .auxClockEnableMask = (u32)(1 << CSS_AUX_TSENS),
        .auxClockSource = CLK_SRC_REFCLK0,
        .auxClockDivNumerator = 1,
        .auxClockDivDenominator = 10,
    },
    {
      .auxClockEnableMask     = AUX_CLK_MASK_USB_PHY_EXTREFCLK,
      .auxClockSource         = CLK_SRC_PLL0,
      .auxClockDivNumerator   = 1,
      .auxClockDivDenominator = (uint32_t)(DEFAULT_APP_CLOCK_KHZ/DESIRED_USB_FREQ_KHZ)
    },
    {
      .auxClockEnableMask     = AUX_CLK_MASK_USB_PHY_REF_ALT_CLK,
      .auxClockSource         = CLK_SRC_PLL0,
      .auxClockDivNumerator   = 1,
      .auxClockDivDenominator = (uint32_t)(DEFAULT_APP_CLOCK_KHZ/DESIRED_USB_FREQ_KHZ)
    },
    {
      .auxClockEnableMask     = AUX_CLK_MASK_USB_CTRL_REF_CLK,
      .auxClockSource         = CLK_SRC_PLL0,
      .auxClockDivNumerator   = 1,
      .auxClockDivDenominator = (uint32_t)(DEFAULT_APP_CLOCK_KHZ/DESIRED_USB_FREQ_KHZ)
    },
    {
      .auxClockEnableMask     = AUX_CLK_MASK_USB_CTRL_SUSPEND_CLK,
      .auxClockSource         = CLK_SRC_PLL0,
      .auxClockDivNumerator   = 1,
      .auxClockDivDenominator = (uint32_t)(DEFAULT_APP_CLOCK_KHZ/DESIRED_USB_FREQ_KHZ)
    },

    {0,0,0,0}, // Null Terminated List
};

//#############################################################################
#include <DrvLeonL2C.h>
void leonL2CacheInitWrThrough()
{
    LL2CConfig_t ll2Config;

    // Invalidate entire L2Cache before enabling it
    DrvLL2CFlushOpOnAllLines(LL2C_OPERATION_INVALIDATE, /*disable cache?:*/ 0);

    ll2Config.LL2CEnable = 1;
    ll2Config.LL2CLockedWaysNo = 0;
    ll2Config.LL2CWayToReplace = 0;
    ll2Config.busUsage = BUS_WRAPPING_MODE;
    ll2Config.hitRate = HIT_WRAPPING_MODE;
    ll2Config.replacePolicy = LRU;
    ll2Config.writePolicy = WRITE_THROUGH;

    DrvLL2CInitialize(&ll2Config);
}


/**************************************************************************************************
 ~~~ Functions Implementation
**************************************************************************************************/
int initClocksAndMemory(void)
{
//    int retVal;

    int ret_code = 0;
    int sc;
    // Configure the system
    OsDrvCprInit();
    OsDrvCprOpen();
    OsDrvTimerInit();
    OsDrvCprAuxClockArrayConfig(auxClkAllOn);
    //leonL2CacheInitWrThrough();
    // Set the shave L2 Cache mode
    //DrvShaveL2CacheSetMode(L2CACHE_CFG);

    // Enable PMB subsystem gated clocks and resets
    SET_REG_WORD   (CMX_RSTN_CTRL,0x00000000);               // engage reset
    SET_REG_WORD   (CMX_CLK_CTRL, 0xffffffff);               // turn on clocks
    SET_REG_WORD   (CMX_RSTN_CTRL,0xffffffff);               // dis-engage reset
    // Enable media subsystem gated clocks and resets
    SET_REG_WORD(MSS_CLK_CTRL_ADR,      0xffffffff);
    SET_REG_WORD(MSS_RSTN_CTRL_ADR,     0xffffffff);

    DrvCprSysDeviceAction(MSS_DOMAIN, ASSERT_RESET,  DEV_MSS_LCD |  DEV_MSS_CIF0 | DEV_MSS_CIF1 | DEV_MSS_SIPP);
    DrvCprSysDeviceAction(MSS_DOMAIN, DEASSERT_RESET, -1);
    DrvCprSysDeviceAction(CSS_DOMAIN, DEASSERT_RESET, -1);
    DrvCprSysDeviceAction(UPA_DOMAIN, DEASSERT_RESET, -1);

    DrvCprStartAllClocks();
    SET_REG_WORD(MSS_SIPP_RSTN_CTRL_ADR, 0);
    SET_REG_WORD(MSS_SIPP_RSTN_CTRL_ADR, 0x3ffffff);
    SET_REG_WORD(MSS_SIPP_CLK_SET_ADR,   0x3ffffff);

    // Init CmxDma
    if ((ret_code = (int)OsDrvCmxDmaInitDefault()) != OS_MYR_DRV_SUCCESS)
    {
        printf("Error: OsDrvCmxDmaInitDefault() FAILED, with error code: %d\n", ret_code);
        return ret_code;
    }

    DrvDdrInitialise(NULL);

    if ((ret_code = (int)rtems_semaphore_create(
        rtems_build_name('F', 'A', 'T', 'R'), 1, RTEMS_DEFAULT_ATTRIBUTES,
        RTEMS_NO_PRIORITY, &fathomSemId)) != RTEMS_SUCCESSFUL)
    {
        printf("Error: rtems_semaphore_create FAILED, with error code: %d\n,", ret_code);
        return ret_code;
    }

    if ((ret_code = (int)OsDrvSvuInit()) != OS_MYR_DRV_SUCCESS)
    {
        printf("Error: OsDrvSvuInit() FAILED, with error code: %d\n", ret_code);
        return ret_code;
    }

    // Explicitly turn off the Shave islands since RTEMS doesn't do it even if the
    // Shave clocks are disabled
    for (int i = POWER_ISLAND_SHAVE_0; i < POWER_ISLAND_SHAVE_0 + SHAVES_USED; i++)
        OsDrvCprPowerTurnOffIsland((enum PowerIslandIndex)i);

    // Set the shave L2 Cache mode
    sc = OsDrvShaveL2CacheInit(L2CACHE_CFG);
    if(sc)
        return sc;

    //Set Shave L2 cache partitions
    int part1Id, part2Id;
    ret_code = OsDrvShaveL2CGetPartition(SHAVEPART128KB, &part1Id);
    assert(ret_code == OS_MYR_DRV_SUCCESS);
    ret_code = OsDrvShaveL2CGetPartition(SHAVEPART128KB, &part2Id);
    assert(ret_code == OS_MYR_DRV_SUCCESS);

    printf("%d %d\n", part1Id, part2Id);

    //Allocate Shave L2 cache set partitions
    ret_code = OsDrvShaveL2CacheAllocateSetPartitions();
    assert(ret_code == OS_MYR_DRV_SUCCESS);

    // Do not allocate partitions here
    // this is done in main app each time we power on a SHAVE


    // Invalidate SHAVE cache
    ret_code = OsDrvShaveL2CachePartitionInvalidate(part1Id);
    assert(ret_code == OS_MYR_DRV_SUCCESS);
    ret_code = OsDrvShaveL2CachePartitionInvalidate(part2Id);
    assert(ret_code == OS_MYR_DRV_SUCCESS);

    return 0;
}


// assumed host will update all this parameters
void initSystem(void)
{
    initClocksAndMemory();
    DrvGpioIrqResetAll();
    DrvGpioInitialiseRange(brdMV0182GpioCfgDefault);
}

