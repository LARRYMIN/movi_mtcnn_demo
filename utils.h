///
/// @file
/// @copyright All code copyright Movidius Ltd 2012, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     utils  Leon header
///

#ifndef _UTILS_H_
#define _UTILS_H_

#define TOP_RESULT          (5)
#include <ipipe.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    fp32 topK;
    s32 topIdx;

}ClassifyResult;

typedef struct
{
    int nWidth;       // image width
    int nHeight;      // image height
    int nPitch;       // image pitch
    int nFormat;      // image format 0:gray, 1: rgb
    int widthStart;           // start point--x
    int heightStart;           // start point--y
    int widthEnd;           // end point---x
    int heightEnd;           // end point---y
    unsigned char* pData;      // data pointer
} tNetImage, *LPXNetImage;

typedef tNetImage* NetImageHandle;

void UptoDown(s32 i, ClassifyResult *topKArray);

void Classify_Test(unsigned char *blob, FrameT *frame);

#ifdef __cplusplus
}
#endif

#endif
