#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <rtems.h>
#include <bsp.h>

#include <rtems/bspIo.h>

#include <semaphore.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <mv_types.h>
#include <rtems/cpuuse.h>
#include <DrvLeon.h>
#include <initSystem.h>
//#include "rtems_config.h"
//#define CMX_CONFIG_SLICE_7_0       (0x11111111)
//#define CMX_CONFIG_SLICE_15_8      (0x11111111)
//#define L2CACHE_NORMAL_MODE (0x6)  // In this mode the L2Cache acts as a cache for the DRAM
//#define L2CACHE_CFG     (L2CACHE_NORMAL_MODE)
//#define BIGENDIANMODE   (0x01000786)
/* Sections decoration is require here for downstream tools */
//extern CmxRamLayoutCfgType __cmx_config;
//CmxRamLayoutCfgType __attribute__((section(".cmx.ctrl"))) __cmx_config = {CMX_CONFIG_SLICE_7_0, CMX_CONFIG_SLICE_15_8};
//uint32_t __l2_config   __attribute__((section(".l2.mode")))  = L2CACHE_CFG;




int main(int argc, char **argv);

void* POSIX_Init (void *args)
{
    UNUSED(args);

    main(0, NULL);
	return NULL;
}

#if !defined (__CONFIG__)
#define __CONFIG__

/* ask the system to generate a configuration table */
#define CONFIGURE_INIT

#define CONFIGURE_MICROSECONDS_PER_TICK 1000 /* 1 milliseconds */

#define CONFIGURE_TICKS_PER_TIMESLICE 10

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_POSIX_INIT_THREAD_TABLE

//#define  CONFIGURE_MINIMUM_TASK_STACK_SIZE 8192
/*3K is usually enough for a thread*/
#define  CONFIGURE_MINIMUM_TASK_STACK_SIZE (8*1024)

#define CONFIGURE_MAXIMUM_POSIX_THREADS              80

#define CONFIGURE_MAXIMUM_POSIX_MUTEXES              600

#define CONFIGURE_MAXIMUM_POSIX_KEYS                 50

#define CONFIGURE_MAXIMUM_POSIX_SEMAPHORES           600

#define CONFIGURE_MAXIMUM_POSIX_CONDITION_VARIABLES 16

#define CONFIGURE_MAXIMUM_POSIX_TIMERS          4

#define CONFIGURE_MAXIMUM_TIMERS                4

#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES 8
#define CONFIGURE_MAXIMUM_TASKS                 4

#define CONFIGURE_MAXIMUM_REGIONS                1
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_MAXIMUM_DRIVERS 10
#define CONFIGURE_MAXIMUM_SEMAPHORES 16
#define CONFIGURE_MAXIMUM_DEVICES 8
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_NULL_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER

// User extension to be able to catch abnormal terminations
static void Fatal_extension(
    Internal_errors_Source  the_source,
    bool                    is_internal,
    Internal_errors_t       the_error
    )
{
    switch(the_source)
    {
    case RTEMS_FATAL_SOURCE_EXIT:
        if(the_error)
            printk("Exited with error code %d\n", the_error);
        break; // normal exit
    case RTEMS_FATAL_SOURCE_ASSERT:
        printk("%s : %d in %s \n",
               ((rtems_assert_context *)the_error)->file,
               ((rtems_assert_context *)the_error)->line,
               ((rtems_assert_context *)the_error)->function);
        break;
    case RTEMS_FATAL_SOURCE_EXCEPTION:
        rtems_exception_frame_print((const rtems_exception_frame *) the_error);
        break;
    default:
        printk ("\nSource %d Internal %d Error %d  0x%X:\n",
                the_source, is_internal, the_error, the_error);
        break;
    }
}

#define CONFIGURE_MAXIMUM_USER_EXTENSIONS    1
#define CONFIGURE_INITIAL_EXTENSIONS         { .fatal = &Fatal_extension }

#include <rtems/confdefs.h>
#define MSS_CLOCKS 0xFFFFFFFF
/*
#define MSS_CLOCKS         (  DEV_MSS_APB_SLV     |     \
                              DEV_MSS_APB2_CTRL   |     \
                              DEV_MSS_RTBRIDGE    |     \
                              DEV_MSS_RTAHB_CTRL  |     \
                              DEV_MSS_LRT         |     \
                              DEV_MSS_LRT_DSU     |     \
                              DEV_MSS_LRT_L2C     |     \
                              DEV_MSS_LRT_ICB     |     \
                              DEV_MSS_AXI_BRIDGE  |     \
                              DEV_MSS_MXI_CTRL    |     \
                              DEV_MSS_MXI_DEFSLV  |     \
                              DEV_MSS_AXI_MON     |     \
                              DEV_MSS_MIPI        |     \
                              DEV_MSS_CIF0        |     \
                              DEV_MSS_LCD         |     \
                              DEV_MSS_AMC         |     \
                              DEV_MSS_SIPP        |     \
                              DEV_MSS_TIM         |     \
                              DEV_MSS_SIPP_ABPSLV )
*/

// Program the booting clocks
// System Clock configuration on start-up
BSP_SET_CLOCK(DEFAULT_OSC_CLOCK_KHZ, DEFAULT_APP_CLOCK_KHZ, 1, 1, APP_CSS_CLOCKS, APP_MSS_CLOCKS, APP_UPA_CLOCKS, APP_SIPP_CLOCKS,0);
// Program the  L2C at startup
#if (MV_RTEMS_VERSION > 0x02000000)
BSP_SET_L2C_CONFIG(1, L2C_REPL_LRU, 0, L2C_MODE_WRITE_THROUGH, 0, NULL);
#else
BSP_SET_L2C_CONFIG(1, L2C_REPL_LRU, 0, L2C_MODE_WRITE_THROUGH, 0, NULL);
#endif

#endif /* if defined CONFIG */
