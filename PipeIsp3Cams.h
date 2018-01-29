///
/// @file
/// @copyright All code copyright Movidius Ltd 2012, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     Create and Destroy 3 camera flic base isp pipeline interface.
///
///
///


#ifndef _PIPE_ISP_3CAMS_H
#define _PIPE_ISP_3CAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CUSTOM_FLIC_PRIORITY
#define CUSTOM_FLIC_PRIORITY 211
#endif

#ifndef N_POOL_FRMS
#define N_POOL_FRMS 3
#endif

#ifndef N_POOL_FRMS_SRC
#define N_POOL_FRMS_SRC 4
#endif

#ifndef N_POOL_FRMS_STL
#define N_POOL_FRMS_STL 1
#endif

// by changing this define this pipeline can run any number of cams.
// it can run and any number of cams smaller that this
#ifndef APP_NR_OF_CAMS
#define APP_NR_OF_CAMS 1
#endif

typedef int (*GetSrcSzLimits)(uint32_t srcId, icSourceSetup* srcSet);

/// Create 3 camera flic base isp pipeline interface.
void pipe3CamsCreate(GetSrcSzLimits getSrcSzLimits);

///Destroy 3 camera flic base isp pipeline interface.
void pipe3CamsDestroy(void);


#ifdef __cplusplus
}
#endif

#endif
