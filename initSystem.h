/******************************************************************************

 @File         : initSystem.h
 @Author       : MT
 @Brief        : IPIPE preprocessor Hw configuration  API
 Date          : 02 - Sep - 2014
 E-mail        : xxx.xx@movidius.com
 Copyright     : � Movidius Srl 2013, � Movidius Ltd 2014

 Description :


******************************************************************************/

#ifndef INITSYSTEM_LOS_H
#define INITSYSTEM_LOS_H

/**************************************************************************************************
 ~~~ Includes
 **************************************************************************************************/
#include <brdDefines.h>
#include <DrvCpr.h>

/**************************************************************************************************
 ~~~  Specific #defines and types (typedef,enum,struct)
 **************************************************************************************************/
#ifndef DEFAULT_APP_CLOCK_KHZ
#define DEFAULT_APP_CLOCK_KHZ       480000
#endif

#ifndef DEFAULT_OSC_CLOCK_KHZ
#define DEFAULT_OSC_CLOCK_KHZ       BOARD_REF_CLOCK_KHZ
#endif

#define BIGENDIANMODE               (0x01000786)

#define APP_CSS_CLOCKS                  (( DEV_CSS_LOS               )   |    \
                                         ( DEV_CSS_LAHB_CTRL         )   |    \
                                         ( DEV_CSS_APB4_CTRL         )   |    \
                                         ( DEV_CSS_CPR               )   |    \
                                         ( DEV_CSS_ROM               )   |    \
                                         ( DEV_CSS_LOS_L2C           )   |    \
                                         ( DEV_CSS_MAHB_CTRL         )   |    \
                                         ( DEV_CSS_LOS_ICB           )   |    \
                                         ( DEV_CSS_LOS_DSU           )   |    \
                                         ( DEV_CSS_LOS_TIM           )   |    \
                                         ( DEV_CSS_GPIO              )   |    \
                                         ( DEV_CSS_JTAG              )   |    \
                                         ( DEV_CSS_APB1_CTRL         )   |    \
                                         ( DEV_CSS_AHB_DMA           )   |    \
                                         ( DEV_CSS_APB3_CTRL         )   |    \
                                         ( DEV_CSS_I2C0              )   |    \
                                         ( DEV_CSS_I2C1              )   |    \
                                         ( DEV_CSS_I2C2              )   |    \
                                         ( DEV_CSS_UART              )   |    \
                                         ( DEV_CSS_SPI0              )   |    \
                                         ( DEV_CSS_SPI1              )   |    \
                                         ( DEV_CSS_SPI2              )   |    \
                                         ( DEV_CSS_SAHB_CTRL         )   |    \
                                         ( DEV_CSS_MSS_MAS           )   |    \
                                         ( DEV_CSS_UPA_MAS           )   |    \
                                         ( DEV_CSS_DSS_APB           )   |    \
                                         ( DEV_CSS_DSS_BUS           )   |    \
                                         ( DEV_CSS_DSS_BUS_DXI       )   |    \
                                         ( DEV_CSS_DSS_BUS_AAXI      )   |    \
                                         ( DEV_CSS_DSS_BUS_MXI       )   |    \
                                         ( DEV_CSS_LAHB2SHB          )   |    \
                                         ( DEV_CSS_SAHB2MAHB         )   |    \
                                         ( DEV_CSS_USB               )   |    \
                                         ( DEV_CSS_USB_APBSLV        )   |    \
                                         ( DEV_CSS_AON               ))

#define APP_UPA_CLOCKS                     (( DEV_UPA_SHAVE_L2        ) | \
                                            ( DEV_UPA_CDMA             ) | \
                                            ( DEV_UPA_BIC              ) | \
                                            ( DEV_UPA_CTRL             ) | \
                                            ( DEV_UPA_SH0              ) | \
                                            ( DEV_UPA_SH1              ) | \
                                            ( DEV_UPA_SH2              ) | \
                                            ( DEV_UPA_SH3              ) | \
                                            ( DEV_UPA_SH4              ) | \
                                            ( DEV_UPA_SH5              ) | \
                                            ( DEV_UPA_SH6              ) | \
                                            ( DEV_UPA_SH7              ))

#define APP_MSS_CLOCKS                  (( DEV_MSS_APB_SLV          ) | \
                                         ( DEV_MSS_APB2_CTRL        ) | \
                                         ( DEV_MSS_RTBRIDGE         ) | \
                                         ( DEV_MSS_RTAHB_CTRL       ) | \
                                         ( DEV_MSS_LRT              ) | \
                                         ( DEV_MSS_LRT_DSU          ) | \
                                         ( DEV_MSS_LRT_L2C          ) | \
                                         ( DEV_MSS_LRT_ICB          ) | \
                                         ( DEV_MSS_AXI_BRIDGE       ) | \
                                         ( DEV_MSS_MXI_CTRL         ) | \
                                         ( DEV_MSS_MXI_DEFSLV       ) | \
                                         ( DEV_MSS_AXI_MON          ) | \
                                         ( DEV_MSS_LCD              ) | \
                                         ( DEV_MSS_TIM              ) | \
                                         ( DEV_MSS_AMC              ) | \
                                         ( DEV_MSS_SIPP             ))

#define APP_SIPP_CLOCKS                 (( DEV_SIPP_SIGMA           ) | \
                                         ( DEV_SIPP_LSC             ) | \
                                         ( DEV_SIPP_RAW             ) | \
                                         ( DEV_SIPP_DBYR            ) | \
                                         ( DEV_SIPP_DOGL            ) | \
                                         ( DEV_SIPP_LUMA            ) | \
                                         ( DEV_SIPP_SHARPEN         ) | \
                                         ( DEV_SIPP_CGEN            ) | \
                                         ( DEV_SIPP_MED             ) | \
                                         ( DEV_SIPP_CHROMA          ) | \
                                         ( DEV_SIPP_CC              ) | \
                                         ( DEV_SIPP_LUT             ) | \
                                         ( DEV_SIPP_UPFIRDN0        ) | \
                                         ( DEV_SIPP_UPFIRDN1        ) | \
                                         ( DEV_SIPP_UPFIRDN2        ) | \
                                         ( DEV_SIPP_MIPI_RX0        ) | \
                                         ( DEV_SIPP_MIPI_RX1        ) | \
                                         ( DEV_SIPP_MIPI_RX2        ) | \
                                         ( DEV_SIPP_MIPI            ) | \
                                         ( DEV_SIPP_SIPP_ABPSLV     ))

/**************************************************************************************************
 ~~~  Exported Functions
 **************************************************************************************************/

// ----------------------------------------------------------------------------
/// Setup all the clock configurations needed by this application and also the ddr
///
/// @return    0 on success, non-zero otherwise
int initClocksAndMemory(void);

void initSystem(void);

#endif //APPHWCONFIG_LOS_H
