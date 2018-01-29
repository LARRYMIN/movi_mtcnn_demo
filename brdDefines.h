#ifndef BRD_DEFINES_H
#define BRD_DEFINES_H

#ifdef MV0182
    #define BOARD_REF_CLOCK_KHZ              (12000)
#endif // MV0182

#ifdef MV0212
    #define BOARD_REF_CLOCK_KHZ              (12000)
#endif // MV0212

#ifndef BOARD_REF_CLOCK_KHZ
#error Please select proper BOARD type
#endif

#endif // BRD_DEFINES_H
