#include <OsDrvSvu.h>
#include <OsDrvShaveL2Cache.h>
#include <swcShaveLoaderLocal.h>
#include <OsDrvCpr.h>
#include <DrvLeonL2C.h>
#include <DrvShaveL2Cache.h>
#include <iostream>
#include <vector>

#include <OsDrvTimer.h>



#include "mtcnn.h"



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

#define GoogleNet_Dim  224   //300  
const unsigned int cache_memory_size = 25 * 1024 * 1024;
const unsigned int scratch_memory_size = 110 * 1024;

char DDR_BSS cache_memory[cache_memory_size];

#define MAX_RESOLUTION (2104*1560)

#define MAXI_PIXEL_SIZE (4)

#define RGB_PIXEL_SZ (3)

//unsigned char DDR_BSS rgb_buf[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

unsigned char * rgb_buf;

unsigned char DDR_BSS rgb_buf_resized[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

float DDR_BSS rgb_buf_fp32[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

//fp16 DDR_BSS rgb_buf_fp16[MAX_RESOLUTION*MAXI_PIXEL_SIZE*2];

fp16 DDR_BSS rgb_buf_fp16[100*100*3];


float __attribute__((section(".cmx.data"))) networkMean[] = {0.40787054*255.0, 0.45752458*255.0, 0.48109378*255.0};

char __attribute__((section(".cmx.bss"))) scratch_memory[scratch_memory_size];
u8* timings = NULL;
u8* debugBuffer = NULL;





dmaTransactionList_t DMA_DESCRIPTORS_SECTION task[1];

u8 DDR_BSS fathomBSS[F_BSS_SIZE] __attribute__((aligned(64)));
float DDR_DATA fathomOutput[OUTPUTSIZE] __attribute__((aligned(64)));

static float __attribute__((section(".cmx.data"))) YUV2RGB_CONVERT_MATRIX[3][3] = { { 1, 0, 1.4022 }, 
                                                                                { 1, -0.3456, -0.7145 }, 
                                                                                { 1, 1.771, 0 } 
                                                                                };


tyTimeStamp timer_data_local_det;
u64 cyclesElapsed_local_det;
u32 clocksPerUs;
float usedMs = 0.0;

extern u8 __attribute__((section(".ddr.data"))) PnetBlob[];

extern u8 __attribute__((section(".ddr.data"))) pnetBlob12x12_4[];
extern u8 __attribute__((section(".ddr.data"))) pnetBlob12x12_2[];


// pnet 
extern u8 __attribute__((section(".ddr.data"))) pnetBlob_4_77_58[];
extern u8 __attribute__((section(".ddr.data"))) pnetBlob_2_77_58[];
extern u8 __attribute__((section(".ddr.data"))) pnetBlob_4_39_29[];
extern u8 __attribute__((section(".ddr.data"))) pnetBlob_2_39_29[];
extern u8 __attribute__((section(".ddr.data"))) pnetBlob_4_20_15[];
extern u8 __attribute__((section(".ddr.data"))) pnetBlob_2_20_15[];
// pnet end


extern u8 __attribute__((section(".ddr.data"))) rnetBlob24_4[];
extern u8 __attribute__((section(".ddr.data"))) rnetBlob24_2[];



extern u8 __attribute__((section(".ddr.data"))) onetBlob48_2[];
extern u8 __attribute__((section(".ddr.data"))) onetBlob48_4[];
extern u8 __attribute__((section(".ddr.data"))) onetBlob48_10[];


extern u8 __attribute__((section(".ddr.data"))) RnetBlob[];

extern u8 __attribute__((section(".ddr.data"))) OnetBlob[];

extern u8 DDR_DATA InputTensor[];


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



#define ISLOG false

extern "C"
 //convert to FP16
void  ConvU8toFP16(unsigned char *in, fp16 *out, int img_size)
 {
	 for(int i =0; i<img_size; i++){
		 out[i] = f32Tof16(((fp32)in[i]) - networkMean[i%3]);
	 }
 }



extern "C"
void  ConvU8toFP16WithMean(unsigned char *in, fp16 *out, int img_size)
 {
	 for(int i =0; i<img_size; i++){
		 out[i] = f32Tof16(float(  (  (fp32)in[i]  - mean_val)* std_val));
	 }
 }



 extern "C"
 //convert to FP16
void  ConvFP32toFP16(fp32 *in, fp16 *out, int img_size)
 {
	 for(int i =0; i<img_size; i++){
		 out[i] = f32Tof16(  in[i] );
	 }
 }





bool CompareBBox(const FaceInfo & a, const FaceInfo & b) {
	return a.bbox.score > b.bbox.score;
}
float MTCNN::IoU(float xmin, float ymin, float xmax, float ymax,
	float xmin_, float ymin_, float xmax_, float ymax_, bool is_iom) {
	float iw = std::min(xmax, xmax_) - std::max(xmin, xmin_) + 1;
	float ih = std::min(ymax, ymax_) - std::max(ymin, ymin_) + 1;
	if (iw <= 0 || ih <= 0)
		return 0;
	float s = iw*ih;
	if (is_iom) {
		float ov = s / min((xmax - xmin + 1)*(ymax - ymin + 1), (xmax_ - xmin_ + 1)*(ymax_ - ymin_ + 1));
		return ov;
	}
	else {
		float ov = s / ((xmax - xmin + 1)*(ymax - ymin + 1) + (xmax_ - xmin_ + 1)*(ymax_ - ymin_ + 1) - s);
		return ov;
	}
}
std::vector<FaceInfo> MTCNN::NMS(std::vector<FaceInfo>& bboxes,
	float thresh, char methodType) {
	std::vector<FaceInfo> bboxes_nms;
	if (bboxes.size() == 0) {
		return bboxes_nms;
	}
	std::sort(bboxes.begin(), bboxes.end(), CompareBBox);

	int32_t select_idx = 0;
	int32_t num_bbox = static_cast<int32_t>(bboxes.size());
	std::vector<int32_t> mask_merged(num_bbox, 0);
	bool all_merged = false;

	while (!all_merged) {
		while (select_idx < num_bbox && mask_merged[select_idx] == 1)
			select_idx++;
		if (select_idx == num_bbox) {
			all_merged = true;
			continue;
		}

		bboxes_nms.push_back(bboxes[select_idx]);
		mask_merged[select_idx] = 1;

		FaceBox select_bbox = bboxes[select_idx].bbox;
		float area1 = static_cast<float>((select_bbox.xmax - select_bbox.xmin + 1) * (select_bbox.ymax - select_bbox.ymin + 1));
		float x1 = static_cast<float>(select_bbox.xmin);
		float y1 = static_cast<float>(select_bbox.ymin);
		float x2 = static_cast<float>(select_bbox.xmax);
		float y2 = static_cast<float>(select_bbox.ymax);

		select_idx++;
//#pragma omp parallel for num_threads(threads_num)
		for (int32_t i = select_idx; i < num_bbox; i++) {
			if (mask_merged[i] == 1)
				continue;

			FaceBox & bbox_i = bboxes[i].bbox;
			float x = std::max<float>(x1, static_cast<float>(bbox_i.xmin));
			float y = std::max<float>(y1, static_cast<float>(bbox_i.ymin));
			float w = std::min<float>(x2, static_cast<float>(bbox_i.xmax)) - x + 1;
			float h = std::min<float>(y2, static_cast<float>(bbox_i.ymax)) - y + 1;
			if (w <= 0 || h <= 0)
				continue;

			float area2 = static_cast<float>((bbox_i.xmax - bbox_i.xmin + 1) * (bbox_i.ymax - bbox_i.ymin + 1));
			float area_intersect = w * h;

			switch (methodType) {
			case 'u':
				if (static_cast<float>(area_intersect) / (area1 + area2 - area_intersect) > thresh)
					mask_merged[i] = 1;
				break;
			case 'm':
				if (static_cast<float>(area_intersect) / std::min(area1, area2) > thresh)
					mask_merged[i] = 1;
				break;
			default:
				break;
			}
		}
	}
	return bboxes_nms;
}
void MTCNN::BBoxRegression(vector<FaceInfo>& bboxes) {
//#pragma omp parallel for num_threads(threads_num)
	//Ô­À´Ïà¶ÔÓÚÁ½µãµÄÆ«ÒÆ
	for (int i = 0; i <(int)bboxes.size(); ++i) {
		FaceBox &bbox = bboxes[i].bbox;
		float *bbox_reg = bboxes[i].bbox_reg;
		float w = bbox.xmax - bbox.xmin + 1;
		float h = bbox.ymax - bbox.ymin + 1;

		if(ISLOG)printf("\n---BBoxRegression---w,  h----%f,  %f-----\n",w,h);
		if(ISLOG)printf("\n---bbox_reg[0]---%f---%f---%f----%f-----\n",bbox_reg[0],bbox_reg[1],bbox_reg[2],bbox_reg[3]);

		bbox.xmin += bbox_reg[0] * w;
		bbox.ymin += bbox_reg[1] * h;
		bbox.xmax += bbox_reg[2] * w;
		bbox.ymax += bbox_reg[3] * h;
	}
	
	/*
	//Ïà¶ÔÓÚ×îÐ¡µãÆ«ÒÆ
	for (int i = 0; i < bboxes.size(); ++i) {
		FaceBox &bbox = bboxes[i].bbox;
		float *bbox_reg = bboxes[i].bbox_reg;
		float w = bbox.xmax - bbox.xmin + 1;
		float h = bbox.ymax - bbox.ymin + 1;
		float x1 = bbox.xmin;
		float y1 = bbox.ymin;
		bbox.xmin = x1 + bbox_reg[0] * w;
		bbox.ymin = y1 + bbox_reg[1] * h;
		bbox.xmax = x1 + bbox_reg[2] * w;
		bbox.ymax = y1+ bbox_reg[3] * h;
	}
	*/
}

void MTCNN::BBoxPad(vector<FaceInfo>& bboxes, int width, int height) {
//#pragma omp parallel for num_threads(threads_num)
	for (int i = 0; i < (int)bboxes.size(); ++i) {
		FaceBox &bbox = bboxes[i].bbox;
		bbox.xmin = round(max(bbox.xmin, 0.f));
		bbox.ymin = round(max(bbox.ymin, 0.f));
		bbox.xmax = round(min(bbox.xmax, width - 1.f));
		bbox.ymax = round(min(bbox.ymax, height - 1.f));
	}
}
void MTCNN::BBoxPadSquare(vector<FaceInfo>& bboxes, int width, int height) {
//#pragma omp parallel for num_threads(threads_num)
	for (int i = 0; i < (int)bboxes.size(); ++i) {
		FaceBox &bbox = bboxes[i].bbox;
		float w = bbox.xmax - bbox.xmin + 1;
		float h = bbox.ymax - bbox.ymin + 1;
		float side = h>w ? h : w;
		bbox.xmin = round(max(bbox.xmin + (w - side)*0.5f, 0.f));

		bbox.ymin = round(max(bbox.ymin + (h - side)*0.5f, 0.f));
		bbox.xmax = round(min(bbox.xmin + side - 1, width - 1.f));
		bbox.ymax = round(min(bbox.ymin + side - 1, height - 1.f));
	}
}



void MTCNN::GenerateBBox(shared_ptr<float> confidence, shared_ptr<float> reg_box , int feature_map_h_, int feature_map_w_,
	float scale, float thresh) {

    cout<<" --------------mw-------------------GenerateBBox----------"<<endl;
	int spatical_size = feature_map_h_*feature_map_w_;
	float* confidence_data = confidence.get();
	float* reg_data = reg_box.get();
	candidate_boxes_.clear();
	for (int i = 0; i<spatical_size; i++) {

		//printf("\nconfidence_data[%d] = %f\n",i , confidence_data[i]);

		if (confidence_data[i] >= thresh) {

			int y = i / feature_map_w_;
			int x = i - feature_map_w_ * y;
			FaceInfo faceInfo;
			FaceBox &faceBox = faceInfo.bbox;

			faceBox.xmin = (float)(x * pnet_stride) / scale;
			faceBox.ymin = (float)(y * pnet_stride) / scale;
			faceBox.xmax = (float)(x * pnet_stride + pnet_cell_size - 1.f) / scale;
			faceBox.ymax = (float)(y * pnet_stride + pnet_cell_size - 1.f) / scale;

			faceInfo.bbox_reg[0] = reg_data[i*4];
			faceInfo.bbox_reg[1] = reg_data[i*4 + 1];
			faceInfo.bbox_reg[2] = reg_data[i*4 + 2 ];
			faceInfo.bbox_reg[3] = reg_data[i*4 + 3 ];

			//printf("\n\ni=%d  confidence_data =%f   %f, %f, %f, %f\n", i,confidence_data[i], faceInfo.bbox_reg[0], faceInfo.bbox_reg[1],faceInfo.bbox_reg[2],faceInfo.bbox_reg[3]);
		

			//std::cout << faceBox.xmin << endl << faceBox.ymin << endl << endl;
			//std::cout << faceInfo.bbox_reg[0] << endl << faceInfo.bbox_reg[1] << endl << faceInfo.bbox_reg[2] << endl;

			faceBox.score = confidence_data[i];

			candidate_boxes_.push_back(faceInfo);
		}
	}
}


MTCNN::MTCNN(const string& proto_model_dir) {
	printf("\n-------MTCNN OK----------\n");

	
	
}
vector<FaceInfo> MTCNN::ProposalNet(const cv::Mat& img, int minSize, float threshold, float factor) {
	cv::Mat  resized;
	int width = img.cols;
	int height = img.rows;
	float scale = 12.f / minSize;
	//float scale1 = 12.f / 400;

	float minWH = std::min(height, width) *scale;
	std::vector<float> scales;
	//scales.push_back(scale);
	
	while (minWH >= 12) {
		scales.push_back(scale);
		minWH *= factor;
		scale *= factor;
	}
	
	total_boxes_.clear();


	//std::cout << scales.size() << endl;

	for (int i = 0; i < scales.size(); i++) {     // 0, 77    1, 39    2, 20


		int ws = (int)std::ceil(width*scales[i]);
		int hs = (int)std::ceil(height*scales[i]);


		//std::cout << i << " input: " << ws << "\t" << hs << endl;

		double t, t1;

	    OsDrvTimerStartTicksCount(&timer_data_local_det);	
		cv::resize(img, resized, cv::Size(ws, hs), 0, 0, cv::INTER_LINEAR);
		
        OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
        OsDrvCprGetSysClockPerUs(&clocksPerUs);
        usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
        printf("pnet resize processing cost %2f ms\n",usedMs);   


		//resized.convertTo(resized, CV_32FC3,  0.0078125, -127.5*0.0078125);

		printf("\n\n-----mw----i=%d-----resized--100-----------------\n\n\n",i);

		float *  da = (float*)(resized.data);
		for(int m =0; m<100; m++){
			//printf("%f,",   *(da+m )    );
			
		}



		float* confidence_begin0;
		float * rect_begin0 ;
		int pipeline =  0  ; // is how many 12*12
		int feature_map_h_ ,  feature_map_w_;

	    u32 FathomBlobSizeBytes_4 ;
	    u8*  FathomBlob_4;

	    u32 FathomBlobSizeBytes_2 ;
	    u8*  FathomBlob_2;


		//u32 FathomBlobSizeBytes = *(u32*)&pnetBlob_4_77_58[BLOB_FILE_SIZE_OFFSET];
	    //float DDR_DATA fathomOutput[OUTPUTSIZE] __attribute__((aligned(64)));
	    //InputTensor    rgb_buf_fp16



	    OsDrvTimerStartTicksCount(&timer_data_local_det);	

	    ConvU8toFP16WithMean(resized.data, rgb_buf_fp16, ws*hs*3);

	    OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
        OsDrvCprGetSysClockPerUs(&clocksPerUs);
        usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
        printf("ConvU8toFP16WithMean processing cost %2f ms\n",usedMs);   


	    printf("\n-----------ProposalNet----i = %d---------\n",i);


		if(i==0){   //  77  58
			pipeline =  34*24  ; // is how many 12*12
			feature_map_h_ =34;
			feature_map_w_= 24;
			FathomBlobSizeBytes_4 = *(u32*)&pnetBlob_4_77_58[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_4 = pnetBlob_4_77_58;
   
       		FathomBlobSizeBytes_2 = *(u32*)&pnetBlob_2_77_58[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_2 = pnetBlob_2_77_58;

		}


		if(i==1){
			pipeline =  15*10  ; // is how many 12*12
			feature_map_h_ =15;
			feature_map_w_= 10;
			FathomBlobSizeBytes_4 = *(u32*)&pnetBlob_4_39_29[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_4 = pnetBlob_4_39_29;
   
       		FathomBlobSizeBytes_2 = *(u32*)&pnetBlob_2_39_29[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_2 = pnetBlob_2_39_29;

		}


		if(i==2){
			pipeline =  5*3  ; // is how many 12*12
			feature_map_h_ =5;
			feature_map_w_= 3;
			FathomBlobSizeBytes_4 = *(u32*)&pnetBlob_4_20_15[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_4 = pnetBlob_4_20_15;
   
       		FathomBlobSizeBytes_2 = *(u32*)&pnetBlob_2_20_15[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_2 = pnetBlob_2_20_15;

		}

		OsDrvTimerStartTicksCount(&timer_data_local_det);	
		

		

	    FathomRun(FathomBlob_4, (u32)FathomBlobSizeBytes_4, rgb_buf_fp16, fathomOutput, &config, timings, debugBuffer,
	        cache_memory_size, scratch_memory_size, cache_memory, scratch_memory, 0, 1);
	    u16* value = (u16*)fathomOutput;

	    //pipeline =  34*24  ; // is how many 12*12

		printf("\n------------------rect_begin0---------------------\n");
        rect_begin0 = (float *)malloc(4*pipeline * sizeof(float));
        for(int i=0, k=0; i<pipeline*4; i=i+4, k=k+4){
			
			*(rect_begin0+k)  =  f16Tof32( *(value+i));
			*(rect_begin0+k+1)  = f16Tof32( *(value+i+1));
			*(rect_begin0+k+2)  =  f16Tof32(*(value+i+2));
			*(rect_begin0+k+3)  = f16Tof32( *(value+i+3));

			//printf("i=%d  %f, %f, %f, %f\n",i/4,  *(rect_begin0+k), *(rect_begin0+k+1),*(rect_begin0+k+2),*(rect_begin0+k+3));
			
		}

		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
        OsDrvCprGetSysClockPerUs(&clocksPerUs);
        usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
        printf("PNET 1 time processing cost %2f ms\n",usedMs);   

	    
        OsDrvTimerStartTicksCount(&timer_data_local_det);	

        FathomRun(FathomBlob_2, (u32)FathomBlobSizeBytes_2, rgb_buf_fp16, fathomOutput, &config, timings, debugBuffer,
		   cache_memory_size, scratch_memory_size, cache_memory, scratch_memory, 0, 1);

        value = (u16*)fathomOutput;
		confidence_begin0= (float *)malloc(1*pipeline * sizeof(float));


		for(int i=0, k=0; i<pipeline*2; i=i+2, k=k+1){
		
			*(confidence_begin0+k)  = f16Tof32( *(value+i+1));
			//printf("%f\n",  *(confidence_begin0+k)   );
		}


		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
        OsDrvCprGetSysClockPerUs(&clocksPerUs);
        usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
        printf("pnet FathomBlob_2 processing cost %2f ms\n",usedMs);   


		

		std::shared_ptr<float> cc(confidence_begin0);
		std::shared_ptr<float> rr(rect_begin0);


		OsDrvTimerStartTicksCount(&timer_data_local_det);	


		GenerateBBox(cc, rr, feature_map_h_, feature_map_w_, scales[i], threshold);


		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
		OsDrvCprGetSysClockPerUs(&clocksPerUs);
		usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
		printf("GenerateBBox processing cost %2f ms\n",usedMs);   


		OsDrvTimerStartTicksCount(&timer_data_local_det);	
		std::vector<FaceInfo> bboxes_nms = NMS(candidate_boxes_, 0.5, 'u');
		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
		OsDrvCprGetSysClockPerUs(&clocksPerUs);
		usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
		printf("***NMS processing cost %2f ms\n",usedMs); 

		if (bboxes_nms.size()>0) {
			total_boxes_.insert(total_boxes_.end(), bboxes_nms.begin(), bboxes_nms.end());
		}


	}




	int num_box = (int)total_boxes_.size();
	if(ISLOG)printf("\n---mw------------total_boxes_----FaceBox--num_box=%d---------\n",num_box);

	for(int j=0 ; j<num_box; j++){

		FaceInfo faceInfo = total_boxes_[j];
		FaceBox faceBox = faceInfo.bbox;

		if(ISLOG)printf("\n---mw---total_boxes_----FaceBox---------==--- %d---%d-----%d---%d---\n",    (int)faceBox.xmin, (int)faceBox.ymin, (int)faceBox.xmax, (int)faceBox.ymax);


	}

	if(ISLOG)printf("\n---mw-----end-------total_boxes_----FaceBox-----------\n");


	vector<FaceInfo> res_boxes;
	if (num_box != 0) {

		if(ISLOG)printf("\n---mw------start------res_boxes--0--FaceBox-----------\n");
		res_boxes = NMS(total_boxes_, 0.7f, 'u');

		for(int j=0 ; j<res_boxes.size(); j++){

			FaceInfo faceInfo = res_boxes[j];
			FaceBox faceBox = faceInfo.bbox;

			if(ISLOG)printf("\n---mw---res_boxes----FaceBox---------==--- %d---%d-----%d---%d---\n",    (int)faceBox.xmin, (int)faceBox.ymin, (int)faceBox.xmax, (int)faceBox.ymax);


	 	}

	 	if(ISLOG)printf("\n---mw-----end-------res_boxes--0--FaceBox-----------\n\n");

	 	if(ISLOG)printf("\n---mw------start------res_boxes--1--FaceBox-----------\n");

		BBoxRegression(res_boxes);

		for(int j=0 ; j<res_boxes.size(); j++){

			FaceInfo faceInfo = res_boxes[j];
			FaceBox faceBox = faceInfo.bbox;

			if(ISLOG)printf("\n---mw---res_boxes----FaceBox---------==--- %d---%d-----%d---%d---\n",    (int)faceBox.xmin, (int)faceBox.ymin, (int)faceBox.xmax, (int)faceBox.ymax);


	 	}

	 	if(ISLOG)printf("\n---mw------end------res_boxes--1--FaceBox-----------\n\n");

	 	if(ISLOG)printf("\n---mw------start------res_boxes--2--FaceBox-----------\n");

		BBoxPadSquare(res_boxes, width, height);

		for(int j=0 ; j<res_boxes.size(); j++){

			FaceInfo faceInfo = res_boxes[j];
			FaceBox faceBox = faceInfo.bbox;

			if(ISLOG)printf("\n---mw---res_boxes----FaceBox---------==--- %d---%d-----%d---%d---\n",    (int)faceBox.xmin, (int)faceBox.ymin, (int)faceBox.xmax, (int)faceBox.ymax);


	 	}

	 	if(ISLOG)printf("\n---mw------end------res_boxes--2--FaceBox-----------\n\n");

	}
	
	return res_boxes;
}



vector<FaceInfo> MTCNN::NextStage(const cv::Mat& image, vector<FaceInfo> &pre_stage_res, int input_w, int input_h, int stage_num, const float threshold) {
	
	if(ISLOG)printf("\n---mw------------NextStage  stage_num = %d--------------\n",stage_num);
	
	vector<FaceInfo> res;
	int batch_size = (int)pre_stage_res.size();
	if (batch_size == 0)
		return res;
	std::shared_ptr<float> input_layer = nullptr;
	std::shared_ptr<float> confidence = nullptr;
	std::shared_ptr<float> reg_box = nullptr;
	std::shared_ptr<float> reg_landmark = nullptr;


    u32 FathomBlobSizeBytes_4 ;
    u8*  FathomBlob_4;

    u32 FathomBlobSizeBytes_2 ;
    u8*  FathomBlob_2;

   int pipeline =  batch_size  ; // is how many 12*12



	switch (stage_num) {
	case 2: {


       		FathomBlobSizeBytes_4 = *(u32*)&rnetBlob24_4[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_4 = rnetBlob24_4;
   
       		FathomBlobSizeBytes_2 = *(u32*)&rnetBlob24_2[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_2 = rnetBlob24_2;


	}break;
	case 3: {


       		FathomBlobSizeBytes_4 = *(u32*)&onetBlob48_4[BLOB_FILE_SIZE_OFFSET];
       		FathomBlob_4 = onetBlob48_4;
   
       	    FathomBlobSizeBytes_2 = *(u32*)&onetBlob48_2[BLOB_FILE_SIZE_OFFSET];
       	    FathomBlob_2 = onetBlob48_2;

	}break;
	default:
		return res;
		break;
	}

	//float * input_data = input_layer->mutable_cpu_data();
	int spatial_size = input_h*input_w;

	if(ISLOG)printf("\n---mw------------NextStage  input_h = %d-----input_w=%d---------\n", input_h,  input_w);
	

    float * rect_begin0 = (float *)malloc(4*pipeline * sizeof(float));

    float * confidence_begin0= (float *)malloc(2*pipeline * sizeof(float));
	


    //#pragma omp parallel for num_threads(threads_num)
    std::vector<float>  alignment_temp_ ;

    if(ISLOG)printf("\n---mw------------NextStage  batch_size = %d--------------\n", batch_size);
	
	float* landmark_data = nullptr;

	for (int n = 0; n < batch_size; ++n) {


		FaceBox &box = pre_stage_res[n].bbox;

		if(ISLOG)printf("\n---mw--Point((int)box.xmin, (int)box.ymin), Point((int)box.xmax, (int)box.ymax)==--- %d---%d-----%d---%d---\n",  (int)box.xmin, (int)box.ymin, (int)box.xmax, (int)box.ymax);
		
		OsDrvTimerStartTicksCount(&timer_data_local_det);	
		Mat roi = image(Rect(Point((int)box.xmin, (int)box.ymin), Point((int)box.xmax, (int)box.ymax))).clone();
		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
		OsDrvCprGetSysClockPerUs(&clocksPerUs);
		usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
		printf("*****image(Rect(Point( processing cost %2f ms\n",usedMs); 

		if(ISLOG)printf("\n---mw------------NextStage  batch_size = %d--------------\n",batch_size);
		cout<<"roi total = "<<roi.total()<<"\n";
		cout<<"roi size = "<<roi.size()<<"\n";
		cout<<"roi channels = "<<roi.channels()<<"\n";

		resize(roi, roi, Size(input_w, input_h));

		//roi.convertTo(roi, CV_32FC3,  0.0078125, -127.5*0.0078125);

		printf("\n\n-----mw----n=%d-----roi--100-----------------\n\n\n",n);
		float *  da = (float*)(roi.data);
		for(int m =0; m<100; m++){
			//printf("%f,", *( da+m ));
			
		}

/*
		float *input_data_n = input_data + input_layer->offset(n);
		Vec3b *roi_data = (Vec3b *)roi.data;
		CHECK_EQ(roi.isContinuous(), true);
		for (int k = 0; k < spatial_size; ++k) {
			input_data_n[k] = float((roi_data[k][0] - mean_val)*std_val);
			input_data_n[k + spatial_size] = float((roi_data[k][1] - mean_val)*std_val);
			input_data_n[k + 2 * spatial_size] = float((roi_data[k][2] - mean_val)*std_val);
		}
*/

	    if(ISLOG)printf("\n---mw------------NextStage  ConvU8toFP16 --------------\n");
		ConvU8toFP16WithMean(roi.data, rgb_buf_fp16, input_w*input_h*3);


		FathomRun(FathomBlob_4, (u32)FathomBlobSizeBytes_4, rgb_buf_fp16, fathomOutput, &config, timings, debugBuffer,
			   cache_memory_size, scratch_memory_size, cache_memory, scratch_memory, 0, 1);



		
		u16* value = (u16*)fathomOutput;

		printf("\n\nNextStage 11111111\n");

		for(int i=0, k=0; i<pipeline*4; i=i+4, k=k+4){
			
			*(rect_begin0+n*4+k)  =  f16Tof32( *(value+i));
			*(rect_begin0+n*4+k+1)  = f16Tof32( *(value+i+1));
			*(rect_begin0+n*4+k+2)  =  f16Tof32(*(value+i+2));
			*(rect_begin0+n*4+k+3)  = f16Tof32( *(value+i+3));

			//printf("i=%d  %f, %f, %f, %f\n",i/4,  *(rect_begin0+k), *(rect_begin0+k+1),*(rect_begin0+k+2),*(rect_begin0+k+3));
		
			
		}


	    FathomRun(FathomBlob_2, (u32)FathomBlobSizeBytes_2, rgb_buf_fp16, fathomOutput, &config, timings, debugBuffer,
		   cache_memory_size, scratch_memory_size, cache_memory, scratch_memory, 0, 1);

	    value = (u16*)fathomOutput;


		for(int i=0, k=0; i<pipeline*2; i=i+1, k=k+1){
		
			*(confidence_begin0+n*2+k)  = f16Tof32( *(value+i));
			printf(" %f ,  ",  *(confidence_begin0+n*2+k) );
		

		}

		if(ISLOG)printf("\n---mw------------NextStage  stage_num --------------\n");
	


		

	    //landmarks
	    if( stage_num == 3){


	       // Blob<float>* points = net->output_blobs()[1];
	      //  const float* points_begin = points->cpu_data();
	      //  const float* points_end = points_begin + points->channels() * count;

	        landmark_data= (float *)malloc(10 * pipeline * sizeof(float));


	    	printf("\n\n------------start ONET out 10------------------\n\n");

	   		u32 FathomBlobSizeBytes = *(u32*)&onetBlob48_10[BLOB_FILE_SIZE_OFFSET];
	   		u8* FathomBlob = onetBlob48_10;

	   	    FathomRun(FathomBlob, (u32)FathomBlobSizeBytes, rgb_buf_fp16, fathomOutput, &config, timings, debugBuffer,
		        cache_memory_size, scratch_memory_size, cache_memory, scratch_memory, 0, 1);

	        value = (u16*)fathomOutput;

			for(int i=0, k=0; i<10; i=i+1, k=k+1){

				*(landmark_data+n*10+k)  = f16Tof32( *(value+i)  );
			    printf(" %f ,  ",  *(landmark_data+n*10+k) );

			}
			printf("\n--------------------\n");
		
		/*
			std::vector<float>::iterator iterator;
			
			for( iterator=alignment_temp_.begin()
			;   iterator!=alignment_temp_.end()
			;   iterator++){

				printf("%f, ", *iterator);
			}
		*/
	    }
	}



	const float* confidence_data = confidence_begin0;
	const float* reg_data = rect_begin0;
	


	for (int k = 0; k < batch_size; ++k) {
		printf("\n\n  confidence_data[2 * %d + 1] = %f \n", k, confidence_data[2 * k + 1]);
		if (confidence_data[2 * k + 1] >= threshold) {
			FaceInfo info;
			info.bbox.score = confidence_data[2 * k + 1];
			info.bbox.xmin = pre_stage_res[k].bbox.xmin;
			info.bbox.ymin = pre_stage_res[k].bbox.ymin;
			info.bbox.xmax = pre_stage_res[k].bbox.xmax;
			info.bbox.ymax = pre_stage_res[k].bbox.ymax;
			for (int i = 0; i < 4; ++i) {
				info.bbox_reg[i] = reg_data[4 * k + i];
			}
		
			if( stage_num == 3){

				float w = info.bbox.xmax - info.bbox.xmin + 1.f;
				float h = info.bbox.ymax - info.bbox.ymin + 1.f;
				for (int i = 0; i < 5; ++i){
					info.landmark[2 * i] = landmark_data[10 * k + 2 * i] * w + info.bbox.xmin;
					info.landmark[2 * i + 1] = landmark_data[10 * k + 2 * i + 1] * h + info.bbox.ymin;
				}

			}
			
			res.push_back(info);
		}
	}


	return res;


}
vector<FaceInfo> MTCNN::Detect(const cv::Mat& input, const int minSize, const float* threshold, const float factor, const int stage, int a, int b, int c,int d) {
	
	printf("\n---------mwmwmwmwmwmw-------Detect 1--------------\n");

	Mat image ;
	image = input;
	//cv::resize(input, image, cv::Size(960, 1280), 0, 0, cv::INTER_LINEAR);

	cout<<"image total = "<<image.total()<<"\n";
	cout<<"image size = "<<image.size()<<"\n";
	cout<<"image channels = "<<image.channels()<<"\n";

	const u8 *rgb = image.data;
	for(int m =0; m<100; m++){
		printf("%d,", rgb[m]);
		
	}
	
	
	vector<FaceInfo> pnet_res;
	vector<FaceInfo> rnet_res;
	vector<FaceInfo> onet_res;

	double t;
	t = (double)cvGetTickCount();

	if (stage >= 1)
	{
		vector<FaceInfo> pnet_tmp_res;
		
		pnet_tmp_res = ProposalNet(image, minSize, threshold[0], factor);



		printf("\n---------mwmwmwmw-------ProposalNet completed--------------\n");

		//std::cerr << "********** " << pnet_tmp_res.size() << endl;
		
		/*
		//Ö±½Ó´«Èë´«Í³ÈËÁ³¿ò´úÂëÓÐbug
		if (pnet_tmp_res.size())
		{
			pnet_tmp_res[0].bbox.xmin = a;
			pnet_tmp_res[0].bbox.ymin = b;
			pnet_tmp_res[0].bbox.xmax = c - 1;
			pnet_tmp_res[0].bbox.ymax = d - 1;
		}
		*/

		//³öÏÖsize£¨£©>0µÄ bugÖ÷ÒªÔ­ÒòÊÇi>3Ê±£¬¶øpnet_tmp_res.size<3,µ¼ÖÂ´«Èë¿ÕÖ¸Õë
		if(pnet_tmp_res.size()>0){
			for (int i = 0; i <  1 ; ++i)
				pnet_res.push_back(pnet_tmp_res[i]);
		}

	}

	printf("\n---------mwmwmwmwmwmw-------Detect  2--------------\n");

	t = (double)cvGetTickCount() - t;
	//fprintf(stderr, "Pnet ¼ì²âÊ±¼ä £º %g ms\n", t / ((double)cvGetTickFrequency()*1000.));
	fprintf(stderr, "pnet++++++%g,", t / ((double)cvGetTickFrequency()*1000.));

	printf("\n---------mwmwmwmwmwmw-------Detect  3--------------\n");

	t = (double)cvGetTickCount();

	printf("\n---mw------pnet_res.size()=%d--------------\n", pnet_res.size());



	for(int m=0; m< pnet_res.size(); m++){

		FaceInfo faceInfo = pnet_res[m];
		FaceBox faceBox = faceInfo.bbox;

		if(ISLOG)printf("\n---mw-----FaceInfo faceBox = pnet_res[m]-----==--- %d---%d-----%d---%d---\n",    (int)faceBox.xmin, (int)faceBox.ymin, (int)faceBox.xmax, (int)faceBox.ymax);

	}



	if (stage >= 2 && pnet_res.size()>0)
	{
		if (pnet_max_detect_num < (int)pnet_res.size()){
			pnet_res.resize(pnet_max_detect_num);
		}

		printf("\n---mw------------Detect  3000--------------\n");

		int num = (int)pnet_res.size();
		int size = (int)ceil(1.f*num / step_size);

		if(ISLOG)printf("\n---mw------------Detect  size = %d--------------\n",size);

		for (int iter = 0; iter < size; ++iter){
			if(ISLOG)printf("\n---mw------------Detect  iter = %d--------------\n",iter);
			int start = iter*step_size;
			int end = min(start + step_size, num);
			vector<FaceInfo> input(pnet_res.begin() + start, pnet_res.begin() + end);

			
			OsDrvTimerStartTicksCount(&timer_data_local_det);		
			vector<FaceInfo> res = NextStage(image, input, 24, 24, 2, threshold[1]);
			OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
			OsDrvCprGetSysClockPerUs(&clocksPerUs);
			usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
			printf("**NextStage  2 processing cost %2f ms\n",usedMs);

			rnet_res.insert(rnet_res.end(), res.begin(), res.end());
		}
		printf("\n---mw------------Detect  3001--------------\n");
		rnet_res = NMS(rnet_res, 0.7f, 'u');




		OsDrvTimerStartTicksCount(&timer_data_local_det);	

		BBoxRegression(rnet_res);

		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
		OsDrvCprGetSysClockPerUs(&clocksPerUs);
		usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
		printf("**BBoxRegression processing cost %2f ms\n",usedMs); 


		OsDrvTimerStartTicksCount(&timer_data_local_det);	
		BBoxPadSquare(rnet_res, image.cols, image.rows);
		OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
		OsDrvCprGetSysClockPerUs(&clocksPerUs);
		usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
		printf("**BBoxPadSquare processing cost %2f ms\n",usedMs);


	}

	printf("\n---------mwmwmwmwmwmw-------Detect  4--------------\n");
	t = (double)cvGetTickCount() - t;
	//fprintf(stderr, "Rnet ¼ì²âÊ±¼ä £º %g ms\n", t / ((double)cvGetTickFrequency()*1000.));
    fprintf(stderr, "rnet+++++%g,", t / ((double)cvGetTickFrequency()*1000.));

	t = (double)cvGetTickCount();
	if (stage >= 3 && rnet_res.size()>0)
	{
		int num = (int)rnet_res.size();
		int size = (int)ceil(1.f*num / step_size);
		for (int iter = 0; iter < size; ++iter){
			int start = iter*step_size;
			int end = min(start + step_size, num);
			vector<FaceInfo> input(rnet_res.begin() + start, rnet_res.begin() + end);
			OsDrvTimerStartTicksCount(&timer_data_local_det);	
			vector<FaceInfo> res = NextStage(image, input, 48, 48, 3, threshold[2]);
			OsDrvTimerGetElapsedTicks(&timer_data_local_det,&cyclesElapsed_local_det);
			OsDrvCprGetSysClockPerUs(&clocksPerUs);
			usedMs = cyclesElapsed_local_det /1000.0 / clocksPerUs;
			printf("**NextStage  3 processing cost %2f ms\n",usedMs);   
			onet_res.insert(onet_res.end(), res.begin(), res.end());
		}
		BBoxRegression(onet_res);
		onet_res = NMS(onet_res, 0.7f, 'm');
		BBoxPad(onet_res, image.cols, image.rows);

	}
	printf("\n---------mwmwmwmwmwmw-------Detect  5--------------\n");

	t = (double)cvGetTickCount() - t;
	//fprintf(stderr, "Onet ¼ì²âÊ±¼ä £º %g ms\n", t / ((double)cvGetTickFrequency()*1000.));
	fprintf(stderr, "onet+++++ %g\n", t / ((double)cvGetTickFrequency()*1000.));
	
	
	

	/*
	//²âÊÔÊ¹ÓÃ×îºóÒ»¸öonet
	t = (double)cvGetTickCount();
	if (stage >= 2 && pnet_res.size()>0)
	{
		int num = (int)pnet_res.size();
		cout << "pnet_num=" << num << endl;
		int size = (int)ceil(1.f*num / step_size);
		for (int iter = 0; iter < size; ++iter){
			int start = iter*step_size;
			int end = min(start + step_size, num);
			vector<FaceInfo> input(pnet_res.begin() + start, pnet_res.begin() + end);
			vector<FaceInfo> res = NextStage(image, input, 48, 48, 3, threshold[2]);
			onet_res.insert(onet_res.end(), res.begin(), res.end());
		}
		BBoxRegression(onet_res);
		onet_res = NMS(onet_res, 0.7f, 'm');
		BBoxPad(onet_res, image.cols, image.rows);

	}
	t = (double)cvGetTickCount() - t;
	fprintf(stderr, "Onet ¼ì²âÊ±¼ä £º %g ms\n", t / ((double)cvGetTickFrequency()*1000.));
	return onet_res;
	*/


	if (stage == 1){
		return pnet_res;
	}
	else if (stage == 2){
		return rnet_res;
	}
	else if (stage == 3){
		return onet_res;
	}
	else{
		return onet_res;
	}
}
