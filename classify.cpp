#include <OsDrvSvu.h>
#include <OsDrvShaveL2Cache.h>
#include <swcShaveLoaderLocal.h>
#include <OsDrvCpr.h>
#include <DrvLeonL2C.h>
#include <DrvShaveL2Cache.h>
#include <iostream>
#include <vector>


#include <opencv2/opencv.hpp>

#include "mtcnn/rgb2yuv.h"


#include "mtcnn/mtcnn.h"
using namespace std;  

#if defined(__RTEMS__)
#include <OsDrvCmxDma.h> 
#else
#include <DrvCmxDma.h>
#endif

#include "fathomRun.h"
#include "utils.h"
#include <Fp16Convert.h>
#include <VcsHooksApi.h>

#ifndef DDR_DATA
#define DDR_DATA  __attribute__((section(".ddr.data")))
#endif 
#ifndef DDR_BSS
#define DDR_BSS  __attribute__((section(".ddr.bss")))
#endif
#ifndef DMA_DESCRIPTORS_SECTION
#define DMA_DESCRIPTORS_SECTION __attribute__((section(".cmx.cdmaDescriptors")))
#endif

#define OUTPUTSIZE 2*56*56*192
#define F_BSS_SIZE 40000000

#define GoogleNet_Dim 320  // 720    //  224   //300  
const unsigned int cache_memory_size = 25 * 1024 * 1024;
const unsigned int scratch_memory_size = 110 * 1024;

char DDR_BSS cache_memory[cache_memory_size];

#define MAX_RESOLUTION (2104*1560)

#define MAXI_PIXEL_SIZE (4)

#define RGB_PIXEL_SZ (3)

unsigned char DDR_BSS rgb_buf[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

unsigned char DDR_BSS rgb_buf_resized[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

float DDR_BSS rgb_buf_fp32[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

fp16 DDR_BSS rgb_buf_fp16[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

float __attribute__((section(".cmx.data"))) networkMean[] = {0.40787054*255.0, 0.45752458*255.0, 0.48109378*255.0};

char __attribute__((section(".cmx.bss"))) scratch_memory[scratch_memory_size];

dmaTransactionList_t DMA_DESCRIPTORS_SECTION task[1];

u8 DDR_BSS fathomBSS[F_BSS_SIZE] __attribute__((aligned(64)));
u8 DDR_DATA fathomOutput[OUTPUTSIZE] __attribute__((aligned(64)));

static float __attribute__((section(".cmx.data"))) YUV2RGB_CONVERT_MATRIX[3][3] = { { 1, 0, 1.4022 }, 
	                                                                                                { 1, -0.3456, -0.7145 }, 
	                                                                                                { 1, 1.771, 0 } 
                                                                                                       };




extern DynamicContext_t MODULE_DATA(YUV2RGB);
extern DynamicContext_t MODULE_DATA(ImgResize);



tyTimeStamp timer_data_local_det;
u64 cyclesElapsed_local_det;
u32 clocksPerUs;
float usedMs = 0.0;











extern "C"
 //convert to FP16
void  ConvU8toFP16(unsigned char *in, fp16 *out, int img_size)
 {
	 for(int i =0; i<img_size; i++){
		 out[i] = f32Tof16(((fp32)in[i]) - networkMean[i%3]);
	 }
 }

 void NetResize(NetImageHandle inPut, NetImageHandle outPut, int pixSize)
 {
	 float cbufy0, cbufy1;
	 float cbufx0, cbufx1;
	 int startX, startY;
	 float sv, su;
	 int i, j;
	 int widthStart = inPut->widthStart;
	 int heightStart = inPut->heightStart;
	 int widthEnd = inPut->widthEnd;
	 int heightEnd = inPut->heightEnd;
	 //int srcWidth = inPut->nWidth;
	 //int srcHeight = inPut->nHeight;
	 int srcPitch = inPut->nWidth*pixSize;
	 int dstWidth = outPut->nWidth;
	 int dstHeight = outPut->nHeight;
	 int dstPitch = outPut->nWidth * pixSize;
	 float xScale = (widthEnd-widthStart)/(float)dstWidth;
	 float yScale = (heightEnd-heightStart)/(float)dstHeight;
	 unsigned char* inputFrame = inPut->pData;
	 unsigned char* outputFrame = outPut->pData;
 
	 for(j=0; j<dstHeight; j++) {
		 startY = floor((j+0.5)*yScale-0.5);
		 su = (j+0.5)*yScale-0.5-startY;
		 if(startY<0){
		 su=0,startY=0;
		 }
		 if(startY>=heightEnd-heightStart-1){
		 su=0,startY=heightEnd-heightStart-2;
		 }
 
		 startY = startY+heightStart;
		 cbufy0 = 1.0f-su;
		 cbufy1 = su;
 
		 for(i=0; i<dstWidth*pixSize ; i++) {
			 startX = floor((i/pixSize+0.5)*xScale-0.5);
			 sv = (i/pixSize+0.5)*xScale-0.5-startX;
			 if(startX<0) {
				 sv=0,startX=0;
			 }
			 if(startX>=widthEnd-widthStart-1) {
				 sv=0,startX=widthEnd-widthStart-2;
			 }
 
			 startX = startX+widthStart;
			 cbufx0 = 1.0f-sv;
			 cbufx1 = sv;
 
			 float fvalue = (float)(inputFrame[startY*srcPitch+startX*pixSize+i%pixSize]*cbufy0*cbufx0+
			 inputFrame[(startY+1)*srcPitch+startX*pixSize+i%pixSize]*cbufy1*cbufx0+
			 inputFrame[startY*srcPitch+(startX+1)*pixSize+i%pixSize]*cbufy0*cbufx1+
			 inputFrame[(startY+1)*srcPitch+(startX+1)*pixSize+i%pixSize]*cbufy1*cbufx1);
			 outputFrame[j*dstPitch+i] = (unsigned char)(fvalue);
		 }
	 }
 
	 return;
 }

 int ImageResize(unsigned char *in, unsigned char *out, int src_w, int src_h, int dst_w, int dst_h)
 {
	 tNetImage inputFrame;
	 tNetImage outputFrame;
	 int pixSize= 3;
	 int outputImageSize=0;
 
	  {
		 inputFrame.nWidth = src_w;
		 inputFrame.nHeight = src_h;
		 //inputFrame.nFormat = pStreamInfo->resizeFormat; //no use
		 inputFrame.widthStart = 0;
		 inputFrame.widthEnd = src_w;
		 inputFrame.heightStart = 0;
		 inputFrame.heightEnd = src_h;
		 inputFrame.pData = in;
 
		 outputFrame.nWidth = dst_w;
		 outputFrame.nHeight = dst_h;
		 //outputFrame.nFormat = pStreamInfo->resizeFormat;
		 outputFrame.widthStart = 0;
		 outputFrame.widthEnd = dst_w;
		 outputFrame.heightStart = 0;
		 outputFrame.heightEnd = dst_h;
		 outputFrame.pData = out;
 
			 inputFrame.nPitch = src_w*3;
			 outputFrame.nPitch = dst_w*3;
			 pixSize = 3;
 
		 NetResize(&inputFrame, &outputFrame, pixSize);
		 outputImageSize = outputFrame.nWidth * outputFrame.nHeight*pixSize;
	 }
	 return outputImageSize;
 }

 static void RunShvImageResize(unsigned char *in, unsigned char *out, int src_w, int src_h, int dst_w, int dst_h)
 {
	 int first_shave = 0;
	 int last_shave = 0;
	 int status;
	 
	 u32 running;
	 
	 const u32 noShaves = last_shave-first_shave+1;
	 swcShaveUnit_t svuList[noShaves];
	 u64 shavesEnableMask = 0;
	 for(u32 i=0; i<noShaves; i++)
	 {
		 svuList[i] = first_shave+i;
		 shavesEnableMask |= 1<<(first_shave+i);
	 }
	 
	 status = OsDrvSvuOpenShaves(svuList, noShaves, OS_MYR_PROTECTION_SEM);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 status = OsDrvCprTurnOnShaveMask(shavesEnableMask);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 
	 for (int i = first_shave; i <= last_shave; i++)
	 {
		 DrvShaveL2CacheSetLSUPartId(i, 0);
		 DrvShaveL2CacheSetInstrPartId(i, 1);
	 }
	 
	 OsDrvShaveL2CachePartitionInvalidate(0);
	 OsDrvShaveL2CachePartitionInvalidate(1);
	 
	 status = OsDrvSvuSetupDynShaveApps(&MODULE_DATA(ImgResize),svuList,noShaves);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 int shaveused;
	 int pixel_size = RGB_PIXEL_SZ;
	 status = OsDrvSvuRunShaveAlgoCC(&MODULE_DATA(ImgResize),&shaveused,"iiiiiii",in,out, &src_w,&src_h,&dst_w,&dst_h,&pixel_size);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 status = OsDrvSvuDynWaitShaves(svuList, noShaves, OS_DRV_SVU_WAIT_FOREVER, &running);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 status = OsDrvSvuCleanupDynShaveApps(&MODULE_DATA(ImgResize));
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 status = OsDrvSvuCloseShaves(svuList, noShaves);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
	 
	 DrvLL2CFlushOpOnAddrRange(LL2C_OPERATION_INVALIDATE, 0,
		   (u32)out,
		   (u32)out +
		   dst_w * dst_h * RGB_PIXEL_SZ);
	 status = OsDrvCprTurnOffShaveMask(shavesEnableMask);
	 if (status != OS_MYR_DYN_INFR_SUCCESS)
		 OSDRV_DYN_INFR_CHECK_CODE(status);
 }

 static void ConvertYUV2RGB(unsigned char *yuvFrame, unsigned char *rgbFrame, int width, int height)
        {
            int uIndex = width * height;
            int vIndex = uIndex + ((width * height) >> 2);
 //           int gIndex = width * height;
 //           int rIndex = gIndex * 2;  //Modify for BGR format
//            int bIndex = gIndex * 2;

            int temp = 0;

            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    // R
                    temp = (int)(yuvFrame[y * width + x] + (yuvFrame[vIndex + (y / 2) * (width / 2) + x / 2] - 128) * YUV2RGB_CONVERT_MATRIX[0][2]);
                    rgbFrame[3*(y * width + x) + 2] = (unsigned char)(temp < 0 ? 0 : (temp > 255 ? 255 : temp));

                    // G
                    temp = (int)(yuvFrame[y * width + x] + (yuvFrame[uIndex + (y / 2) * (width / 2) + x / 2] - 128) * YUV2RGB_CONVERT_MATRIX[1][1] + (yuvFrame[vIndex + (y / 2) * (width / 2) + x / 2] - 128) * YUV2RGB_CONVERT_MATRIX[1][2]);
                    rgbFrame[3*(y * width + x) + 1] = (unsigned char)(temp < 0 ? 0 : (temp > 255 ? 255 : temp));

                    // B
                    temp = (int)(yuvFrame[y * width + x] + (yuvFrame[uIndex + (y / 2) * (width / 2) + x / 2] - 128) * YUV2RGB_CONVERT_MATRIX[2][1]);
                    rgbFrame[3*(y * width + x)+ 0] = (unsigned char)(temp < 0 ? 0 : (temp > 255 ? 255 : temp));
                }
            }
        }

static void RunShvConvertYUV2RGB(unsigned char *yuvFrame, unsigned char *rgbFrame, int width, int height)
{
    int first_shave = 0;
    int last_shave = 0;
    int status;

    u32 running;
	
    const u32 noShaves = last_shave-first_shave+1;
    swcShaveUnit_t svuList[noShaves];
    u64 shavesEnableMask = 0;
    for(u32 i=0; i<noShaves; i++)
    {
        svuList[i] = first_shave+i;
        shavesEnableMask |= 1<<(first_shave+i);
    }

    status = OsDrvSvuOpenShaves(svuList, noShaves, OS_MYR_PROTECTION_SEM);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);
    status = OsDrvCprTurnOnShaveMask(shavesEnableMask);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);

    for (int i = first_shave; i <= last_shave; i++)
    {
        DrvShaveL2CacheSetLSUPartId(i, 0);
        DrvShaveL2CacheSetInstrPartId(i, 1);
    }

    OsDrvShaveL2CachePartitionInvalidate(0);
    OsDrvShaveL2CachePartitionInvalidate(1);

    status = OsDrvSvuSetupDynShaveApps(&MODULE_DATA(YUV2RGB),svuList,noShaves);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);
    int shaveused;
    status = OsDrvSvuRunShaveAlgoCC(&MODULE_DATA(YUV2RGB),&shaveused,"iiii",yuvFrame,rgbFrame, &width,&height);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);
    status = OsDrvSvuDynWaitShaves(svuList, noShaves, OS_DRV_SVU_WAIT_FOREVER, &running);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);
    status = OsDrvSvuCleanupDynShaveApps(&MODULE_DATA(YUV2RGB));
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);
    status = OsDrvSvuCloseShaves(svuList, noShaves);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);

    DrvLL2CFlushOpOnAddrRange(LL2C_OPERATION_INVALIDATE, 0,
          (u32)rgbFrame,
          (u32)rgbFrame +
          width * height * RGB_PIXEL_SZ);
    status = OsDrvCprTurnOffShaveMask(shavesEnableMask);
    if (status != OS_MYR_DYN_INFR_SUCCESS)
        OSDRV_DYN_INFR_CHECK_CODE(status);
}




MTCNN mtcnn("");


//extern u8 DDR_DATA InputTensor[];

u8 DDR_DATA inImage[1280*960] __attribute__((aligned(64)));



float factor = 0.500f;
float threshold[3] = { 0.9f, 0.7f, 0.6f };

//float threshold[3] = { 0.5f, 0.5f, 0.3f };
//float factor = 0.709f;
//float threshold[3] = { 0.7f, 0.6f, 0.6f };

int minSize =200;
double t = (double)cv::getTickCount();

vector<FaceInfo> faceInfo;

float thrh[3] = { 0.9f, 0.7f, 0.6f };





u8 testbuf[1280*960*3];


//char testbuf[1920*1080*3];

int tmp_cnt = 0;


extern "C"
void Classify_Test(unsigned char *blob, FrameT *frame)
{
	
	
	printf("\nClassify_Test\n");

    u32 FathomBlobSizeBytes = *(u32*)&blob[BLOB_FILE_SIZE_OFFSET];
    u8* timings = NULL;
    u8* debugBuffer = NULL;
    s32 j;
    s32 i;
	

    ClassifyResult top[TOP_RESULT+1];
	
	

    int src_h = frame->height[0];
//  int src_w = frame->tSize[0]/src_h;
    int src_w = frame->stride[0];

    printf("orignal data ---src_w %d, src_h %d\n", src_w, src_h);


	
	
////////////////////////////////////////
	
    unsigned char *yuv_buf = (unsigned char *)(frame->fbPtr[0]);

    OsDrvTimerStartTicksCount(&timer_data_local_det);	
	RunShvConvertYUV2RGB(yuv_buf, rgb_buf, src_w, src_h);
	OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
	OsDrvCprGetSysClockPerUs(&clocksPerUs);
	usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
	printf("RunShvConvertYUV2RGB processing cost %2f ms\n",usedMs);  

	Mat mat  =  Mat(src_h, src_w, CV_8UC3 , rgb_buf);
	Mat resized;
	OsDrvTimerStartTicksCount(&timer_data_local_det);	
	cv::resize(mat, resized, cv::Size(960, 1280), 0, 0, cv::INTER_LINEAR);
	OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
	OsDrvCprGetSysClockPerUs(&clocksPerUs);
	usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
	printf("Classify_Test cv::resize processing cost %2f ms\n",usedMs); 

	for(int j=0; j<960*1280*3; j++){
		rgb_buf[j] = (resized.data)[j];
	}

/////////////////////////////////////////


    //loadMemFromFileSimple("1_yuv420p.yuv",3*1280*960*sizeof(u8), inImage);
	//unsigned char *yuv_buf =  inImage;
///////////////////////////////////////////////////////



    char input_file[] = "1.rgbjpg";
    //loadMemFromFile(input_file	, 0, 0, 3*1280*960, rgb_buf);

    printf("\n\n-----mw---------testbuf--1000-----------------\n\n\n");
    for(int m =0; m<100; m++){
		//printf("%d,", testbuf[m]);
		
	}

    //RunShvConvertYUV2RGB(testbuf, rgb_buf, 960, 1280); 

	//rgb_buf=testbuf;

/////////////////////////////////////////////////
	
	
    //ConvertYUV2RGB(yuv_buf, rgb_buf, 960, 1280);
    //RunShvConvertYUV2RGB(yuv_buf, rgb_buf, 960, 1280);  
	//rgb_buf  orignal data   it is continuous 4 bytes, then will be converted to float
	//rrrrggggbbbb
	

    printf("\n\n-----mw---------rgb_buf--1000-----------------\n\n\n");

    
	for(int m =0; m<100; m++){
		printf("%d,", rgb_buf[m]);
		
	}


	printf("\n\n-----mw---------inImage-------------------\n\n\n");

	
    int resize;
	
	
    //resize = ImageResize(rgb_buf, rgb_buf_resized, src_w, src_h, GoogleNet_Dim, GoogleNet_Dim);
	
	//resize = ImageResize(rgb_buf, rgb_buf_resized, 320, 320, GoogleNet_Dim, GoogleNet_Dim);
	
	
	// resize height width to 700*700
	
//    RunShvImageResize(rgb_buf, rgb_buf_resized, src_w, src_h, GoogleNet_Dim, GoogleNet_Dim);
#if 0
    for(i = 0; i < GoogleNet_Dim * GoogleNet_Dim * 3; i++)
        rgb_buf_fp32[i] = rgb_buf_resized[i];
    for(i = 0; i < GoogleNet_Dim * GoogleNet_Dim; i++)
    	{
    	     rgb_buf_fp32[3*i + 0] = rgb_buf_fp32[3*i + 0] - networkMean[0];  //B
             rgb_buf_fp32[3*i + 1] = rgb_buf_fp32[3*i + 1] - networkMean[1];  //G
             rgb_buf_fp32[3*i + 2] = rgb_buf_fp32[3*i + 2] - networkMean[2];  //R
    	}
    //ConvFP32toFP16(rgb_buf_fp32, rgb_buf_fp16, resize);
    for(i = 0; i < GoogleNet_Dim * GoogleNet_Dim * 3; i++)
        rgb_buf_fp16[i] = f32Tof16(rgb_buf_fp32[i]);
#endif
    //ConvU8toFP16(rgb_buf_resized, rgb_buf_fp16, resize);    
//***********For debug********************//
#if 0
    tmp_cnt ++;
    if (tmp_cnt == 20)
    	{
		int i_, j_;
		int id = 0;
		for (i_ = 0; i_ < 10; i_++)
		{
			for (j_ = 0; j_<5; j_++)
			{
				printf("%f ", (fp32)rgb_buf_resized[id]);
				id++;
			}
			printf("\n");
		}
		printf("~~~~~~~~~~~~~~~~\n");
		id = 0;
		for (i_ = 0; i_ < 10; i_++)
		{
			for (j_ = 0; j_<5; j_++)
			{
				printf("%f ", f16Tof32(rgb_buf_fp16[id]));
				id++;
			}
			printf("\n");
              	}
    	     saveMemoryToFile((u32)yuv_buf, 1.5*src_w*src_h, "raw.yuv");
    	     saveMemoryToFile((u32)rgb_buf, 3*src_w*src_h, "raw.rgb");
             saveMemoryToFile((u32)rgb_buf_resized, 3*GoogleNet_Dim*GoogleNet_Dim, "resized.rgb");

             while(1);
	}
#endif
	
	
	cout<<"--------2"<<endl;
//***********For debug********************//
    fp32 fp32TmpResult;

    FathomRunConfig config =
    {
        .fathomBSS = fathomBSS,
        .fathomBSS_size = 1000,
        .dmaLinkAgent = 0,
        .dataPartitionNo = 0,
        .instrPartitionNo = 1,
        .firstShave = 0,
        .lastShave = MVTENSOR_MAX_SHAVES - 1,
        .dmaTransactions = &task[0]
    };
    bzero(fathomBSS, sizeof(fathomBSS));

    for(j=1; j<=TOP_RESULT; j++) {
        top[j].topK=0;
        top[j].topIdx=0;
    }

	cout<<"-----------rgb_buf_resized------3"<<endl;
	
	
	
	
	for(int i =0; i< 30;i++){
	 // printf("%u,",rgb_buf_resized[i]);
		
	}
	
	
	//mtcnn.detect( rgb_buf ,rgb_buf ,GoogleNet_Dim ,GoogleNet_Dim );



	//char input_file[] = "1920_1080.rgb";
    //loadMemFromFile(input_file	, 0, 0, 1920*1080*3, testbuf);


    int x1=1, x2=1, y1=1, y2=1;
	Mat image  =  Mat(1280, 960, CV_8UC3 , rgb_buf);
	//Mat image =  Mat(src_w, src_h, 21, testbuf);


	OsDrvTimerStartTicksCount(&timer_data_local_det);
	faceInfo = mtcnn.Detect(image, minSize, thrh, factor, 3, x1, y1, x2, y2);
	OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
	OsDrvCprGetSysClockPerUs(&clocksPerUs);
	usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
	printf("mtcnn.Detect processing cost %2f ms\n",usedMs); 	
	

	printf("\n---mw------------Detect  completed------faceInfo.size() =%d--------\n", faceInfo.size());
	


	for (int i = 0; i < faceInfo.size(); i++)
	{
		int x = (int)faceInfo[i].bbox.xmin;
		int y = (int)faceInfo[i].bbox.ymin;
		int w = (int)(faceInfo[i].bbox.xmax - faceInfo[i].bbox.xmin + 1);
		int h = (int)(faceInfo[i].bbox.ymax - faceInfo[i].bbox.ymin + 1);
		printf("\n%d,%d,%d,%d\n",x, y, w, h);

		cv::rectangle(image, cv::Rect(x, y, w, h), cv::Scalar(255, 0, 0), 2);
		//printf("\n%d,%d,%d,%d\n",x, y, w, h);

		cv::rectangle(image, cv::Rect(x1, y1, x2-x1, y2-y1), cv::Scalar(0, 0, 255), 2);
	}


	//Mat yuvImg;
    //cvtColor(image, yuvImg, CV_RGB2YUV_I420);
    //yuv_buf = yuvImg.data;



	Mat resized1;
	cv::resize(image, resized1, cv::Size(2104, 1560), 0, 0, cv::INTER_LINEAR);


	OsDrvTimerStartTicksCount(&timer_data_local_det);
    ConvertWH(resized1.data, yuv_buf, 2104, 1560);
    OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
	OsDrvCprGetSysClockPerUs(&clocksPerUs);
	usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
	printf("ConvertWH processing cost %2f ms\n",usedMs); 

	
	
	
	
	/*

	
	
	Mat img =  Mat(src_w, src_h, CV_16FC3, rgb_buf_fp16);
	

    MTCNN mtcnn();
	vector<Rect> rectangles;
        vector<float> confidences;
        std::vector<std::vector<cv::Point>> alignment;
        mtcnn.detection(img, rectangles, confidences, alignment);



        // output
        for(int i = 0; i < rectangles.size(); i++)
        {
            int green = confidences[i] * 255;
            int red = (1 - confidences[i]) * 255;
            rectangle(img, rectangles[i], cv::Scalar(0, green, red), 3);
            for(int j = 0; j < alignment[i].size(); j++)
            {
                cv::circle(img, alignment[i][j], 5, cv::Scalar(255, 255, 0), 3);
            }
        }

        frame_count++;
        cv::putText(img, std::to_string(frame_count), cvPoint(3, 13), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(0, 255, 0), 1, CV_AA);
        //writer.write(img);
        //imshow("Live", img);
	    //output this img to USB or HDMI
	
	
	
	*/
	

//    FathomRun(blob, (u32)FathomBlobSizeBytes, (rgb_buf_fp16), fathomOutput, &config, timings, debugBuffer,
//		cache_memory_size, scratch_memory_size, cache_memory, scratch_memory, 1, 1);
	
	
	
	

	cout<<"---end Classify_Test---4"<<endl;

/*
	
    u16* probabilities = (u16*)fathomOutput;




    printf("\nminwu Classification probabilities:\n");
    //Google Net output 1000 classification
#if 1
    for (i = 0; i < 1000; i++) {
        fp32TmpResult = f16Tof32(probabilities[i]);
		//printf("%d  %f \n ",i,   fp32TmpResult);
        if(fp32TmpResult > top[1].topK) {
            top[1].topK= fp32TmpResult;
            top[1].topIdx = i;
            UptoDown(1,top);
        }
    }
    for (i = 1; i <= TOP_RESULT; i++)
        printf("%d: %.6f\n", top[i].topIdx, top[i].topK);
#endif



	
#if 0
    float maxResult = 0.0;
    int maxIndex = -1;
    for (i = 0; i < 1000; i++) {
        fp32TmpResult = f16Tof32(probabilities[i]);
        if(fp32TmpResult > maxResult)
        	{
        	    maxResult = fp32TmpResult;
             maxIndex = i;
        	}
    	}
    printf("%d: %.6f\n", maxIndex, maxResult);
#endif
	
*/


	
}

