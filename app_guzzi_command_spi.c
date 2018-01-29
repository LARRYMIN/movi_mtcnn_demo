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
* @file app_guzzi_command_spi.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <rtems/libio.h>
#include <utils/mms_debug.h>

#include <DrvGpio.h>

#include <OsDrvSpiSlaveCP.h>
#include <OsMessageProtocol.h>

#include "app_guzzi_command_spi.h"
#include "app_guzzi_spi_commands.h"

#ifndef APP_GUZZI_CAMMAND_SPI_HOST_NOTIFY_GPIO
#define APP_GUZZI_CAMMAND_SPI_HOST_NOTIFY_GPIO 22
#endif
#ifndef APP_GUZZI_CAMMAND_SPI_CH_ID
#define APP_GUZZI_CAMMAND_SPI_CH_ID SPI1
#endif
#ifndef APP_GUZZI_CAMMAND_SPI_VC_ID
#define APP_GUZZI_CAMMAND_SPI_VC_ID 1
#endif
#ifndef APP_GUZZI_CAMMAND_SPI_VC_PRIO
#define APP_GUZZI_CAMMAND_SPI_VC_PRIO 150
#endif

#define RTEMS_DRIVER_AUTO_MAJOR 0
#define SPI_COMMAND_BUFFER_SIZE 1024

mmsdbg_define_variable(
        vdl_app_guzzi_command_spi,
        DL_DEFAULT,
        0,
        "vdl_app_guzzi_command_spi",
        "SPI interface for GUZZI App."
    );
#define MMSDEBUGLEVEL mmsdbg_use_variable(vdl_app_guzzi_command_spi)

DRVSPI_CONFIGURATION(
        APP_GUZZI_CAMMAND_SPI_CH_ID,            /* dev */
        1,                                      /* cpol */
        1,                                      /* cpha */
        1,                                      /* bytesPerWord */
        0,                                      /* dmaEnabled */
        APP_GUZZI_CAMMAND_SPI_HOST_NOTIFY_GPIO, /* hostIrqGpio */
        10                                      /* interruptLevel */
    );
DECLARE_COMMUNICATION_PROTOCOL_DRIVER_TABLE(
        comm_protocol_dirver_table              /* drvTblName */
    );
DECLARE_OS_MESSAGING_VIRTUAL_CHANNEL(
        comm_protocol_vc,                       /* vcHandle */
        "app_guzzi_command_spi",                /* channelName */
        APP_GUZZI_CAMMAND_SPI_VC_ID,            /* channelId */
        APP_GUZZI_CAMMAND_SPI_VC_PRIO,          /* priority */
        SPI_COMMAND_BUFFER_SIZE,                /* rx_fifo_size */
        SPI_COMMAND_BUFFER_SIZE,                /* tx_fifo_size */
        ".ddr.bss"                              /* fifo_data_section */
    );

static rtems_device_major_number comm_protocol_major;
static char spi_command_buffer[SPI_COMMAND_BUFFER_SIZE] __attribute__((
        section(".ddr.bss"),
        aligned(8)
    ));

static uint32_t get_dynamic_tuning (app_guzzi_command_t *command, char * data)
{
    char        *next;
    uint32      i;
    uint8_t     *dtp_payload = command->live_tuning.data;

    command->id = APP_GUZZI_COMMAND__NOP;
    command->live_tuning.size = strtoul(data, &next, 0);

    if (command->live_tuning.size > sizeof(command->live_tuning.data)) {
        mmsdbg(DL_ERROR, "Dynamic dtp size: %d exceeded: %d",
                (int)command->live_tuning.size,
                sizeof(command->live_tuning.data)
                );
        goto EXIT_1;
    }

    command->live_tuning.offset = strtoul(next, &next, 0);
    for (i= 0; i < command->live_tuning.size; i ++) {
        *dtp_payload = strtoul(next, &next, 0);
        dtp_payload ++;
    }

    command->id = APP_GUZZI_COMMAND__LIVE_TUNING_APPLY;
    return 0;

EXIT_1:
    return -1;
}

static uint32_t get_tuning_id (app_guzzi_command_t *command, char *data)
{
    // Skip leading white spaces
    while (' ' == *data) {
        data++;
    }

    strncpy((char *)command->live_tuning.data,
            data,
            MAX_DYNAMIC_DTP_PAYLOAD_SIZE);

    command->id = APP_GUZZI_COMMAND__LIVE_TUNING_UNLOCK;
    return 0;
}


static uint32_t get_output_control (app_guzzi_command_t *command, char *data)
{


    command->id = APP_GUZZI_COMMAND__OUTPUT_CONTROL;
    command->output_control.camera_en_bit_mask = strtoul(
                                                            data,
                                                            &data,
                                                            0
                                                        );
    command->output_control.frame_type_en_bit_mask = strtoul(
                                                            data,
                                                            &data,
                                                            0
                                                        );
    command->output_control.frame_format_en_bit_mask = strtoul(
                                                            data,
                                                            &data,
                                                            0
                                                        );
    return 0;
}

app_guzzi_command_t *spi_command_to_app_guzzi_command(
        app_guzzi_command_t *command,
        char *command_spi
    )
{
    uint32_t command_spi_id;
    //uint32_t cammmm_id;
    char *command_spi_params;

    command_spi_id = strtoul(
            command_spi,
            &command_spi_params,
            0
        );
    //cammmm_id = sizeof(command);
    //mmsdbg(DL_ERROR, "command_spi_id=%d | cammmm_id:%d", command_spi_id, cammmm_id);
    command->cam.id = 0;
    switch (command_spi_id) {
        case APP_GUZZI_SPI_CMD_START_STREAM:
            command->id = APP_GUZZI_COMMAND__CAM_START;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            //mmsdbg(DL_ERROR, "command_spi_id=%d %d", command_spi_id, command->cam.id);
            break;
        case APP_GUZZI_SPI_CMD_STOP_STREAM:
            command->id = APP_GUZZI_COMMAND__CAM_STOP;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_REQ_STILL:
            command->id = APP_GUZZI_COMMAND__CAM_CAPTURE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_MOV_LENS:
            command->id = APP_GUZZI_COMMAND__CAM_LENS_MOVE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.lens_move.pos = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_FOCUS_TRIGGER:
            command->id = APP_GUZZI_COMMAND__CAM_AF_TRIGGER;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_AE_MANUAL:
            command->id = APP_GUZZI_COMMAND__CAM_AE_MANUAL;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.ae_manual.exp_us = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.ae_manual.sensitivity_iso = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.ae_manual.frame_duration_us = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_AE_AUTO:
            command->id = APP_GUZZI_COMMAND__CAM_AE_AUTO;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_SET_AWB_MODE:
            command->id = APP_GUZZI_COMMAND__CAM_AWB_MODE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.awb_mode.mode = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_SCENE_MODES:
            command->id = APP_GUZZI_COMMAND__CAM_SCENE_MODE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.scene_mode.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_ANTIBANDING_MODES:
            command->id = APP_GUZZI_COMMAND__CAM_ANTIBANDING_MODE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.antibanding_mode.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_AE_LOCK:
            command->id = APP_GUZZI_COMMAND__CAM_AE_LOCK;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.ae_lock_mode.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_AE_TARGET_FPS_RANGE:
            command->id = APP_GUZZI_COMMAND__CAM_AE_TARGET_FPS_RANGE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.ae_target_fps_range.min_fps = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.ae_target_fps_range.max_fps = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_AWB_LOCK:
            command->id = APP_GUZZI_COMMAND__CAM_AWB_LOCK;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.awb_lock_control.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_CAPTURE_INTENT:
            command->id = APP_GUZZI_COMMAND__CAM_CAPTURE_INTRENT;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.capture_intent.mode = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_CONTROL_MODE:
            command->id = APP_GUZZI_COMMAND__CAM_CONTROL_MODE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.control_mode.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_FRAME_DURATION:
            command->id = APP_GUZZI_COMMAND__CAM_FRAME_DURATION;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.frame_duration.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_EXPOSURE_COMPENSATION:
            command->id = APP_GUZZI_COMMAND__CAM_AE_EXPOSURE_COMPENSATION;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.exposure_compensation.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
                    break;
        case APP_GUZZI_SPI_CMD_SENSITIVITY:
            command->id = APP_GUZZI_COMMAND__CAM_SENSITIVITY;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.sensitivity.iso_val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
                    break;
        case APP_GUZZI_SPI_CMD_EFFECT_MODE:
            command->id = APP_GUZZI_COMMAND__CAM_EFFECT_MODE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.effect_mode.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_AF_MODE:
            command->id = APP_GUZZI_COMMAND__CAM_AF_MODE;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.af_mode.type = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_NOISE_REDUCTION_STRENGTH:
            command->id = APP_GUZZI_COMMAND__CAM_NOISE_REDUCTION_STRENGTH;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.noise_reduction_strength.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_SATURATION:
            command->id = APP_GUZZI_COMMAND__CAM_SATURATION;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.saturation.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_BRIGHTNESS:
            command->id = APP_GUZZI_COMMAND__CAM_BRIGHTNESS;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.brightness.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_STREAM_FORMAT:
            command->id = APP_GUZZI_COMMAND__CAM_FORMAT;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.format.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_CAM_RESOLUTION:
            command->id = APP_GUZZI_COMMAND__CAM_RESOLUTION;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.resolution.width = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.resolution.height = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_CMD_SHARPNESS:
            command->id = APP_GUZZI_COMMAND__CAM_SHARPNESS;
            command->cam.id = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            command->cam.sharpness.val = strtoul(
                    command_spi_params,
                    &command_spi_params,
                    0
                );
            break;
        case APP_GUZZI_SPI_LIVE_TUNING_UNLOCK:
            get_tuning_id(command, command_spi_params);
            break;
        case APP_GUZZI_SPI_LIVE_TUNING_APPLY:
            get_dynamic_tuning(command, command_spi_params);
            break;
        case APP_GUZZI_SPI_CMD_OUT_CTRL:
            get_output_control(command, command_spi_params);
            break;
        default:
            mmsdbg(
                    DL_ERROR,
                    "Unknown APP GUZZI SPI command id: %d",
                    (int)command_spi_id
                );
            command->id = APP_GUZZI_COMMAND__NOP;
    }

    return command;
}

static void notify_boot_ready(void)
{
    DrvGpioSetPinLo(APP_GUZZI_CAMMAND_SPI_HOST_NOTIFY_GPIO);
    DrvGpioSetPinHi(APP_GUZZI_CAMMAND_SPI_HOST_NOTIFY_GPIO);
}

/* TODO: Driver support! */
int app_guzzi_command_spi_peek(
        void *app_private,
        app_guzzi_command_callback_t *callback
    )
{
    app_guzzi_command_t command;
    char *command_spi;
    int n;

    command_spi = spi_command_buffer;

    n = MessagePassingRead( /* Expect to read whole packet! */
            APP_GUZZI_CAMMAND_SPI_VC_ID,
            command_spi,
            SPI_COMMAND_BUFFER_SIZE-1
        );
    if (n) {
        command_spi[n] = '\000';
        callback(
            app_private,
            spi_command_to_app_guzzi_command(
                    &command,
                    command_spi
                )
            );
        return 1;
    }

    return 0;
}

void app_guzzi_command_spi_wait(
        void *app_private,
        app_guzzi_command_callback_t *callback
    )
{
    app_guzzi_command_t command;
    char *command_spi;
    rtems_libio_rw_args_t tr;

    command_spi = spi_command_buffer;
    tr.buffer = command_spi;

    rtems_io_read(
            comm_protocol_major,
            APP_GUZZI_CAMMAND_SPI_VC_ID,
            &tr
        );
    if (tr.bytes_moved) {
        command_spi[tr.bytes_moved] = '\000';
        callback(
            app_private,
            spi_command_to_app_guzzi_command(
                    &command,
                    command_spi
                )
            );
    }
}

/* TODO: Driver support! */
int app_guzzi_command_spi_wait_timeout(
        void *app_private,
        app_guzzi_command_callback_t *callback,
        uint32_t timeout_ms
    )
{
    OsVirtualChannel *vc;
    app_guzzi_command_t command;
    char *command_spi;
    rtems_status_code rc;
    rtems_event_set events;
    int n;

    command_spi = spi_command_buffer;
    //mmsdbg(DL_ERROR, "command_spi:%d\n", command_spi);

    assert(vc = (OsVirtualChannel *)MessagePassingGetVirtualChannel(
            APP_GUZZI_CAMMAND_SPI_VC_ID
        ));

    n = BaseMessagePassingRead(
            (VirtualChannel *)vc,
            command_spi,
            SPI_COMMAND_BUFFER_SIZE-1
        );

    if (!n) {
        vc->rxWaitTaskId = rtems_task_self();
        rc = rtems_event_receive(
                RTEMS_EVENT_0,
                RTEMS_EVENT_ALL,
                (rtems_clock_get_ticks_per_second() / 1000) * timeout_ms,
                &events
            );
        if (rc == RTEMS_TIMEOUT) {
            return 0;
        }
        assert(!rc);
        n = BaseMessagePassingRead(
                (VirtualChannel *)vc,
                command_spi,
                SPI_COMMAND_BUFFER_SIZE-1
            );
    }
    //mmsdbg(DL_ERROR, "wrong command parsed");
    if (n) {
        command_spi[n] = '\000';
        callback(
            app_private,
            spi_command_to_app_guzzi_command(
                    &command,
                    command_spi
                )
            );
    }

    return n ? 1 : 0;
}

void app_guzzi_command_spi_init(void)
{
    //mmsdbg(DL_ERROR, "app_guzzi_command_spi_init E");
    /* TODO: common place? */
    assert(RTEMS_SUCCESSFUL == rtems_io_register_driver(
            RTEMS_DRIVER_AUTO_MAJOR,
            &comm_protocol_dirver_table,
            &comm_protocol_major
        ));

    assert(RTEMS_SUCCESSFUL == rtems_io_initialize(
            comm_protocol_major,
            0,
            &comm_protocol_vc
        ));
    assert(RTEMS_SUCCESSFUL == rtems_io_open(
            comm_protocol_major,
            APP_GUZZI_CAMMAND_SPI_VC_ID,
            NULL
        ));

    notify_boot_ready();
}

