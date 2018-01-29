#include <Opipe.h>

#include <Flic.h>
#include <MemAllocator.h>

#include "PlgSource.h"
#include "PlgIspCtrl.h"
#include "PlgIspProc.h"
#include "PlgOutItf.h"

#include "PipeIsp3Cams.h"
#include "sched.h"

PlgSource         plgSrc    [APP_NR_OF_CAMS] SECTION(".cmx_direct.data"); //Source
PlgIspCtrl       *plgIspCtrl                 SECTION(".cmx_direct.data"); //Guzzi wrapper
PlgIspProc        plgIsp    [APP_NR_OF_CAMS] SECTION(".cmx_direct.data"); //ISP
PlgPool<ImgFrame> plgPoolSrc[APP_NR_OF_CAMS] SECTION(".cmx_direct.data"); //source out pool
PlgPool<ImgFrame> plgPoolIsp[APP_NR_OF_CAMS] SECTION(".cmx_direct.data"); //isp    out pool
PlgOutItf         plgOut                     SECTION(".cmx_direct.data"); //out

PlgIspProc        plgIspStl    [APP_NR_OF_CAMS] SECTION(".cmx_direct.data"); //ISP
PlgPool<ImgFrame> plgPoolIspStl[APP_NR_OF_CAMS] SECTION(".cmx_direct.data"); //isp    out pool

Pipeline          p                          SECTION(".cmx_direct.data");


void pipe3CamsCreate(GetSrcSzLimits getSrcSzLimits)
{
    uint32_t c = 0;
    OpipeReset();
    // create all plugins
    RgnAlloc.Create(RgnBuff, DEF_POOL_SZ);
    plgIspCtrl = PlgIspCtrl::instance();
    plgIspCtrl->Create();
    plgIspCtrl->schParam.sched_priority = CUSTOM_FLIC_PRIORITY;
    plgOut    .Create();
    plgOut.schParam.sched_priority = CUSTOM_FLIC_PRIORITY;
    for(c = (uint32_t)IC_SOURCE_0; c < APP_NR_OF_CAMS; c++) {
        plgSrc[c]    .Create((icSourceInstance)c); plgSrc[c].schParam.sched_priority = CUSTOM_FLIC_PRIORITY;
        plgSrc[c]    .outFmt   = SIPP_FMT_16BIT;
        icSourceSetup srcSet;
        if(0 == getSrcSzLimits(c, &srcSet)) {
            icSourceSetup* srcLimits = &srcSet;
            srcLimits->maxBpp = 16;
            plgPoolSrc[c].Create(&RgnAlloc, N_POOL_FRMS_SRC, ((srcLimits->maxPixels * srcLimits->maxBpp)) >> 3); //RAW
            plgPoolIsp[c].Create(&RgnAlloc, N_POOL_FRMS, (srcLimits->maxPixels * 3)>>1); //YUV420
            plgPoolIspStl[c].Create(&RgnAlloc, N_POOL_FRMS_STL, (srcLimits->maxPixels * 3)>>1); //YUV420
        }
        else {
            // not define max size for initialized camera
            assert(0);
        }
        //srcSet.appSpecificInfo incorporate information about 1.2 downscale preview will be applay or not on this image
        plgIsp[c].Create(c, srcSet.appSpecificInfo);
        plgIsp[c].schParam.sched_priority = CUSTOM_FLIC_PRIORITY;

        plgIspStl[c].Create(c+APP_NR_OF_CAMS, 0);
        plgIspStl[c].schParam.sched_priority = CUSTOM_FLIC_PRIORITY;
    }

    // add to pipeline
    p.Add(plgIspCtrl);
    p.Add(&plgOut);
    for(c = 0; c < APP_NR_OF_CAMS; c++) {
        p.Add(&plgSrc       [c]);
        p.Add(&plgIsp       [c]);
        p.Add(&plgIspStl    [c]);
        p.Add(&plgPoolSrc   [c]);
        p.Add(&plgPoolIsp   [c]);
        p.Add(&plgPoolIspStl[c]);

    }

    // link plugins
    for(c = 0; c < APP_NR_OF_CAMS; c++) {
        plgPoolSrc[c].out             .Link(&plgSrc[c].inO);
        plgSrc[c]    .outCommand      .Link(&plgIspCtrl->inCtrlResponse);
        plgIspCtrl -> outSrcCommand[c].Link(&plgSrc[c].inCommand);
        plgPoolIsp[c].out             .Link(&plgIsp[c].inO);
        plgSrc[c]    .out             .Link(&plgIsp[c].inI);
        plgIsp[c]    .outF            .Link(&plgOut.in );
        plgIsp[c]    .outE            .Link(&plgIspCtrl->inCtrlResponse);

        plgPoolIspStl[c].out          .Link(&plgIspStl[c].inO);
        plgSrc[c]       .outStl       .Link(&plgIspStl[c].inI);
        plgIspStl[c]    .outF         .Link(&plgOut.in );
        plgIspStl[c]    .outE         .Link(&plgIspCtrl->inCtrlResponse);

    }
    plgOut.outCmd.Link(&plgIspCtrl->inCtrlResponse);
    plgIspCtrl->outOutCmd.Link(&plgOut.inCmd);

    p.Start();
}

void pipe3CamsDestroy(void) {
    p.Stop();
    p.Wait();
    p.Delete();
    RgnAlloc.Delete();
}
