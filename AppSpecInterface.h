/*
 * AppSpecInterface.h
 *
 *  Created on: Mar 6, 2017
 *      Author: truicam
 */

#ifndef APPS_GUZZISPI_MV182_A208M_B208M_C208M_MA2150_LEON_APPSPECINTERFACE_H_
#define APPS_GUZZISPI_MV182_A208M_B208M_C208M_MA2150_LEON_APPSPECINTERFACE_H_

#include "hal/hal_camera_module/hat_cm_driver.h"
//#include "sendOutApi.h"

#include <utils/mms_debug.h>
#include <utils/profile/profile.h>
#include <osal/osal_stdlib.h>
#include <osal/osal_mutex.h>
#include <osal/osal_time.h>
#include <osal/osal_string.h>

#include "sendOutApi.h"
#include "PipeIsp3Cams.h"

#ifndef PIPE_CREATE
#define PIPE_CREATE pipe3CamsCreate
#endif
#ifndef PIPE_DESTROY
#define PIPE_DESTROY pipe3CamsDestroy
#endif

#define APP_SPEC_DEFS01 mmsdbg_define_variable(vdl_ic, DL_DEFAULT, 0, "vdl_ic", "Guzzi IC.");


#ifndef MMSDEBUGLEVEL
#define MMSDEBUGLEVEL mmsdbg_use_variable(vdl_ic)
#endif

#define INSTANCES_COUNT_MAX     MAX_NR_OF_CAMS

#define APP_DBG_ERROR mmsdbg


static inline int getSrcLimits(uint32_t srcId, icSourceSetup* srcSet) {
    hat_camera_limits_t limits;
    if ( 0 == hai_cm_driver_get_camera_limits(srcId, &limits)) {
        srcSet->maxWidth  = limits.maxWidth;
        srcSet->maxHeight = limits.maxHeight;
        srcSet->maxPixels = limits.maxPixels;
        srcSet->maxBpp    = limits.maxBpp;
        return 0;
    }
    return -1;
}

#define UPDATE_SRC_LIMITS(SRC_ID,RETERN_VAL) getSrcLimits(SRC_ID,RETERN_VAL)

static inline void doPrint(int a, int b, int c) {
    printf("lg: %d, %d, %d \n", a, b, c);
    //UNUSED(a);UNUSED(b);UNUSED(c);
}
#define _PROFILE_ADD(ID, V1, V2) PROFILE_ADD(ID, V1, V2)
//PROFILE_ADD(ID, V1, V2)
//doPrint((int)ID, (int)V1, (int)V2)


#endif /* APPS_GUZZISPI_MV182_A208M_B208M_C208M_MA2150_LEON_APPSPECINTERFACE_H_ */
