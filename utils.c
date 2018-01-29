///
/// @file
/// @copyright All code copyright Movidius Ltd 2012, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     Application configuration Leon file
///

// 1: Includes
// ----------------------------------------------------------------------------

#include <mv_types.h>
#include <stdio.h>
#include "utils.h"

// 2:  Source Specific #defines and types  (typedef,enum,struct)
// ----------------------------------------------------------------------------

// 3: Global Data (Only if absolutely necessary)

// ----------------------------------------------------------------------------
// Sections decoration is required here for downstream tools

// 4: Static Local Data
// ----------------------------------------------------------------------------

// 5: Static Function Prototypes
// ----------------------------------------------------------------------------

// Required for synchronisation between internal USB thread and our threads

// 6: Functions Implementation
// ----------------------------------------------------------------------------
void UptoDown(s32 i, ClassifyResult *topKArray)
{
    s32 t1, t2, pos;
    fp32 tmpVal = 0;
    s32 tmpIdx;

    t1 = 2*i;
    t2 = t1+1;

    if(t1 > TOP_RESULT)
        return;
    else {
        if(t2>TOP_RESULT)
            pos=t1;
        else{
             //Get smaller value position.
            pos=(topKArray[t1].topK>topKArray[t2].topK)? t2:t1;
        }

        if(topKArray[i].topK> topKArray[pos].topK) {
            //new top value in
            tmpVal = topKArray[i].topK;
            tmpIdx = topKArray[i].topIdx;
            topKArray[i].topK= topKArray[pos].topK;
            topKArray[i].topIdx = topKArray[pos].topIdx;
            topKArray[pos].topK = tmpVal;
            topKArray[pos].topIdx = tmpIdx;
            UptoDown(pos, topKArray);
        }
    }
}
