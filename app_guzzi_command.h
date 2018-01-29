/* =============================================================================
* Copyright (c) 2013-2015 MM Solutions AD
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
* @file app_guzzi_command.h
*
* @author ( MM Solutions AD )
*
*/
/* -----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 03-Jul-2015 : Author ( MM Solutions AD )
*! Created
* =========================================================================== */

#ifndef _APP_GUZZI_COMMAND_H
#define _APP_GUZZI_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MAX_DYNAMIC_DTP_PAYLOAD_SIZE (64)

typedef enum {
    APP_GUZZI_COMMAND__NOP, //0
    APP_GUZZI_COMMAND__CAM_START, //1
    APP_GUZZI_COMMAND__CAM_STOP, //2
    APP_GUZZI_COMMAND__CAM_CAPTURE, //3
    APP_GUZZI_COMMAND__CAM_LENS_MOVE, //4
    APP_GUZZI_COMMAND__CAM_AF_TRIGGER, //5
    APP_GUZZI_COMMAND__CAM_AE_MANUAL, //6
    APP_GUZZI_COMMAND__CAM_AE_AUTO, //7
    APP_GUZZI_COMMAND__CAM_AWB_MODE, //8
    APP_GUZZI_COMMAND__CAM_SCENE_MODE, //9
    APP_GUZZI_COMMAND__CAM_ANTIBANDING_MODE, //10
    APP_GUZZI_COMMAND__CAM_AE_EXPOSURE_COMPENSATION, //11
    sth_else2, //12
    APP_GUZZI_COMMAND__CAM_AE_LOCK, //13
    APP_GUZZI_COMMAND__CAM_AE_TARGET_FPS_RANGE, //14
    sth_else3, //15
    APP_GUZZI_COMMAND__CAM_AWB_LOCK, //16
    APP_GUZZI_COMMAND__CAM_CAPTURE_INTRENT, //17
    APP_GUZZI_COMMAND__CAM_CONTROL_MODE, //18
    sth_else4, //19
    sth_else5, //20
    APP_GUZZI_COMMAND__CAM_FRAME_DURATION, //21
    sth_else7, //22
    APP_GUZZI_COMMAND__CAM_SENSITIVITY, //23
    APP_GUZZI_COMMAND__CAM_EFFECT_MODE, //24
    sth_else8, //25
    APP_GUZZI_COMMAND__CAM_AF_MODE, //26
    APP_GUZZI_COMMAND__CAM_NOISE_REDUCTION_STRENGTH, //27
    APP_GUZZI_COMMAND__CAM_SATURATION, //28
    sth_else9, //29
    sth_else10, //30
    APP_GUZZI_COMMAND__CAM_BRIGHTNESS, //31
    sth_else11, //32
    APP_GUZZI_COMMAND__CAM_FORMAT, //33
    APP_GUZZI_COMMAND__CAM_RESOLUTION, //34
    APP_GUZZI_COMMAND__CAM_SHARPNESS, //35
    APP_GUZZI_COMMAND__LIVE_TUNING_UNLOCK, //36
    APP_GUZZI_COMMAND__LIVE_TUNING_APPLY, //37
    APP_GUZZI_COMMAND__OUTPUT_CONTROL, //38
    APP_GUZZI_COMMAND__MAX
} app_guzzi_command_id_t;

typedef struct {
    app_guzzi_command_id_t id;
    union {
        struct {
            uint32_t id;
            union {
                struct {
                    uint32_t pos;
                } lens_move;
                struct {
                    uint32_t exp_us;
                    uint32_t sensitivity_iso;
                    uint32_t frame_duration_us;
                } ae_manual;
                struct {
                    uint32_t mode;
                } awb_mode;
                struct {
                    uint32_t type;
                } scene_mode;
                struct {
                    uint32_t type;
                } antibanding_mode;
                struct {
                    uint32_t type;
                } ae_lock_mode;
                struct {
                    uint32_t min_fps;
                    uint32_t max_fps;
                } ae_target_fps_range;
                struct {
                    uint32_t type;
                } awb_lock_control;
                struct {
                    uint32_t mode;
                } capture_intent;
                struct {
                    uint32_t type;
                } control_mode;
                struct {
                    uint64_t val;
                } frame_duration;
                struct {
                    uint32_t val;
                } exposure_compensation;
                struct {
                    uint32_t iso_val;
                } sensitivity;
                struct {
                    uint32_t type;
                } effect_mode;
                struct {
                    uint32_t type;
                } af_mode;
                struct {
                    uint32_t val;
                } noise_reduction_strength;
                struct {
                    uint32_t val;
                } saturation;
                struct {
                    uint32_t val;
                } brightness;
                struct {
                    uint32_t val;
                } format;
                struct {
                    uint32_t width;
                    uint32_t height;
                } resolution;
                struct {
                    uint32_t val;
                } sharpness;
            };
        } cam;
        struct {
            uint32_t size;
            uint32_t offset;
            uint8_t  data[MAX_DYNAMIC_DTP_PAYLOAD_SIZE];
        } live_tuning;
        struct {
            uint32_t camera_en_bit_mask;
            uint32_t frame_type_en_bit_mask;
            uint32_t frame_format_en_bit_mask;
        } output_control;
    };
} app_guzzi_command_t;

typedef void app_guzzi_command_callback_t(
        void *app_private,
        app_guzzi_command_t *command
    );

int app_guzzi_command_peek(
        void *app_private,
        app_guzzi_command_callback_t *callback
    );
void app_guzzi_command_wait(
        void *app_private,
        app_guzzi_command_callback_t *callback
    );
int app_guzzi_command_wait_timeout(
        void *app_private,
        app_guzzi_command_callback_t *callback,
        uint32_t timeout_ms
    );

#ifdef __cplusplus
}
#endif

#endif /* _APP_GUZZI_COMMAND_H */
