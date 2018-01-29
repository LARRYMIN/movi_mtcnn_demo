/* =============================================================================
* Copyright (c) 2013-2014 MM Solutions AD
* All rights reserved. Property of MM Solutions AD.
*
* This source code may not be used against the terms and conditions stipulated
* in the licensing agreement under which it has been supplied, or without the
* written permission of MM Solutions. Rights to use, copy, modify, and
* distribute or disclose this source code and its documentation are granted only
* through signed licensing agreement, provided that this copyright notice
* appears in all copies, modifications, and distributions and subject to the
* following conditions:
* THIS SOURCE CODE AND ACCOMPANYING DOCUMENTATION, IS PROVIDED AS IS, WITHOUT
* WARRANTY OF ANY KIND, EXPRESS OR IMPLIED. MM SOLUTIONS SPECIFICALLY DISCLAIMS
* ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN
* NO EVENT SHALL MM SOLUTIONS BE LIABLE TO ANY PARTY FOR ANY CLAIM, DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
* PROFITS, OR OTHER LIABILITY, ARISING OUT OF THE USE OF OR IN CONNECTION WITH
* THIS SOURCE CODE AND ITS DOCUMENTATION.
* =========================================================================== */
/**
* @file
*
* @author ( MM Solutions AD )
*
*/
/* -----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 05-Nov-2013 : Author ( MM Solutions AD )
*! Created
* =========================================================================== */

#include <stdint.h>
#include <osal/osal_stdlib.h>
#include <osal/osal_string.h>
#include <osal/osal_time.h>
#include <utils/mms_debug.h>

#include <version_info.h>

#include "rtems.h"
#ifdef _REPORT_CPU_USAGE
#include "rtems/cpuuse.h"
#endif
//#include <pool_bm.h>

#include <platform/inc/platform.h>

#include <guzzi_event/include/guzzi_event.h>
#include <guzzi_event_global/include/guzzi_event_global.h>

#include <dtp/dtp_server_defs.h>

#include <components/camera/vcamera_iface/virt_cm/inc/virt_cm.h>

#include "initSystem.h"
#include "camera_control.h"
#include "app_guzzi_command_spi.h"
#include "app_guzzi_command_dbg.h"

#include "sendOutApi.h"

unsigned int do_not_use_i2c_ch_0 = 0;

dtp_server_hndl_t dtp_srv_hndl;
extern uint8_t ext_dtp_database[];
extern uint8_t ext_dtp_database_end[];
static uint8_t dtp_dtb_checked = 0;

mmsdbg_define_variable(
        vdl_guzzi_i2c,
        DL_DEFAULT,
        0,
        "vdl_guzzi_i2c",
        "Test android control."
    );
#define MMSDEBUGLEVEL mmsdbg_use_variable(vdl_guzzi_i2c)

#if INPUT_UNIT_IS_USB
#define APP_GUZZI_COMMAND_WAIT_TIMEOUT app_guzzi_command_wait_timeout_usb
#else
#define APP_GUZZI_COMMAND_WAIT_TIMEOUT app_guzzi_command_wait_timeout_spi
#endif
/*
 * ****************************************************************************
 * ** Temp functions referd by guzzi lib **************************************
 * ****************************************************************************
 */

int platform_drv_power_init(void);
int platform_drv_power_deinit(void);
int platform_cam_led_1(int action);

void guzzi_camera3_capture_result__x11_configure_streams(
        int camera_id,
        void *streams
    )
{
    UNUSED(camera_id);
    UNUSED(streams);
}

void guzzi_camera3_capture_result(
        int camera_id,
        unsigned int stream_id,
        unsigned int frame_number,
        void *data,
        unsigned int data_size
    )
{
    UNUSED(camera_id);
    UNUSED(stream_id);
    UNUSED(frame_number);
    UNUSED(data);
    UNUSED(data_size);
}

/*
 * ****************************************************************************
 * ** App GUZZI apply_tuning callback *****************************************
 * ****************************************************************************
 */
static void live_tuning_apply (app_guzzi_command_t *command)
{
    uint8_t *dest = ext_dtp_database + command->live_tuning.offset;
    uint8_t *src = command->live_tuning.data;

    if ((dest + command->live_tuning.size) > ext_dtp_database_end) {
        mmsdbg(DL_ERROR, "DTP offset: %d + size: %d exceeded: %d",
                (int)command->live_tuning.size,
                (int)command->live_tuning.offset,
                (int)(ext_dtp_database_end - ext_dtp_database)
                );
        return;
    }

    if (!dtp_dtb_checked) {
        mmsdbg(DL_ERROR, "DTP db is not checked");
        return;
    }

    while (command->live_tuning.size--) {
        *dest++ = *src++;
    }
}

/*
 * ****************************************************************************
 * ** App GUZZI check_tuning_id callback ***************************************
 * ****************************************************************************
 */

static void live_tuning_unlock (app_guzzi_command_t *command)
{
    dtp_hndl_t              hdtp;
    dtp_leaf_data_t         leaf;
    dtpdb_static_common_t   static_common;
    dtpdb_static_private_t  static_private;
    dtpdb_dynamic_common_t  dcomm;
    dtpdb_dynamic_private_t dprv;
    dtp_out_data_t          *dtp_data;

    osal_memset(&static_common, 0x00, sizeof(static_common));
    osal_memset(&static_private, 0x00, sizeof(static_private));
    osal_memset(&dcomm, 0x00, sizeof(dcomm));
    osal_memset(&dprv, 0x00, sizeof(dprv));

    if(dtpsrv_get_hndl(dtp_srv_hndl,
                            DTP_DB_ID_CHECK_SUM,
                            0,
                            0,
                            dtp_param_order,
                            &hdtp, &leaf))
    {
        mmsdbg(DL_ERROR, "DTP_DB_ID_CHECK_ID - can't get DTP client handle");
        goto EXIT_1;
    }

    dtpsrv_pre_process(hdtp, &static_common, &static_private);
    if (dtpsrv_process(hdtp, &dcomm, &dprv, (void**)&dtp_data))
    {
        mmsdbg(DL_ERROR, "DTP_DB_ID_CHECK_SUM - process error");
        goto EXIT_2;
    }

    if (DTP_DAT_TYPE_FIXED != dtp_data->data_type_id)
    {
        mmsdbg(DL_ERROR, "DTP_DB_ID_CHECK_SUM - wrong data type");
        goto EXIT_2;
    }

    if (osal_strcmp(dtp_data->d_fixed.p_data0, (const char*)command->live_tuning.data))
    {
        mmsdbg(DL_ERROR, "DTP_DB_ID_CHECK_SUM - mismatch");
        goto EXIT_2;
    }

    dtp_dtb_checked = 1;

EXIT_2:
    dtpsrv_free_hndl(hdtp);
EXIT_1:
    return;
}

/*
 * ****************************************************************************
 * ** Profile callback  *******************************************************
 * ****************************************************************************
 */
static void profile_ready_cb(
        profile_t *profile,
        void *prv,
        void *buffer,
        unsigned int buffer_size
    )
{
    UNUSED(prv);
    UNUSED(buffer);
    UNUSED(buffer_size);

    //printf(">>> prof: addr=%#010x size=%d\n", buffer, buffer_size);
    PROFILE_RELEASE_READY(buffer);
}

/*
 * ****************************************************************************
 * ** App GUZZI Command callback **********************************************
 * ****************************************************************************
 */
static void app_guzzi_command_callback(
        void *app_private,
        app_guzzi_command_t *command
    )
{
    UNUSED(app_private);

    mmsdbg(DL_ERROR, "command->id:%d", (int)command->id);

    switch (command->id) {
        case APP_GUZZI_COMMAND__LIVE_TUNING_UNLOCK:
            live_tuning_unlock(command);
            return;
        case APP_GUZZI_COMMAND__LIVE_TUNING_APPLY:
            live_tuning_apply(command);
            return;
        case APP_GUZZI_COMMAND__OUTPUT_CONTROL:
            //sendOutControl(command->output_control.camera_en_bit_mask,
            //               command->output_control.frame_type_en_bit_mask,
            //               command->output_control.frame_format_en_bit_mask);
            return;
        default:
            ; // do nothing - just pass thru
    }

    if (camera_control_is_active(command->cam.id))
    {
        if (APP_GUZZI_COMMAND__CAM_START == command->id) {
            mmsdbg(DL_ERROR, "Skipping command START for already activated camera %d", (int)command->cam.id);
            return;
        }
    } else {
        if (APP_GUZZI_COMMAND__CAM_START != command->id) {
            mmsdbg(DL_ERROR, "Skipping command %d for Non active camera %d", (int)command->id, (int)command->cam.id);
            return;
        }
    }
    switch (command->id) {
        case APP_GUZZI_COMMAND__NOP:
            //camera_control_start(0);
            break;
        case APP_GUZZI_COMMAND__CAM_START:
            PROFILE_ADD(PROFILE_ID_EXT_START_CMD, 0, 0);
            mmsdbg(DL_ERROR, "command \"%d %d\" sent\n", (int)command->id, (int)command->cam.id);
            camera_control_start(command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_STOP:
            PROFILE_ADD(PROFILE_ID_EXT_STOP_CMD, 0, 0);
            camera_control_stop(command->cam.id);
            mmsdbg(DL_ERROR, "command \"%d %d\" sent\n", (int)command->id, (int)command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_CAPTURE:
            PROFILE_ADD(PROFILE_ID_EXT_CAPTURE_CMD, 0, 0);
            camera_control_capture(command->cam.id);
            mmsdbg(DL_ERROR, "command \"%d %d\" sent\n", (int)command->id, (int)command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_LENS_MOVE:
            PROFILE_ADD(PROFILE_ID_EXT_LENS_MOVE, 0, 0);
            camera_control_lens_move(
                    command->cam.id,
                    command->cam.lens_move.pos
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.lens_move.pos);
            break;
        case APP_GUZZI_COMMAND__CAM_AF_TRIGGER:
            PROFILE_ADD(PROFILE_ID_EXT_LENS_MOVE, 0, 0);
            camera_control_focus_trigger(command->cam.id);
            mmsdbg(DL_ERROR, "command \"%d %d\" sent\n", (int)command->id, (int)command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_MANUAL:
            camera_control_ae_manual(
                    command->cam.id,
                    command->cam.ae_manual.exp_us,
                    command->cam.ae_manual.sensitivity_iso,
                    command->cam.ae_manual.frame_duration_us
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d %d %d\" sent\n",
                    (int)command->id,
                    (int)command->cam.id,
                    (int)command->cam.ae_manual.exp_us,
                    (int)command->cam.ae_manual.sensitivity_iso,
                    (int)command->cam.ae_manual.frame_duration_us);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_AUTO:
            camera_control_ae_auto(
                    command->cam.id,
                    CAMERA_CONTROL__AE_AUTO__FLASH_MODE__AUTO
                );
            mmsdbg(DL_ERROR, "command \"%d %d\" sent\n", (int)command->id, (int)command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_AWB_MODE:
            camera_control_awb_mode(
                    command->cam.id,
                    command->cam.awb_mode.mode
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.awb_mode.mode);
            break;
        case APP_GUZZI_COMMAND__CAM_SCENE_MODE:
            camera_control_scene_mode(
                    command->cam.id,
                    command->cam.scene_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.scene_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_ANTIBANDING_MODE:
            camera_control_antibanding_mode(
                    command->cam.id,
                    command->cam.antibanding_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.antibanding_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_LOCK:
            camera_control_ae_lock_mode(
                    command->cam.id,
                    command->cam.ae_lock_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.ae_lock_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_TARGET_FPS_RANGE:
            camera_control_ae_target_fps_range(
                    command->cam.id,
                    command->cam.ae_target_fps_range.min_fps,
                    command->cam.ae_target_fps_range.max_fps
                );
        mmsdbg(DL_ERROR, "command \"%d %d %d %d\" sent\n", (int)command->id,
                (int)command->cam.id, (int)command->cam.ae_target_fps_range.min_fps,
                (int)command->cam.ae_target_fps_range.max_fps);
            break;
        case APP_GUZZI_COMMAND__CAM_AWB_LOCK:
            camera_control_awb_lock_mode(
                    command->cam.id,
                    command->cam.awb_lock_control.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.awb_lock_control.type);
            break;
        case APP_GUZZI_COMMAND__CAM_CAPTURE_INTRENT:
            camera_control_capture_intent(
                    command->cam.id,
                    command->cam.capture_intent.mode
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.capture_intent.mode);
            break;
        case APP_GUZZI_COMMAND__CAM_CONTROL_MODE:
            camera_control_mode(
                    command->cam.id,
                    command->cam.control_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.control_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_FRAME_DURATION:
            camera_control_frame_duration(
                    command->cam.id,
                    command->cam.frame_duration.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.frame_duration.val);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_EXPOSURE_COMPENSATION:
            camera_control_exp_compensation(
                    command->cam.id,
                    command->cam.exposure_compensation.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.exposure_compensation.val);
            break;
        case APP_GUZZI_COMMAND__CAM_SENSITIVITY:
            camera_control_sensitivity(
                    command->cam.id,
                    command->cam.sensitivity.iso_val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.sensitivity.iso_val);
            break;
        case APP_GUZZI_COMMAND__CAM_EFFECT_MODE:
            camera_control_effect_mode(
                    command->cam.id,
                    command->cam.effect_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.effect_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_AF_MODE:
            camera_control_af_mode(
                    command->cam.id,
                    command->cam.af_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.af_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_NOISE_REDUCTION_STRENGTH:
            camera_control_noise_reduction_strength(
                    command->cam.id,
                    command->cam.noise_reduction_strength.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.noise_reduction_strength.val);
            break;
        case APP_GUZZI_COMMAND__CAM_SATURATION:
            camera_control_saturation(
                    command->cam.id,
                    command->cam.saturation.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.saturation.val);
            break;
        case APP_GUZZI_COMMAND__CAM_BRIGHTNESS:
            camera_control_brightness(
                    command->cam.id,
                    command->cam.brightness.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.brightness.val);
            break;
        case APP_GUZZI_COMMAND__CAM_FORMAT:
            camera_control_format(
                    command->cam.id,
                    command->cam.format.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.format.val);
            break;
        case APP_GUZZI_COMMAND__CAM_RESOLUTION:
            camera_control_resolution(
                    command->cam.id,
                    command->cam.resolution.width,
                    command->cam.resolution.height
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d %d\" sent\n", (int)command->id,
                    (int)command->cam.id, (int)command->cam.resolution.width,
                    (int)command->cam.resolution.height);
            break;
        case APP_GUZZI_COMMAND__CAM_SHARPNESS:
            camera_control_sharpness(
                    command->cam.id,
                    command->cam.sharpness.val
                );
            mmsdbg(DL_ERROR, "command \"%d %d %d\" sent\n",
                    (int)command->id, (int)command->cam.id, (int)command->cam.sharpness.val);
            break;
        default:
            mmsdbg(DL_ERROR, "Unknown App GUZZI Command: %d", (int)command->id);
    }
}

/*
 * ****************************************************************************
 * ** Temp observe function ***************************************************
 * ****************************************************************************
 */
/* TODO: Implement this in board/platform dependent part  */
int app_guzzi_command_wait_timeout_spi (void *app_private,
        app_guzzi_command_callback_t *callback,
                                        uint32_t timeout_ms)
{
    return app_guzzi_command_spi_wait_timeout(app_private, callback, timeout_ms)
         + app_guzzi_command_dbg_peek(app_private, callback);
}
//////////////////////////////////////////////////////////////

osal_sem    *app_guzzi_command_sem;

app_guzzi_command_t *spi_command_to_app_guzzi_command(app_guzzi_command_t *command, char *command_spi);

#define MAX_APP_GUZZI_COMMANDS  8
app_guzzi_command_t app_guzzi_commands[MAX_APP_GUZZI_COMMANDS];
int app_guzzi_command_rd = 0, app_guzzi_command_wr = 0;

int app_guzzi_command_get_wr_idx()
{
int idx = app_guzzi_command_wr + 1;
    if (idx >= MAX_APP_GUZZI_COMMANDS)
        idx = 0;
    if (app_guzzi_command_rd == idx)
    {
        mmsdbg(DL_ERROR, "Error: command queue overflow\n");
        return -1;
    }
    return idx;
}

void app_guzzi_command_commit_wr_idx(int idx)
{
    app_guzzi_command_wr = idx;
}


int app_guzzi_command_get_rd_idx()
{
int idx = app_guzzi_command_rd;

    if (app_guzzi_command_wr == idx)
    {
        mmsdbg(DL_ERROR, "Error: command queue overflow\n");
        return -1;
    }
    idx++;
    if (idx == MAX_APP_GUZZI_COMMANDS)
        idx = 0;

    return idx;
}

void app_guzzi_command_commit_rd_idx(int idx)
{
    app_guzzi_command_rd = idx;
}


void app_guzzi_command_execute(uint8_t *in_command)
{
int idx;
    idx = app_guzzi_command_get_wr_idx();
    if (idx < 0)
        return;
//    printf ("\nUSB CMD: \"%s\"\n\n", in_command);
    spi_command_to_app_guzzi_command(&app_guzzi_commands[idx], (char*)in_command);
    app_guzzi_command_commit_wr_idx(idx);
    osal_sem_post(app_guzzi_command_sem);
}


int app_guzzi_command_wait_timeout_usb(void *app_private,
                                       app_guzzi_command_callback_t *callback,
                                       uint32_t timeout_ms)
{
int rd_idx;
int err;

    app_guzzi_command_dbg_peek(app_private, callback);

    err = osal_sem_wait_timeout(app_guzzi_command_sem, timeout_ms);
    if (err)
    {
//        mmsdbg (DL_ERROR, "Sem wait err: %d time: %d ", err, (int)timeout_ms );
        return 1;
    }

    rd_idx = app_guzzi_command_get_rd_idx();

    if (rd_idx < 0)
        return -1;

    callback(app_private, &app_guzzi_commands[rd_idx]);

    app_guzzi_command_commit_rd_idx(rd_idx);

    return err;
}

/*
 * ****************************************************************************
 * ** Main ********************************************************************
 * ****************************************************************************
 */
int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    //mmsdbg(DL_ERROR, "main E");
    version_info_init();
    rtems_object_set_name(RTEMS_SELF, "main");
    initSystem();

    osal_init();
//    pool_bm_init();

    dtpsrv_create(&dtp_srv_hndl);
    dtpsrv_import_db(
            dtp_srv_hndl,
            ext_dtp_database,
            ext_dtp_database_end - ext_dtp_database
        );
    PROFILE_INIT(4096, 2, profile_ready_cb, NULL);

    guzzi_platform_init();
    guzzi_event_global_ctreate();

    sendOutCreate(&sendOut_initCfg);

    virt_cm_detect();


    app_guzzi_command_spi_init(); /* TODO: move to board/platform dependent part */

    app_guzzi_command_sem = osal_sem_create(0);

    platform_drv_power_init();
    platform_cam_led_1(0);

    for (;;) {
        APP_GUZZI_COMMAND_WAIT_TIMEOUT(
                NULL,
                app_guzzi_command_callback,
                1000
            );
#ifdef _REPORT_CPU_USAGE
        sleep(3);
        rtems_cpu_usage_report();
        rtems_cpu_usage_reset ();
#endif
    }

    platform_cam_led_1(1);
    platform_drv_power_deinit();

    osal_sem_destroy(app_guzzi_command_sem);
    return 0;
}

