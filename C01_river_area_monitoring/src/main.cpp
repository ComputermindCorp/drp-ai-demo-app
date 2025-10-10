/*
 * Original Code (C) Copyright Edgecortix, Inc. 2022
 * Modified Code (C) Copyright Renesas Electronics Corporation 2023
 * Modified Code (C) Copyright Computermind Corp 2024
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
*/

/***********************************************************************************************************************
* File Name    : main.cpp
* Version      : 1.00
* Description  : RZ/V2H and RZ/V2N DRP-AI Sample Application for PyTorch DeepLabv3 + Megvii-Base Detection YOLOX with MIPI/USB Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
/*DRP-AI TVM[*1] Runtime*/
#include "MeraDrpRuntimeWrapper.h"
/*Pre-processing Runtime Header*/
#include "PreRuntime.h"
/*Definition of Macros & other variables*/
#include "define.h"
#include "define_color.h"
/*USB camera control*/
#include "camera.h"
/*Image control*/
#include "image.h"
/*Wayland control*/
#include "wayland.h"
/*YOLOX Post-Processing*/
#include "box.h"
/*IniFile Post-Processing*/
#include "IniFile.h"
/*Mutual exclusion*/
#include <mutex>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <opencv2/opencv.hpp>

using namespace std;
/*****************************************
* Global Variables
******************************************/
/*Multithreading*/
static sem_t terminate_req_sem;
static pthread_t ai_inf_thread;
static pthread_t kbhit_thread;
static pthread_t capture_thread;
static pthread_t img_thread;
static pthread_t hdmi_thread;
static mutex mtx;

/*Flags*/
static atomic<uint8_t> inference_start (0);
static atomic<uint8_t> img_obj_ready   (0);
static atomic<uint8_t> hdmi_obj_ready   (0);

/*Global Variables*/
static uint8_t postproc_data[DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H];
// static uint8_t postproc_data1[DRPAI_OUT_WIDTH * DRPAI_OUT_HEIGHT];

static float drpai_output_buf0[num_inf_out];
static float  drpai_output_buf1[DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H*DEEPLABV3_MODEL_OUT_NUM*(NUM_CLASS_DEEPLABV3-1)];
static uint8_t output_mask[DRPAI_OUT_WIDTH * DRPAI_OUT_HEIGHT];
static uint64_t capture_address;
static uint8_t buf_id;
static Image img;
static IniFile inif;

/*AI Inference for DRPAI*/
/* DRP-AI TVM[*1] Runtime object */
MeraDrpRuntimeWrapper runtime;
MeraDrpRuntimeWrapper runtime_2;
/* Pre-processing Runtime object */
PreRuntime preruntime;
PreRuntime preruntime_2;

static int drpai_fd0 = -1;
static int drpai_fd1 = -1;
static double yolox_drpai_time = 0;
static double deeplabv3_drpai_time = 0;
#ifdef DISP_AI_FRAME_RATE
static double ai_fps = 0;
static double cap_fps = 0;
static double proc_time_capture = 0;
static uint32_t array_cap_time[30] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
#endif /* DISP_AI_FRAME_RATE */
static uint32_t disp_time = 0;
static uint32_t array_drp_time[30] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
static uint32_t array_disp_time[30] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
static float lowest_kpt_score = 0;
static int32_t drp_max_freq;
static int32_t drpai_freq;
static uint32_t ai_time = 0;

/*YOLOX*/
static uint32_t bcount = 0;
static uint32_t ppl_count_local = 0; /*To be used only in Inference Threads*/
static uint32_t person_count = 0;
static vector<detection> det_res;
static vector<detection> det_ppl;
static uint32_t river_area_count = 0;
static uint32_t river_area_cnt;
static uint32_t river_area_count_array[10];

static Wayland wayland;
static vector<detection> det;
static Camera* capture = NULL;

static double yolox_pre_time = 0;
static double yolox_post_time = 0;
static double yolox_ai_time = 0;
static double deeplabv3_pre_time = 0;
static double deeplabv3_post_time = 0;
static double deeplabv3_ai_time = 0;

static uint32_t switch_time_display = 0;

 
/*****************************************
* Function Name     : float16_to_float32
* Description       : Function by Edgecortex. Cast uint16_t a into float value.
* Arguments         : a = uint16_t number
* Return value      : float = float32 number
******************************************/
float float16_to_float32(uint16_t a)
{
    return __extendXfYf2__<uint16_t, uint16_t, 10, float, uint32_t, 23>(a);
}

/*****************************************
* Function Name : timedifference_msec
* Description   : compute the time differences in ms between two moments
* Arguments     : t0 = start time
*                 t1 = stop time
* Return value  : the time difference in ms
******************************************/
static double timedifference_msec(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000000.0;
}

/*****************************************
* Function Name : wait_join
* Description   : waits for a fixed amount of time for the thread to exit
* Arguments     : p_join_thread = thread that the function waits for to Exit
*                 join_time = the timeout time for the thread for exiting
* Return value  : 0 if successful
*                 not 0 otherwise
******************************************/
static int8_t wait_join(pthread_t *p_join_thread, uint32_t join_time)
{
    int8_t ret_err;
    struct timespec join_timeout;
    ret_err = clock_gettime(CLOCK_REALTIME, &join_timeout);
    if ( 0 == ret_err )
    {
        join_timeout.tv_sec += join_time;
        ret_err = pthread_timedjoin_np(*p_join_thread, NULL, &join_timeout);
    }
    return ret_err;
}

/*****************************************
* Function Name : get_result
* Description   : Get DRP-AI Output from memory via DRP-AI Driver
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t get_result()
{
    int8_t ret = 0;
    int32_t i = 0;
    int32_t output_num = 0;
    std::tuple<InOutDataType, void*, int64_t> output_buffer;
    int64_t output_size;
    uint32_t size_count = 0;

    /* Get the number of output of the target model. */
    output_num = runtime.GetNumOutput();
    size_count = 0;
    /*GetOutput loop*/
    for (i = 0; i < output_num; i++)
    {
        /* output_buffer below is tuple, which is { data type, address of output data, number of elements } */
        output_buffer = runtime.GetOutput(i);
        /*Output Data Size = std::get<2>(output_buffer). */
        output_size = std::get<2>(output_buffer);

        /*Output Data Type = std::get<0>(output_buffer)*/
        if (InOutDataType::FLOAT16 == std::get<0>(output_buffer))
        {
            /*Output Data = std::get<1>(output_buffer)*/
            uint16_t *data_ptr = reinterpret_cast<uint16_t *>(std::get<1>(output_buffer));
            for (int j = 0; j < output_size; j++)
            {
                /*FP16 to FP32 conversion*/
                drpai_output_buf0[j + size_count] = float16_to_float32(data_ptr[j]);
            }
        }
        else if (InOutDataType::FLOAT32 == std::get<0>(output_buffer))
        {
            /*Output Data = std::get<1>(output_buffer)*/
            float *data_ptr = reinterpret_cast<float *>(std::get<1>(output_buffer));
            for (int j = 0; j < output_size; j++)
            {
                drpai_output_buf0[j + size_count] = data_ptr[j];
            }
        }
        else
        {
            std::cerr << "[ERROR] Output data type : not floating point." << std::endl;
            ret = -1;
            break;
        }
        size_count += output_size;
    }

    return ret;
}
/*****************************************
* Function Name : get_result
* Description   : Get DRP-AI Output from memory via DRP-AI Driver
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t get_result_2()
{
    int8_t ret = 0;
    int32_t i = 0;
    int32_t output_num = 0;
    std::tuple<InOutDataType, void*, int64_t> output_buffer;
    int64_t output_size;
    uint32_t size_count = 0;

    /* Get the number of output of the target model. */
    output_num = 1;
    size_count = 0;
    /*GetOutput loop*/
    for (i = 0; i < output_num; i++)
    {
        /* output_buffer below is tuple, which is { data type, address of output data, number of elements } */
        output_buffer = runtime_2.GetOutput(i);
        /*Output Data Size = std::get<2>(output_buffer). */
        output_size = std::get<2>(output_buffer);

        /*Output Data Type = std::get<0>(output_buffer)*/
        if (InOutDataType::FLOAT16 == std::get<0>(output_buffer))
        {
            /*Output Data = std::get<1>(output_buffer)*/
            uint16_t *data_ptr = reinterpret_cast<uint16_t *>(std::get<1>(output_buffer));
            for (int j = 0; j < output_size; j++)
            {
                /*FP16 to FP32 conversion*/
                drpai_output_buf1[j + size_count] = float16_to_float32(data_ptr[j]);
            }
        }
        else if (InOutDataType::FLOAT32 == std::get<0>(output_buffer))
        {
            /*Output Data = std::get<1>(output_buffer)*/
            float *data_ptr = reinterpret_cast<float *>(std::get<1>(output_buffer));
            for (int j = 0; j < output_size; j++)
            {
                drpai_output_buf1[j + size_count] = data_ptr[j];
            }
        }
        else
        {
            std::cerr << "[ERROR] Output data type : not floating point." << std::endl;
            ret = -1;
            break;
        }
        size_count += output_size;
    }

    return ret;
}

/*****************************************
 * Function Name : ceil3
 * Description   : ceil num specifiy digit
 * Arguments     : num number
 *               : base ceil digit
 * Return value  : int32_t result
 ******************************************/
static int32_t ceil3(int32_t num, int32_t base)
{
    double x = (double)(num) / (double)(base);
    double y = ceil(x) * (double)(base);
    return (int32_t)(y);
}

/*****************************************
 * Function Name : ceil3
 * Description   : ceil num specifiy digit
 * Arguments     : num number
 *               : base ceil digit
 * Return value  : int32_t result
 ******************************************/
static int32_t Get_river_area_count_average()
{
    int i=0;
    int iCount=0;
    uint32_t river_area_count_Total = 0;
    uint32_t river_area_count_average = 0;
    int iArraySize = SIZE_OF_ARRAY(river_area_count_array);
    for (i=0; i<iArraySize; i++){
        if (river_area_count_array[i] > 0){
            river_area_count_Total += river_area_count_array[i];
            iCount++;
        }
    }
    if (river_area_count_Total > 0){
        river_area_count_average = river_area_count_Total / iCount;
    }else{
        river_area_count_average=0;
    }
    return river_area_count_average;
}

/*****************************************
* Function Name : R_Post_Proc_DeepLabV3
* Description   : CPU post-processing for DeepLabV3
*
* Arguments     : floatarr = drpai output address
* Return value  : -
******************************************/
void R_Post_Proc_DeepLabV3(float* floatarr)
{
    mtx.lock();
    float score;
    int32_t make_hit;
    int i=0;
    int iOut = 0;
    int iBitArray[NUM_CLASS_DEEPLABV3];
    string class_name = CLASS_NAME_RIVER;

    /* Transpose TVM output CHW -> HWC to raise cache utilization efficiency 
       when scanning the probabilities along "C" axis to determine the class index for each pixels. */
    float tmp_output_buf[DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H][NUM_CLASS];
    for (int ci = 0; ci < NUM_CLASS; ci++) 
    {
        for (int i = 0; i < DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H; i++) 
        {
            float current_value = floatarr[ci * DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H + i];
            tmp_output_buf[i][ci] = current_value;
        }
    }

    /*Convert output probabilities to label indice array*/
    for (int i = 0; i < DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H; i++)
    {
        /*Get maximum probability index*/
        uint8_t tmp_max_idx = 0;
        float tmp_max_value = -FLT_MAX;

        for (int ci = 0; ci < NUM_CLASS_DEEPLABV3; ci++) {
            if (tmp_max_value < floatarr[ci * DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H + i]) {
                tmp_max_value = floatarr[ci * DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H + i];
                tmp_max_idx = ci;
            }
        }
        
        postproc_data[i] = tmp_max_idx;
    }
    
    for(i=0;i<NUM_CLASS_DEEPLABV3;i++){
       iBitArray[i]=0;
    }
    
    memcpy(output_mask, postproc_data, DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H * sizeof(uint8_t));
    make_hit=100;  // OK
    uint32_t river_count = 0;
    for(i=0;i< DEEPLABV3_MODEL_IN_W * DEEPLABV3_MODEL_IN_H;i++){
        if (REVER_LABEL_NUM <= output_mask[i]){
            iOut = output_mask[i];
            iBitArray[iOut]=1;
            river_count++;
        }
    }
    int iArraySize = SIZE_OF_ARRAY(river_area_count_array);
    if (river_area_cnt >= iArraySize) river_area_cnt = 0;
    static uint32_t river_area_cnt;
    int idx = river_area_cnt % iArraySize;
    river_area_count_array[idx] = river_count;

    river_area_count = Get_river_area_count_average();

    spdlog::info("DeepLabv3 Result-------------------------------------");
    char strColor[32];
    for(i=0;i<NUM_CLASS_DEEPLABV3;i++){
        if(iBitArray[i] > 0){
            spdlog::info(" Detected Class {} : {}", (i),class_name.c_str());
        }
    }
    
    uint32_t river_pix = inif.get_river_pix();
    uint32_t river_pix_per = (uint32_t)(((double)river_area_count/(double)river_pix) * 100.0);
    if(river_pix<= 0) river_pix_per=0;
    spdlog::info(" Normal River-Pix : {}", river_pix);
    spdlog::info(" River Area Count: {}", river_area_count);
    spdlog::info(" River Area : {} percent", river_pix_per);
    
    /* Clear the score in preparation for the update. */
    lowest_kpt_score = 0;
    score = 1;
    if (make_hit > TH_KPT){
      score = make_hit;
    }
    /* Update the score for display thread. */
    lowest_kpt_score = score;
    goto end;

not_detect:
    lowest_kpt_score = 0;
    goto end;

end:
    mtx.unlock();
    return;
}

/*****************************************
* Function Name : sigmoid
* Description   : Helper function for YOLO Post Processing
* Arguments     : x = input argument for the calculation
* Return value  : sigmoid result of input x
******************************************/
static double sigmoid(double x)
{
    return 1.0/(1.0 + exp(-x));
}

/*****************************************
* Function Name : softmax
* Description   : Helper function for YOLO Post Processing
* Arguments     : val[] = array to be computed softmax
* Return value  : -
******************************************/
static void softmax(float val[NUM_CLASS])
{
    float max_num = -FLT_MAX;
    float sum = 0;
    int32_t i;
    for ( i = 0 ; i<NUM_CLASS ; i++ )
    {
        max_num = max(max_num, val[i]);
    }

    for ( i = 0 ; i<NUM_CLASS ; i++ )
    {
        val[i]= (float) exp(val[i] - max_num);
        sum+= val[i];
    }

    for ( i = 0 ; i<NUM_CLASS ; i++ )
    {
        val[i]= val[i]/sum;
    }
    return;
}

/*****************************************
* Function Name : index
* Description   : Get the index of the bounding box attributes based on the input offset.
* Arguments     : n = output layer number.
*                 offs = offset to access the bounding box attributesd.
*                 channel = channel to access each bounding box attribute.
* Return value  : index to access the bounding box attribute.
******************************************/
int32_t index(uint8_t n, int32_t offs, int32_t channel)
{
    uint8_t num_grid = num_grids[n];
    return offs + channel * num_grid * num_grid;
}

/*****************************************
* Function Name : offset
* Description   : Get the offset nuber to access the bounding box attributes
*                 To get the actual value of bounding box attributes, use index() after this function.
* Arguments     : n = output layer number [0~2].
*                 b = Number to indicate which bounding box in the region [0~2]
*                 y = Number to indicate which region [0~13]
*                 x = Number to indicate which region [0~13]
* Return value  : offset to access the bounding box attributes.
******************************************/
int32_t offset(uint8_t n, int32_t b, int32_t y, int32_t x)
{
    uint8_t num = num_grids[n];
    uint32_t prev_layer_num = 0;
    int32_t i = 0;

    for (i = 0 ; i < n; i++)
    {
        prev_layer_num += NUM_BB *(NUM_CLASS + 5)* num_grids[i] * num_grids[i];
    }
    return prev_layer_num + b *(NUM_CLASS + 5)* num * num + y * num + x;
}

/*****************************************
* Function Name : R_Post_Proc
* Description   : Process CPU post-processing for YoloX
* Arguments     : floatarr = drpai output address
*                 det = detected boxes details
*                 box_count = total number of boxes
* Return value  : -
******************************************/
static void R_Post_Proc(float* floatarr, vector<detection>& det, uint32_t* box_count)
{
    uint32_t count = 0;
    uint32_t BoundingBoxCount = 0;
    /*Memory Access*/
    /* Following variables are required for correct_region_boxes in Darknet implementation*/
    /* Note: This implementation refers to the "darknet detector test" */
    vector<detection> det_buff;
    float new_w, new_h;
    float correct_w = 1.;
    float correct_h = 1.;
    if ((float) (MODEL_IN_W / correct_w) < (float) (MODEL_IN_H/correct_h) )
    {
        new_w = (float) MODEL_IN_W;
        new_h = correct_h * MODEL_IN_W / correct_w;
    }
    else
    {
        new_w = correct_w * MODEL_IN_H / correct_h;
        new_h = MODEL_IN_H;
    }

    int32_t n = 0;
    int32_t b = 0;
    int32_t y = 0;
    int32_t x = 0;
    int32_t offs = 0;
    int32_t i = 0;
    float tx = 0;
    float ty = 0;
    float tw = 0;
    float th = 0;
    float tc = 0;
    float center_x = 0;
    float center_y = 0;
    float box_w = 0;
    float box_h = 0;
    float objectness = 0;
    uint8_t num_grid = 0;
    uint8_t anchor_offset = 0;
    float classes[NUM_CLASS];
    float max_pred = 0;
    int32_t pred_class = -1;
    float probability = 0;
    detection d;
    /* Clear the detected result list */
    det.clear();

    //YOLOX
    int stride = 0;
    vector<int> strides = {8, 16, 32};

    for (n = 0; n<NUM_INF_OUT_LAYER; n++)
    {
        num_grid = num_grids[n];
        anchor_offset = 2 * NUM_BB * (NUM_INF_OUT_LAYER - (n + 1));

        for (b = 0;b<NUM_BB;b++)
        {
           stride = strides[n];
            for (y = 0;y<num_grid;y++)
            {
                for (x = 0;x<num_grid;x++)
                {
                    offs = offset(n, b, y, x);
                    tc = floatarr[index(n, offs, 4)];

                    objectness = tc;

                    if (objectness > TH_PROB)
                    {
                        /* Get the class prediction */
                        for (i = 0;i < NUM_CLASS;i++)
                        {
                            classes[i] = floatarr[index(n, offs, 5+i)];
                        }

                        max_pred = 0;
                        pred_class = -1;
                        for (i = 0; i < NUM_CLASS; i++)
                        {
                            if (classes[i] > max_pred)
                            {
                                pred_class = i;
                                max_pred = classes[i];
                            }
                        }

                        /* Store the result into the list if the probability is more than the threshold */
                        probability = max_pred * objectness;
                        if (probability > TH_PROB)
                        {
                            if (pred_class == PERSON_LABEL_NUM)    //person = 14
                            {
                                tx = floatarr[offs];
                                ty = floatarr[index(n, offs, 1)];
                                tw = floatarr[index(n, offs, 2)];
                                th = floatarr[index(n, offs, 3)];

                                /* Compute the bounding box */
                                /*get_yolo_box/get_region_box in paper implementation*/
                                center_x = (tx+ float(x))* stride;
                                center_y = (ty+ float(y))* stride;
                                center_x = center_x  / (float) MODEL_IN_W;
                                center_y = center_y  / (float) MODEL_IN_H;
                                box_w = exp(tw) * stride;
                                box_h = exp(th) * stride;
                                box_w = box_w / (float) MODEL_IN_W;
                                box_h = box_h / (float) MODEL_IN_H;
                                
                                /* Adjustment for size */
                                /* correct_yolo/region_boxes */
                                center_x = (center_x - (MODEL_IN_W - new_w) / 2. / MODEL_IN_W) / ((float) new_w / MODEL_IN_W);
                                center_y = (center_y - (MODEL_IN_H - new_h) / 2. / MODEL_IN_H) / ((float) new_h / MODEL_IN_H);
                                box_w *= (float) (MODEL_IN_W / new_w);
                                box_h *= (float) (MODEL_IN_H / new_h);

                                center_x = round(center_x * DRPAI_IN_WIDTH);
                                center_y = round(center_y * DRPAI_IN_HEIGHT);
                                box_w = round(box_w * DRPAI_IN_WIDTH);
                                box_h = round(box_h * DRPAI_IN_HEIGHT);
                                
                                if (center_x < IMAGE_OUTPUT_WIDTH && center_y < IMAGE_OUTPUT_HEIGHT){
                                    Box bb = {center_x, center_y, box_w, box_h};
                                    d = {bb, pred_class, probability};
                                    det_buff.push_back(d);
                                    count++;
                                }
                            }
                            BoundingBoxCount++;
                        }
                    }
                }
            }
        }
    }
    /* Non-Maximum Supression filter */
    filter_boxes_nms(det_buff, det_buff.size(), TH_NMS);
    *box_count = count;
    /* Log Output */
    spdlog::info("YOLOX Result-------------------------------------");
    int iBoxCount=0;
    for(i = 0; i < det_buff.size(); i++)
    {
        /* Skip the overlapped bounding boxes */
        if (det_buff[i].prob == 0) continue;
        spdlog::info(" Bounding Box Number : {}",iBoxCount+1);
        spdlog::info(" Bounding Box        : (X, Y, W, H) = ({}, {}, {}, {})", (int)det_buff[i].bbox.x, (int)det_buff[i].bbox.y, (int)det_buff[i].bbox.w, (int)det_buff[i].bbox.h);
        spdlog::info(" Detected Class      : {} (Class {})", label_file_map[det_buff[i].c].c_str(), det_buff[i].c);
        spdlog::info(" Probability         : {} %", (std::round((det_buff[i].prob*100) * 10) / 10));
        iBoxCount++;
    }
    person_count = iBoxCount;
    spdlog::info(" Bounding Box Count  : {}", BoundingBoxCount);
    spdlog::info(" Person Count        : {}", person_count);

    mtx.lock();
    /* Clear the detected result list */
    det.clear();
    copy(det_buff.begin(), det_buff.end(), back_inserter(det));
    mtx.unlock();
    return ;
}

/*****************************************
* Function Name : people_counter
* Description   : Function to count the real number of people detected and does not exceeds the maximum number
* Arguments     : det = detected boxes details
*                 ppl = detected people details
*                 box_count = total number of boxes
*                 ppl_count = actual number of people
* Return value  : -
******************************************/
static void people_counter(vector<detection>& det, vector<detection>& ppl, uint32_t box_count, uint32_t* ppl_count)
{
    mtx.lock();
    uint32_t count = 0;
    ppl.clear();
    for(uint32_t i = 0; i<box_count; i++)
    {
        if(0 == det[i].prob)
        {
            continue;
        }
        else
        {
            ppl.push_back(det[i]);
            count++;
            if(count > NUM_MAX_PERSON-1)
            {
                break;
            }
        }
    }
    *ppl_count = count;
    mtx.unlock();
}

/*****************************************
* Function Name : sign
* Description   : Get the sign of the input value
* Arguments     : x = input value
* Return value  : returns the sign, 1 if positive -1 if not
*******************************************/
static int8_t sign(int32_t x)
{
    return x > 0 ? 1 : -1;
}

/*****************************************
* Function Name : draw_bounding_box
* Description   : Draw bounding box on image.
* Arguments     : -
* Return value  : 0 if succeeded
*               not 0 otherwise
******************************************/
void draw_bounding_box(uint32_t alert_color)
{
    vector<detection> det_buff;
    stringstream stream;
    string result_str;
    int32_t i = 0;
    uint32_t color=0;
 
    mtx.lock();
    copy(det_res.begin(), det_res.end(), back_inserter(det_buff));
    mtx.unlock();

    /* Draw bounding box on RGB image. */
    for (i = 0; i < det_buff.size(); i++)
    {
        /* Skip the overlapped bounding boxes */
        if (det_buff[i].prob == 0) continue;
        
        //color = box_color[det_buff[i].c];
        color = alert_color;
        /* Clear string stream for bounding box labels */
        stream.str("");
        /* Draw the bounding box on the image */
        stream << fixed << setprecision(2) << det_buff[i].prob;
        result_str = label_file_map[det_buff[i].c]+ " "+ stream.str();
        img.draw_rect_box((int)det_buff[i].bbox.x, (int)det_buff[i].bbox.y, (int)det_buff[i].bbox.w, (int)det_buff[i].bbox.h, result_str.c_str(),color,CHAR_SCALE_FONT);
    }
    return;
}

/*****************************************
* Function Name : print_result
* Description   : print the result on display.
* Arguments     : -
* Return value  : 0 if succeeded
*               not 0 otherwise
******************************************/
int8_t print_result(Image* img)
{
#ifdef DEBUG_TIME_FLG
    using namespace std;
    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
#endif // DEBUG_TIME_FLG
    bool bAlert = false;
    int32_t index = 0;
    int32_t signal_color = COLOR_NORMAL_DATA;
    int32_t person_color = COLOR_NORMAL_DATA;
    int32_t person_box_color = COLOR_NORMAL_DATA;
    int32_t signal_color_str = BLACK_DATA;
    stringstream stream;
    string str = "";
    string water_level_str = "";
 
    uint32_t river_pix = inif.get_river_pix();
    uint32_t river_pix_per = (uint32_t)(((double)river_area_count/(double)river_pix) * 100.0);
    if(river_pix<= 0) river_pix_per=0;

    /* Alrm Check.*/
    uint32_t person_alert_per_rate = inif.get_person_alert_per_rate();
    uint32_t caution_per_rate = inif.get_caution_per_rate();
    uint32_t warning_per_rate = inif.get_warning_per_rate();
    uint32_t hazard_per_rate = inif.get_hazard_per_rate();
    if(river_pix_per > hazard_per_rate){
       bAlert = true;
       signal_color = COLOR_HAZARD_DATA;
       signal_color_str = WHITE_DATA;
       water_level_str = "HAZARD WATER LEVEL";
    }else if(river_pix_per > warning_per_rate){
       bAlert = true;
       signal_color = COLOR_WARNING_DATA;
       signal_color_str = WHITE_DATA;
       water_level_str = "WARNING WATER LEVEL";
    }else if(river_pix_per > caution_per_rate){
       bAlert = true;
       signal_color = COLOR_CAUTION_DATA;
       signal_color_str = BLACK_DATA;
       water_level_str = "CAUTION WATER LEVEL";
    }else{
       bAlert = false;
       signal_color = COLOR_NORMAL_DATA;
       signal_color_str = BLACK_DATA;
       water_level_str = "Normal water level";
    }
    if(river_pix_per > person_alert_per_rate && person_count > 0){
       person_color = WHITE_DATA;
       person_box_color = COLOR_PERSON_ALERT_DATA;
    }else{
       person_color = WHITE_DATA;
       person_box_color = box_color[PERSON_LABEL_NUM];
    }

    /* Draw Segmentation on RGB image.*/
    int ret = img->draw_sem_seg(&output_mask[0], signal_color, capture, DEEPLABV3_MODEL_IN_W, DEEPLABV3_MODEL_IN_H, DRPAI_OUT_WIDTH, DRPAI_OUT_HEIGHT, CAM_RESIZED_PADDING);
    if (ret != 0)
    {
        return -1;
    }

#if (0) == INF_YOLOX_SKIP
    /* Draw bounding box on image. */
    draw_bounding_box(person_box_color);
#endif

    /* Draw Post-Proc Time on RGB image.*/
    stream.str("");
    stream << "River Area : " << std::setw(3) << std::fixed << std::setprecision(1) << river_pix_per << "  %";
    str = stream.str();
    index++;
    img->write_string_overlay(str, 1, TEXT_WIDTH_OFFSET_L, LINE_HEIGHT_OFFSET + (LINE_HEIGHT_LARGE * index), CHAR_SCALE_LARGE_LARGE, WHITE_DATA);

#if (0) == INF_YOLOX_SKIP
    stream.str("");
    stream << "Person around the river : " << std::setw(3) << std::fixed << std::setprecision(1) << person_count;
    str = stream.str();
    index++;
    img->write_string_overlay(str, 1, TEXT_WIDTH_OFFSET_L, LINE_HEIGHT_OFFSET + (LINE_HEIGHT_LARGE * index), CHAR_SCALE_LARGE_LARGE, WHITE_DATA);
#endif

    stream.str("");
    stream << water_level_str << std::setw(3) << std::fixed << std::setprecision(1);
    str = stream.str();
    index++;
    if (bAlert == false){
        img->write_string_overlay(str, 1, TEXT_WIDTH_OFFSET_L, LINE_HEIGHT_OFFSET + (LINE_HEIGHT_LARGE * index), CHAR_SCALE_LARGE_LARGE, WHITE_DATA);
    }else{
        img->draw_rect_label_box((IMAGE_OUTPUT_WIDTH/2 - IMAGE_OUTPUT_WIDTH / 4), IMAGE_OUTPUT_HEIGHT / 2, 2, 2, water_level_str.c_str(),signal_color,CHAR_SCALE_ALERT,signal_color_str);
    }

    double disp_yolox_pre_time      = 0;
    double disp_yolox_post_time     = yolox_post_time;
    double disp_yolox_ai_time       = yolox_ai_time;
    double disp_deeplabv3_pre_time  = 0;
    double disp_deeplabv3_post_time = deeplabv3_post_time;
    double disp_deeplabv3_ai_time   = deeplabv3_ai_time;
/* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 1 
    disp_yolox_pre_time += yolox_pre_time;
#else
    disp_yolox_post_time += yolox_pre_time;
#endif  /* PREPROC_MODE_YOLOX */
#if PREPROC_MODE_DEEPLABV3 == 1 
    disp_deeplabv3_pre_time  += deeplabv3_pre_time;
#else
    disp_deeplabv3_post_time  += deeplabv3_pre_time;
#endif  /* PREPROC_MODE_DEEPLABV3 */
    
    double disp_pre_time = disp_yolox_pre_time + disp_deeplabv3_pre_time;
    double disp_ai_time = disp_yolox_ai_time + disp_deeplabv3_ai_time;
    double disp_post_time = disp_yolox_post_time + disp_deeplabv3_post_time;
    double disp_total_time = disp_pre_time  + disp_ai_time + disp_post_time;
    
    index=0;
    /* Draw Total Time Result on RGB image.*/
    stream.str("");
#if (0) == INF_YOLOX_SKIP
    stream << "Total AI Time (DeepLabv3+YOLOX): " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_total_time * 10) / 10 << "msec";
#else
    stream << "Total AI Time (DeepLabv3): " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_total_time * 10) / 10 << "msec";
#endif
    str = stream.str();
    index++;
    img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    
    if (switch_time_display == 1){
        /* Draw PreProcess Time on RGB image.*/
        stream.str("");
        stream << "DeepLabv3              DRP-AI TVM Pre-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_deeplabv3_pre_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);

        /* Draw Inference Time on RGB image.*/
        stream.str("");
        stream << "DRP-AI TVM (Inference + Data loading) : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_deeplabv3_ai_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    
        /* Draw PostProcess Time on RGB image.*/
        stream.str("");
        stream << "CPU Post-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_deeplabv3_post_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);

#if (0) == INF_YOLOX_SKIP
        /* Draw PreProcess Time on RGB image.*/
        stream.str("");
        stream << "YOLOX              DRP-AI TVM Pre-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_yolox_pre_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);

        /* Draw Inference Time on RGB image.*/
        stream.str("");
        stream << "DRP-AI TVM (Inference + Data loading) : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_yolox_ai_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    
        /* Draw PostProcess Time on RGB image.*/
        stream.str("");
        stream << "CPU Post-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_yolox_post_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
#endif
    
#ifdef DISP_AI_FRAME_RATE
        /* Draw AI/Camera Frame Rate on RGB image.*/
        stream.str("");
        stream << "AI/Camera Frame Rates : " << std::setw(3) << (uint32_t)ai_fps << "/" << (uint32_t)cap_fps << "fps";
        str = stream.str();
        index++;
        img->write_string_overlay(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
#endif /* DISP_AI_FRAME_RATE */
    }

    
#ifdef DEBUG_TIME_FLG
    end = chrono::system_clock::now();
    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0);
    printf("Draw Text Time            : %lf[ms]\n", time);
#endif // DEBUG_TIME_FLG

    return 0;
}

/*****************************************
* Function Name : R_Inf_Thread
* Description   : Executes the DRP-AI inference thread
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Inf_Thread(void *threadid)
{
    /*Semaphore Variable*/
    int32_t inf_sem_check = 0;
    int32_t inf_cnt = -1;
    /*Variable for getting Inference output data*/
    void* output_ptr;
    void* output_ptr2;
    uint32_t out_size;
    /*Variable for Pre-processing parameter configuration*/
    s_preproc_param_t in_param;
    s_preproc_param_t in_param_2;
   
    /*Inference Variables*/
    fd_set rfds;
    fd_set rfds0;
    struct timespec tv;
    int8_t inf_status = 0;
    drpai_status_t drpai_status0;
    drpai_status_t drpai_status1;
    /*Variable for checking return value*/
    int8_t ret = 0;
    /*Variable for Performance Measurement*/
    static struct timespec yolox_run_start_time;
    static struct timespec yolox_run_end_time;
    static struct timespec yolox_pre_start_time;
    static struct timespec yolox_pre_end_time;
    static struct timespec yolox_post_start_time;
    static struct timespec yolox_post_end_time;
    static struct timespec deeplabv3_run_start_time;
    static struct timespec deeplabv3_run_end_time;
    static struct timespec deeplabv3_pre_start_time;
    static struct timespec deeplabv3_pre_end_time;
    static struct timespec deeplabv3_post_start_time;
    static struct timespec deeplabv3_post_end_time;
    static struct timespec drp_prev_time = { .tv_sec = 0, .tv_nsec = 0, };
    /*DeepLabV3 Modify Parameters*/
    drpai_crop_t crop_param;
    static string drpai_param_file;
    uint32_t drp_param_info_size;
    uint8_t i;
    cv::Mat c_frame;

    printf("Inference Thread Starting\n");
    
    in_param.pre_in_shape_w = CAM_IMAGE_WIDTH;
    in_param.pre_in_shape_h = CAM_IMAGE_HEIGHT;
    in_param_2.pre_in_shape_w = CAM_IMAGE_WIDTH;
    in_param_2.pre_in_shape_h = CAM_IMAGE_HEIGHT;
    river_area_cnt = 0;
	
    printf("Inference Loop Starting\n");
    /*Inference Loop Start*/
    while(1)
    {
        inf_cnt++;
        spdlog::info("[START] Start DRP-AI Inference...");
        spdlog::info("Inference ----------- No. {}", (inf_cnt + 1));
        while(1)
        {
            /*Gets the Termination request semaphore value. If different then 1 Termination was requested*/
            /*Checks if sem_getvalue is executed wihtout issue*/
            errno = 0;
            ret = sem_getvalue(&terminate_req_sem, &inf_sem_check);
            if (0 != ret)
            {
                fprintf(stderr, "[ERROR] Failed to get Semaphore Value: errno=%d\n", errno);
                goto err;
            }
            /*Checks the semaphore value*/
            if (1 != inf_sem_check)
            {
                goto ai_inf_end;
            }
            /*Checks if image frame from Capture Thread is ready.*/
            if (inference_start.load())
            {
                break;
            }
            usleep(WAIT_TIME);
        }

        //----------------------------------------------------------------------------------------------
        // YOLOX
        //----------------------------------------------------------------------------------------------
        in_param.pre_in_addr    = (uintptr_t) capture_address;
        /*Gets Pre-process starting time*/
        ret = timespec_get(&yolox_pre_start_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Pre-process Start Time\n");
            goto err;
        }
/* PREPROC_MODE_DEEPLABV3 (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_DEEPLABV3 == 0
        c_frame = img.convert_format_Mat();
#else
/* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 0
        c_frame = img.convert_format_Mat();
#endif  /* PREPROC_MODE_YOLOX */
                	
#endif  /* PREPROC_MODE_DEEPLABV3 */    	

#if (0) == INF_YOLOX_SKIP

        /* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 1 
        ret = preruntime.Pre(&in_param, &output_ptr, &out_size);
        if (0 < ret)
        {
            fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime Pre()\n");
            goto err;
        }
#else
        // YOLOX
        cv::Mat frame1;
        cv::Size size(MODEL_IN_H, MODEL_IN_W);
        // *resize the image to the model input size*/
        cv::resize(c_frame, frame1, size);

        /* changing channel from hwc to chw */
        vector<cv::Mat> rgb_images;
        cv::split(frame1, rgb_images);
        cv::Mat m_flat_r = rgb_images[0].reshape(1, 1);
        cv::Mat m_flat_g = rgb_images[1].reshape(1, 1);
        cv::Mat m_flat_b = rgb_images[2].reshape(1, 1);
        cv::Mat matArray[] = {m_flat_r, m_flat_g, m_flat_b};
        cv::Mat frameWork;
        cv::Mat frameCHW;
        cv::hconcat(matArray, 3, frameCHW);
        /*convert to FP32*/
        frameCHW.convertTo(frameCHW, CV_32FC3);

        /* normailising  pixels */
        cv::divide(frameCHW, 255.0, frameCHW);

        /* DRP AI input image should be continuous buffer */
        if (!frameCHW.isContinuous()){
            frameCHW = frameCHW.clone();
        }
        cv::Mat frame_yolox = frameCHW;

#endif  /* PREPROC_MODE_YOLOX */

        /*Set Pre-processing output to be inference input. */
/* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 1 
        runtime.SetInput(0, (float*)output_ptr);
#else
        runtime.SetInput(0, frame_yolox.ptr<float>());
#endif  /* PREPROC_MODE_YOLOX */

        /*Gets AI Pre-process End Time*/
        ret = timespec_get(&yolox_pre_end_time, TIME_UTC);
        if ( 0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get Pre-process End Time\n");
            goto err;
        }
        
        /*Pre-process Time Result*/
        yolox_pre_time = (timedifference_msec(yolox_pre_start_time, yolox_pre_end_time) * TIME_COEF);

        /*Gets inference starting time*/
        ret = timespec_get(&yolox_run_start_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Inference Start Time\n");
            goto err;
        }

        runtime.Run();
        /*Gets AI Inference End Time*/
        ret = timespec_get(&yolox_run_end_time, TIME_UTC);
        if ( 0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get Inference End Time\n");
            goto err;
        }
        /*Inference Time Result*/
        yolox_ai_time = (timedifference_msec(yolox_run_start_time, yolox_run_end_time) * TIME_COEF);

        /*Gets Post-process starting time*/
        ret = timespec_get(&yolox_post_start_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Post-process Start Time\n");
            goto err;
        }

        /*Process to read the DRPAI output data.*/
        ret = get_result();
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get result from memory.\n");
            goto err;
        }
        /*CPU Post-Processing For YOLOX*/
        bcount = 0;
        det_res.clear();
        R_Post_Proc(drpai_output_buf0, det_res, &bcount);
        /*Count the Number of People Detected*/
        ppl_count_local = 0;
        people_counter(det_res, det_ppl, bcount, &ppl_count_local);

        /* R_Post_Proc time end*/
        ret = timespec_get(&yolox_post_end_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get R_Post_Proc End Time\n");
            goto err;
        }
        yolox_post_time = (timedifference_msec(yolox_post_start_time, yolox_post_end_time)*TIME_COEF);
#endif  /* INF_YOLOX_SKIP */

        //----------------------------------------------------------------------------------------------
        // DeepLabV3
        //----------------------------------------------------------------------------------------------
        in_param_2.pre_in_addr    = (uintptr_t) capture_address;
        /*Gets Pre-process starting time*/
        ret = timespec_get(&deeplabv3_pre_start_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Pre-process Start Time\n");
            goto err;
        }
/* PREPROC_MODE_DEEPLABV3 (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_DEEPLABV3 == 1 
        ret = preruntime_2.Pre(&in_param_2, &output_ptr2, &out_size);
        if (0 < ret)
        {
            fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime_2 Pre()\n");
            goto err;
        }
#else
        // DeebLabV3
        cv::Mat cropped_image = c_frame;
        cv::Mat frame2;
        cv::Size size2(DEEPLABV3_MODEL_IN_W, DEEPLABV3_MODEL_IN_H);
        /*resize the image to the model input size*/
        cv::resize(cropped_image, frame2, size2);
        vector<cv::Mat> rgb_imagesres;
        split(frame2, rgb_imagesres);
        cv::Mat m_flat_r_res = rgb_imagesres[0].reshape(1, 1);
        cv::Mat m_flat_g_res = rgb_imagesres[1].reshape(1, 1);
        cv::Mat m_flat_b_res = rgb_imagesres[2].reshape(1, 1);
        cv::Mat matArrayres[] = {m_flat_r_res, m_flat_g_res, m_flat_b_res};
        cv::Mat frameCHWres;
        cv::hconcat(matArrayres, 3, frameCHWres);
        /*convert to FP32*/
        frameCHWres.convertTo(frameCHWres, CV_32FC3);

        /* normailising  pixels */
        cv::divide(frameCHWres, 255.0, frameCHWres);

        // /* DRP AI input image should be continuous buffer */
        if (!frameCHWres.isContinuous())
            frameCHWres = frameCHWres.clone();

        cv::Mat frame_deeblabv3 = frameCHWres;

#endif  /* PREPROC_MODE_DEEPLABV3 */

        
        /*Set Pre-processing output to be inference input. */
/* PREPROC_MODE_DEEPLABV3 (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_DEEPLABV3 == 1 
        runtime_2.SetInput(0, (float*)output_ptr2);
#else
        runtime_2.SetInput(0, frame_deeblabv3.ptr<float>());
#endif  /* PREPROC_MODE_DEEPLABV3 */

        /*Gets AI Pre-process End Time*/
        ret = timespec_get(&deeplabv3_pre_end_time, TIME_UTC);
        if ( 0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get Pre-process End Time\n");
            goto err;
        }
        
        /*Pre-process Time Result*/
        deeplabv3_pre_time = (timedifference_msec(deeplabv3_pre_start_time, deeplabv3_pre_end_time) * TIME_COEF);

        /*Gets inference starting time*/
        ret = timespec_get(&deeplabv3_run_start_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Inference Start Time\n");
            goto err;
        }
        runtime_2.Run();
        /*Gets AI Inference End Time*/
        ret = timespec_get(&deeplabv3_run_end_time, TIME_UTC);
        if ( 0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get Inference End Time\n");
            goto err;
        }
        /*Inference Time Result*/
        deeplabv3_ai_time = (timedifference_msec(deeplabv3_run_start_time, deeplabv3_run_end_time) * TIME_COEF);

        /*Gets Post-process starting time*/
        ret = timespec_get(&deeplabv3_post_start_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Post-process Start Time\n");
            goto err;
        }

        /*Process to read the DRPAI output data.*/
        ret = get_result_2();
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get result from memory.\n");
            goto err;
        }
        
        /*Preparation for Post-Processing*/
        /*CPU Post-Processing For Deeplabv3*/
        R_Post_Proc_DeepLabV3(drpai_output_buf1);

        /* R_Post_Proc time end*/
        ret = timespec_get(&deeplabv3_post_end_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get R_Post_Proc End Time\n");
            goto err;
        }
        deeplabv3_post_time = (timedifference_msec(deeplabv3_post_start_time, deeplabv3_post_end_time)*TIME_COEF);

        double disp_yolox_pre_time      = 0;
        double disp_yolox_post_time     = yolox_post_time;
        double disp_yolox_ai_time       = yolox_ai_time;
        double disp_deeplabv3_pre_time  = 0;
        double disp_deeplabv3_post_time = deeplabv3_post_time;
        double disp_deeplabv3_ai_time   = deeplabv3_ai_time;
/* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 1 
        disp_yolox_pre_time += yolox_pre_time;
#else
        disp_yolox_post_time += yolox_pre_time;
#endif  /* PREPROC_MODE_YOLOX */
#if PREPROC_MODE_DEEPLABV3 == 1 
        disp_deeplabv3_pre_time  += deeplabv3_pre_time;
#else
        disp_deeplabv3_post_time  += deeplabv3_pre_time;
#endif  /* PREPROC_MODE_DEEPLABV3 */
        
#if (0) == INF_YOLOX_SKIP
        /*Display Processing YOLOX Time On Log File*/
        yolox_drpai_time = (disp_yolox_pre_time + disp_yolox_ai_time + disp_yolox_post_time);
        spdlog::info("YOLOX");
        spdlog::info(" DRP-AI TVM Pre-Processing :  {} [ms]", std::round(disp_yolox_pre_time  * 10) / 10);
        spdlog::info(" DRP-AI TVM (Inference + Data loading) : {} [ms]", std::round(disp_yolox_ai_time * 10) / 10);
        spdlog::info(" CPU Post-Processing : {} [ms]", std::round(disp_yolox_post_time * 10) / 10);
#endif

        /*Display Processing DeepLabv3 Time On Log File*/
        /*Display Processing Time On Console*/
    	deeplabv3_drpai_time = (disp_deeplabv3_pre_time + disp_deeplabv3_ai_time + disp_deeplabv3_post_time);
        spdlog::info("DeepLabv3");
        spdlog::info(" DRP-AI TVM Pre-Processing :  {} [ms]", std::round(disp_deeplabv3_pre_time  * 10) / 10);
        spdlog::info(" DRP-AI TVM (Inference + Data loading) : {} [ms]", std::round(disp_deeplabv3_ai_time * 10) / 10);
        spdlog::info(" CPU Post-Processing : {} [ms]", std::round(disp_deeplabv3_post_time * 10) / 10);
        
        /*Display Processing Frame Rate On Log File*/
        ai_time = (uint32_t)((timedifference_msec(drp_prev_time, deeplabv3_post_end_time) * TIME_COEF));
        int idx = inf_cnt % SIZE_OF_ARRAY(array_drp_time);
        array_drp_time[idx] = ai_time;
        drp_prev_time = deeplabv3_post_end_time;
#ifdef DISP_AI_FRAME_RATE
        int arraySum = std::accumulate(array_drp_time, array_drp_time + SIZE_OF_ARRAY(array_drp_time), 0);
        double arrayAvg = 1.0 * arraySum / SIZE_OF_ARRAY(array_drp_time);
        ai_fps = 1.0 / arrayAvg * 1000.0 + 0.5;
        spdlog::info("AI Frame Rate {} [fps]", (int32_t)ai_fps);
#endif /* DISP_AI_FRAME_RATE */
        inference_start.store(0);
    }
    /*End of Inference Loop*/

/*Error Processing*/
err:
    /*Set Termination Request Semaphore to 0*/
    sem_trywait(&terminate_req_sem);
    goto ai_inf_end;
/*AI Thread Termination*/
ai_inf_end:
    /*To terminate the loop in Capture Thread.*/
    printf("AI Inference Thread Terminated\n");
    pthread_exit(NULL);
}

/*****************************************
* Function Name : R_Capture_Thread
* Description   : Executes the V4L2 capture with Capture thread.
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Capture_Thread(void *threadid)
{
    Camera* capture = (Camera*) threadid;
    /*Semaphore Variable*/
    int32_t capture_sem_check = 0;
    /*First Loop Flag*/
    uint64_t capture_addr = 0;
    int8_t ret = 0;
    uint8_t * img_buffer;
    uint8_t * img_buffer0;
    uint8_t capture_stabe_cnt = 8;  // Counter to wait for the camera to stabilize
    int32_t cap_cnt = -1;
#ifdef DISP_AI_FRAME_RATE
    static struct timespec capture_time;
    static struct timespec capture_time_prev = { .tv_sec = 0, .tv_nsec = 0, };
#endif /* DISP_AI_FRAME_RATE */
    
    printf("Capture Thread Starting\n");

    img_buffer0 = (uint8_t *)capture->drpai_buf->mem;
    capture_address = capture->drpai_buf->phy_addr;
    while(1)
    {
        /*Gets the Termination request semaphore value. If different then 1 Termination was requested*/
        /*Checks if sem_getvalue is executed wihtout issue*/
        errno = 0;
        ret = sem_getvalue(&terminate_req_sem, &capture_sem_check);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Semaphore Value: errno=%d\n", errno);
            goto err;
        }
        /*Checks the semaphore value*/
        if (1 != capture_sem_check)
        {
            goto capture_end;
        }

        /* Capture USB camera image and stop updating the capture buffer */
        capture_addr = (uint32_t)capture->capture_image();
#ifdef DISP_AI_FRAME_RATE
        cap_cnt++;
        ret = timespec_get(&capture_time, TIME_UTC);
        proc_time_capture = (timedifference_msec(capture_time_prev, capture_time) * TIME_COEF);
        capture_time_prev = capture_time;

        int idx = cap_cnt % SIZE_OF_ARRAY(array_cap_time);
        array_cap_time[idx] = (uint32_t)proc_time_capture;
        int arraySum = std::accumulate(array_cap_time, array_cap_time + SIZE_OF_ARRAY(array_cap_time), 0);
        double arrayAvg = 1.0 * arraySum / SIZE_OF_ARRAY(array_cap_time);
        cap_fps = 1.0 / arrayAvg * 1000.0 + 0.5;
#endif /* DISP_AI_FRAME_RATE */

        if (capture_addr == 0)
        {
            fprintf(stderr, "[ERROR] Failed to capture image from camera.\n");
            goto err;
        }
        else
        {
            /* Do not process until the camera stabilizes, because the image is unreliable until the camera stabilizes. */
            if( capture_stabe_cnt > 0 )
            {
                capture_stabe_cnt--;
            }
            else
            {
                img_buffer = capture->get_img();
                if (!inference_start.load())
                {
                    /* Copy captured image to Image object. This will be used in Display Thread. */
                    memcpy(img_buffer0, img_buffer, capture->get_size());
/* PREPROC_MODE_DEEPLABV3 (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_DEEPLABV3 == 0
                    img.camera_to_capture_image(img_buffer, capture->get_size());
#else
/* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 0
                    img.camera_to_capture_image(img_buffer, capture->get_size());
#endif  /* PREPROC_MODE_YOLOX */
                	
#endif  /* PREPROC_MODE_DEEPLABV3 */
                    /* Flush capture image area cache */
                    ret = capture->video_buffer_flush_dmabuf(capture->drpai_buf->idx, capture->drpai_buf->size);
                    if (0 != ret)
                    {
                        goto err;
                    }
                    inference_start.store(1); /* Flag for AI Inference Thread. */
                }

                if (!img_obj_ready.load())
                {
                    img.camera_to_image(img_buffer, capture->get_size());
                    ret = capture->video_buffer_flush_dmabuf(capture->wayland_buf->idx, capture->wayland_buf->size);
                    if (0 != ret)
                    {
                        goto err;
                    }
                    img_obj_ready.store(1); /* Flag for Display Thread. */
                }
            }
        }

        /* IMPORTANT: Place back the image buffer to the capture queue */
        ret = capture->capture_qbuf();
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to enqueue capture buffer.\n");
            goto err;
        }
    } /*End of Loop*/

/*Error Processing*/
err:
    sem_trywait(&terminate_req_sem);
    goto capture_end;

capture_end:
    /*To terminate the loop in AI Inference Thread.*/
    inference_start.store(1);

    printf("Capture Thread Terminated\n");
    pthread_exit(NULL);
}

/*****************************************
* Function Name : R_Img_Thread
* Description   : Executes img proc with img thread
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Img_Thread(void *threadid)
{
    /*Semaphore Variable*/
    int32_t hdmi_sem_check = 0;
    /*Variable for checking return value*/
    int8_t ret = 0;
    double img_proc_time = 0;
    int32_t disp_cnt = 0;
    timespec start_time;
    timespec end_time;

    printf("Image Thread Starting\n");
    while(1)
    {
        /*Gets The Termination Request Semaphore Value, If Different Then 1 Termination Is Requested*/
        /*Checks If sem_getvalue Is Executed Without Issue*/
        errno = 0;
        ret = sem_getvalue(&terminate_req_sem, &hdmi_sem_check);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Semaphore Value: errno=%d\n", errno);
            goto err;
        }
        /*Checks the semaphore value*/
        if (1 != hdmi_sem_check)
        {
            goto hdmi_end;
        }
        /* Check img_obj_ready flag which is set in Capture Thread. */
        if (img_obj_ready.load())
        {
            ret = timespec_get(&start_time, TIME_UTC);
            if (0 == ret)
            {
                fprintf(stderr, "[ERROR] Failed to get Display Start Time\n");
                goto err;
            }
            
            /* Convert YUYV image to BGRA format. */
            img.convert_format();

            /* Convert output image size. */
            img.convert_size(CAM_IMAGE_WIDTH, DRPAI_OUT_WIDTH, CAM_IMAGE_HEIGHT, DRPAI_OUT_HEIGHT, CAM_RESIZED_PADDING);

            /*displays AI Inference Results on display.*/
            ret = print_result(&img);
            if (0 != ret)
            {
                fprintf(stderr, "[ERROR] Failed to displays AI Inference Results on display.\n");
                goto err;
            }

            buf_id = img.get_buf_id();
            img_obj_ready.store(0);

            if (!hdmi_obj_ready.load())
            {
                hdmi_obj_ready.store(1); /* Flag for AI Inference Thread. */
            }
            
            ret = timespec_get(&end_time, TIME_UTC);
            if (0 == ret)
            {
                fprintf(stderr, "[ERROR] Failed to Get Display End Time\n");
                goto err;
            }
            img_proc_time = (timedifference_msec(start_time, end_time) * TIME_COEF);
            
#ifdef DEBUG_TIME_FLG
            printf("Img Proc Time             : %lf[ms]\n", img_proc_time);
#endif
        }
        usleep(WAIT_TIME); //wait 1 tick time
    } /*End Of Loop*/

/*Error Processing*/
err:
    /*Set Termination Request Semaphore To 0*/
    sem_trywait(&terminate_req_sem);
    goto hdmi_end;

hdmi_end:
    /*To terminate the loop in Capture Thread.*/
    img_obj_ready.store(0);
    printf("Img Thread Terminated\n");
    pthread_exit(NULL);
}
/*****************************************
* Function Name : R_Display_Thread
* Description   : Executes the HDMI Display with Display thread
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Display_Thread(void *threadid)
{
    /*Semaphore Variable*/
    int32_t hdmi_sem_check = 0;
    /*Variable for checking return value*/
    int8_t ret = 0;
    double disp_proc_time = 0;
    int32_t disp_cnt = 0;

    timespec start_time;
    timespec end_time;
    static struct timespec disp_prev_time = { .tv_sec = 0, .tv_nsec = 0, };

    /* Initialize waylad (draw overlay)*/
    ret = wayland.init(capture->wayland_buf->idx, IMAGE_OUTPUT_WIDTH, IMAGE_OUTPUT_HEIGHT, IMAGE_CHANNEL_BGRA,true);
    if(0 != ret)
    {
        fprintf(stderr, "[ERROR] Failed to initialize Image for Wayland\n");
        goto err;
    }

    printf("Display Thread Starting\n");
    while(1)
    {
        /*Gets The Termination Request Semaphore Value, If Different Then 1 Termination Is Requested*/
        /*Checks If sem_getvalue Is Executed Without Issue*/
        errno = 0;
        ret = sem_getvalue(&terminate_req_sem, &hdmi_sem_check);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Semaphore Value: errno=%d\n", errno);
            goto err;
        }
        /*Checks the semaphore value*/
        if (1 != hdmi_sem_check)
        {
            goto hdmi_end;
        }
        /* Check hdmi_obj_ready flag which is set in Capture Thread. */
        if (hdmi_obj_ready.load())
        {
            ret = timespec_get(&start_time, TIME_UTC);
            if (0 == ret)
            {
                fprintf(stderr, "[ERROR] Failed to get Display Start Time\n");
                goto err;
            }
            /*Update Wayland*/
            wayland.commit(img.get_img(buf_id), img.get_overlay_img(buf_id));

            /*Reset Overlay image*/
            img.reset_overlay_img(buf_id);

            hdmi_obj_ready.store(0);
            ret = timespec_get(&end_time, TIME_UTC);
            if (0 == ret)
            {
                fprintf(stderr, "[ERROR] Failed to Get Display End Time\n");
                goto err;
            }
            disp_proc_time = (timedifference_msec(start_time, end_time) * TIME_COEF);
            disp_time = (uint32_t)((timedifference_msec(disp_prev_time, end_time) * TIME_COEF));
            int idx = disp_cnt++ % SIZE_OF_ARRAY(array_disp_time);
            array_disp_time[idx] = disp_time;
            disp_prev_time = end_time;
#ifdef DEBUG_TIME_FLG
            /* Draw Disp Frame Rate on RGB image.*/
            int arraySum = std::accumulate(array_disp_time, array_disp_time + SIZE_OF_ARRAY(array_disp_time), 0);
            double arrayAvg = 1.0 * arraySum / SIZE_OF_ARRAY(array_disp_time);
            double disp_fps = 1.0 / arrayAvg * 1000.0;

            printf("Disp Proc Time            : %lf[ms]\n", disp_proc_time);
            printf("Disp Frame Rate           : %lf[fps]\n", disp_fps);
            printf("Dipslay ------------------------------ No. %d\n", disp_cnt);
#endif
        }
        usleep(WAIT_TIME); //wait 1 tick time
    } /*End Of Loop*/

/*Error Processing*/
err:
    /*Set Termination Request Semaphore To 0*/
    sem_trywait(&terminate_req_sem);
    goto hdmi_end;

hdmi_end:
    /*To terminate the loop in Capture Thread.*/
    hdmi_obj_ready.store(0);
    printf("Display Thread Terminated\n");
    pthread_exit(NULL);
}

/*****************************************
* Function Name : R_Kbhit_Thread
* Description   : Executes the Keyboard hit thread (checks if enter key is hit)
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Kbhit_Thread(void *threadid)
{
    /*Semaphore Variable*/
    int32_t kh_sem_check = 0;
    /*Variable to store the getchar() value*/
    int32_t c = 0;
    /*Variable for checking return value*/
    int8_t ret = 0;

    printf("Key Hit Thread Starting\n");

    printf("************************************************\n");
    printf("* Press ENTER key to quit. *\n");
    printf("************************************************\n");

    /*Set Standard Input to Non Blocking*/
    errno = 0;
    ret = fcntl(0, F_SETFL, O_NONBLOCK);
    if (-1 == ret)
    {
        fprintf(stderr, "[ERROR] Failed to run fctnl(): errno=%d\n", errno);
        goto err;
    }

    while(1)
    {
        /*Gets the Termination request semaphore value. If different then 1 Termination was requested*/
        /*Checks if sem_getvalue is executed wihtout issue*/
        errno = 0;
        ret = sem_getvalue(&terminate_req_sem, &kh_sem_check);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Semaphore Value: errno=%d\n", errno);
            goto err;
        }
        /*Checks the semaphore value*/
        if (1 != kh_sem_check)
        {
            goto key_hit_end;
        }

        c = getchar();
        if ('s' == c || 'S' == c)
        {
            inif.set_river_pix(river_area_count);
            inif.write_ini_Param();
            spdlog::info("******************** update river-pix = {} ", (int32_t)river_area_count);
            c = getchar();
        }else if ('d' == c || 'D' == c)
        {
            if (switch_time_display == 0){
                switch_time_display = 1;
            }else{
                switch_time_display = 0;
            }
            c = getchar();
        }else{
            if (EOF != c)
            {
                /* When key is pressed. */
                printf("key Detected.\n");
                goto err;
            }
            else
            {
                /* When nothing is pressed. */
                usleep(WAIT_TIME);
            }
        }
    }

/*Error Processing*/
err:
    /*Set Termination Request Semaphore to 0*/
    sem_trywait(&terminate_req_sem);
    goto key_hit_end;

key_hit_end:
    printf("Key Hit Thread Terminated\n");
    pthread_exit(NULL);
}

/*****************************************
* Function Name : R_Main_Process
* Description   : Runs the main process loop
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t R_Main_Process()
{
    /*Main Process Variables*/
    int8_t main_ret = 0;
    /*Semaphore Related*/
    int32_t sem_check = 0;
    /*Variable for checking return value*/
    int8_t ret = 0;

    printf("Main Loop Starts\n");
    while(1)
    {
        /*Gets the Termination request semaphore value. If different then 1 Termination was requested*/
        errno = 0;
        ret = sem_getvalue(&terminate_req_sem, &sem_check);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to get Semaphore Value: errno=%d\n", errno);
            goto err;
        }
        /*Checks the semaphore value*/
        if (1 != sem_check)
        {
            goto main_proc_end;
        }
        /*Wait for 1 TICK.*/
        usleep(WAIT_TIME);
    }

/*Error Processing*/
err:
    sem_trywait(&terminate_req_sem);
    main_ret = 1;
    goto main_proc_end;
/*Main Processing Termination*/
main_proc_end:
    printf("Main Process Terminated\n");
    return main_ret;
}

/*****************************************
* Function Name : get_drpai_start_addr
* Description   : Function to get the start address of DRPAImem.
* Arguments     : drpai_fd: DRP-AI file descriptor
* Return value  : If non-zero, DRP-AI memory start address.
*                 0 is failure.
******************************************/
uint64_t get_drpai_start_addr(int drpai_fd)
{
    int ret = 0;
    drpai_data_t drpai_data;

    errno = 0;

    /* Get DRP-AI Memory Area Address via DRP-AI Driver */
    ret = ioctl(drpai_fd , DRPAI_GET_DRPAI_AREA, &drpai_data);
    if (-1 == ret)
    {
        std::cerr << "[ERROR] Failed to get DRP-AI Memory Area : errno=" << errno << std::endl;
        return 0;
    }

    return drpai_data.address;
}

/*****************************************
* Function Name : set_drpai_freq
* Description   : Function to set the DRP and DRP-AI frequency.
* Arguments     : drpai_fd: DRP-AI file descriptor
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int set_drpai_freq(int drpai_fd)
{
    int ret = 0;
    uint32_t data;

    errno = 0;
    data = drp_max_freq;
    ret = ioctl(drpai_fd , DRPAI_SET_DRP_MAX_FREQ, &data);
    if (-1 == ret)
    {
        std::cerr << "[ERROR] Failed to set DRP Max Frequency : errno=" << errno << std::endl;
        return -1;
    }

    errno = 0;
    data = drpai_freq;
    ret = ioctl(drpai_fd , DRPAI_SET_DRPAI_FREQ, &data);
    if (-1 == ret)
    {
        std::cerr << "[ERROR] Failed to set DRP-AI Frequency : errno=" << errno << std::endl;
        return -1;
    }
    return 0;
}

/*****************************************
* Function Name : init_drpai
* Description   : Function to initialize DRP-AI.
* Arguments     : drpai_fd: DRP-AI file descriptor
* Return value  : If non-zero, DRP-AI memory start address.
*                 0 is failure.
******************************************/
uint64_t init_drpai(int drpai_fd)
{
    int ret = 0;
    uint64_t drpai_addr = 0;

    /*Get DRP-AI memory start address*/
    drpai_addr = get_drpai_start_addr(drpai_fd);
    if (drpai_addr == 0)
    {
        return 0;
    }

    /*Set DRP-AI frequency*/
    ret = set_drpai_freq(drpai_fd);
    if (ret != 0)
    {
        return 0;
    }

    return drpai_addr;
}

int32_t main(int32_t argc, char * argv[])
{
    /* Log File Setting */
    auto now = std::chrono::system_clock::now();
    auto tm_time = spdlog::details::os::localtime(std::chrono::system_clock::to_time_t(now));
    char date_buf[64];
    char time_buf[128];
    memset(time_buf,0,sizeof(time_buf));
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d_%H-%M-%S", &tm_time);
    sprintf(time_buf,"logs/%s_app_river_level_monitor_cam.log",date_buf);
    auto logger = spdlog::basic_logger_mt("logger", time_buf);
    spdlog::set_default_logger(logger);
    
    /* OpenCVA Disable Code */
    unsigned long OCA_list[16]; 
    for (int i=0; i < 16; i++) OCA_list[i] = 0; 
    OCA_Activate( &OCA_list[0] ); 
    
    /* DRP-AI Frequency Setting */
    if (2 <= argc)
    {
        drp_max_freq = atoi(argv[1]);
    }
    else
    {
        drp_max_freq = DRP_MAX_FREQ;
    }
    if (3 <= argc)
    {
        drpai_freq = atoi(argv[2]);
    }
    else
    {
        drpai_freq = DRPAI_FREQ;
    }

    int8_t main_proc = 0;
    int8_t ret = 0;
    int8_t ret_main = 0;
    /*Multithreading Variables*/
    int32_t create_thread_ai = -1;
    int32_t create_thread_key = -1;
    int32_t create_thread_capture = -1;
    int32_t create_thread_img = -1;
    int32_t create_thread_hdmi = -1;
    int32_t sem_create = -1;
    InOutDataType input_data_type;
    bool runtime_status = false;

    printf("RZ/V2H and RZ/V2N DRP-AI Sample Application\n");
    printf("Model : PyTorch DeepLabv3 | %s  \n", model_dir_deeplabv3.c_str());
#if (0) == INF_YOLOX_SKIP
    printf("Model : Megvii-Base Detection YOLOX | %s\n", model_dir_yolox.c_str());
#endif
    printf("Input : %s\n", INPUT_CAM_NAME);
    spdlog::info("************************************************");
    spdlog::info("  RZ/V2H and RZ/V2N DRP-AI Sample Application");
    spdlog::info("  Model : PyTorch DeepLabv3 with YOLOX | {} {}", model_dir_deeplabv3.c_str(),model_dir_yolox.c_str());
    spdlog::info("  Input : {}", INPUT_CAM_NAME);
    spdlog::info("************************************************");
    printf("Argument : <DRP0_max_freq_factor> = %d\n", drp_max_freq);
    printf("Argument : <AI-MAC_freq_factor> = %d\n", drpai_freq);

     uint64_t drpaimem_addr_start = 0;
    
    /* read ini file*/
    inif.read_ini_config();
    inif.read_ini_Param();
    spdlog::info("Ini file config: {}",INI_FILE_NAME_CONFIG);
    spdlog::info("  river-pix             : {} ", (int32_t)inif.get_river_pix());
    spdlog::info("  person-alert-per-rate : {} ", (int32_t)inif.get_person_alert_per_rate());
    spdlog::info("  caution-per-rate      : {} ", (int32_t)inif.get_caution_per_rate());
    spdlog::info("  warning-per-rate      : {} ", (int32_t)inif.get_warning_per_rate());
    spdlog::info("  hazard-per-rate       : {} ", (int32_t)inif.get_hazard_per_rate());
    
    /*DRP-AI Driver Open*/
    /*For YOLOX*/
    errno = 0;
    drpai_fd0 = open("/dev/drpai0", O_RDWR);
    if (0 > drpai_fd0)
    {
        fprintf(stderr, "[ERROR] Failed to open DRP-AI Driver: errno=%d\n", errno);
        return -1;
    }
    /*Initialzie DRP-AI (Get DRP-AI memory address and set DRP-AI frequency)*/
    drpaimem_addr_start = init_drpai(drpai_fd0);
    if (drpaimem_addr_start == 0)
    {
        goto end_close_drpai;
    }

#if (0) == INF_YOLOX_SKIP

/* PREPROC_MODE_YOLOX (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_YOLOX == 1 
    /*Load pre_dir object to DRP-AI */
    ret = preruntime.Load(pre_dir_yokox);
    if (0 < ret)
    {
        fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime Load().\n");
        goto end_close_drpai;
    }
#endif  /* PREPROC_MODE_YOLOX */

    runtime_status = runtime.LoadModel(model_dir_yolox, drpaimem_addr_start + DRPAI_MEM_OFFSET_YOLOX);
 
    if(!runtime_status)
    {
        fprintf(stderr, "[ERROR] Failed to load model.\n");
        goto end_close_drpai;
    }

    /*Get input data */
    input_data_type = runtime.GetInputDataType(0);
    if (InOutDataType::FLOAT32 == input_data_type)
    {
        /*Do nothing*/
    }
    else if (InOutDataType::FLOAT16 == input_data_type)
    {
        fprintf(stderr, "[ERROR] Input data type : FP16.\n");
        /*If your model input data type is FP16, use std::vector<uint16_t> for reading input data. */
        goto end_close_drpai;
    }
    else
    {
        fprintf(stderr, "[ERROR] Input data type : neither FP32 nor FP16.\n");
        goto end_close_drpai;
    }
#endif  /* INF_YOLOX_SKIP */

    /*For DeepLabv3*/
/* PREPROC_MODE_DEEPLABV3 (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_DEEPLABV3 == 1 
    /*Load pre_dir object to DRP-AI */
    ret = preruntime_2.Load(pre_dir_deeplabv3);
    if (0 < ret)
    {
        fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime_2 Load().\n");
        goto end_close_drpai;
    }
#endif  /* PREPROC_MODE_DEEPLABV3 */

#if (0) == INF_YOLOX_SKIP
    drpaimem_addr_start = drpaimem_addr_start + DRPAI_MEM_OFFSET_DEEPLABCV3;
#endif  /* INF_YOLOX_SKIP */

    runtime_status = runtime_2.LoadModel(model_dir_deeplabv3, drpaimem_addr_start);

    if(!runtime_status)
    {
        fprintf(stderr, "[ERROR] Failed to load model.\n");
        goto end_close_drpai;
      }

    /*Get input data */
    input_data_type = runtime_2.GetInputDataType(0);
    if (InOutDataType::FLOAT32 == input_data_type)
    {
        /*Do nothing*/
    }
    else if (InOutDataType::FLOAT16 == input_data_type)
    {
        fprintf(stderr, "[ERROR] Input data type : FP16.\n");
        /*If your model input data type is FP16, use std::vector<uint16_t> for reading input data. */
        goto end_close_drpai;
    }
    else
    {
        fprintf(stderr, "[ERROR] Input data type : neither FP32 nor FP16.\n");
        goto end_close_drpai;
    }

    /* Create Camera Instance */
    capture = new Camera();

    /* Init and Start Camera */
    ret = capture->start_camera();
    if (0 != ret)
    {
        fprintf(stderr, "[ERROR] Failed to initialize Camera.\n");
        delete capture;
        ret_main = ret;
        goto end_close_drpai;
    }

    /*Initialize Image object.*/
    ret = img.init(CAM_IMAGE_WIDTH, CAM_IMAGE_HEIGHT, CAM_IMAGE_CHANNEL_YUY2, IMAGE_OUTPUT_WIDTH, IMAGE_OUTPUT_HEIGHT, IMAGE_CHANNEL_BGRA, capture->wayland_buf->mem, capture->overlay_buf->mem,capture->capture_buf->mem);
    if (0 != ret)
    {
        fprintf(stderr, "[ERROR] Failed to initialize Image object.\n");
        ret_main = ret;
        goto end_close_camera;
    }

    /*Termination Request Semaphore Initialization*/
    /*Initialized value at 1.*/
    sem_create = sem_init(&terminate_req_sem, 0, 1);
    if (0 != sem_create)
    {
        fprintf(stderr, "[ERROR] Failed to Initialize Termination Request Semaphore.\n");
        ret_main = -1;
        goto end_threads;
    }

    /*Create Key Hit Thread*/
    create_thread_key = pthread_create(&kbhit_thread, NULL, R_Kbhit_Thread, NULL);
    if (0 != create_thread_key)
    {
        fprintf(stderr, "[ERROR] Failed to create Key Hit Thread.\n");
        ret_main = -1;
        goto end_threads;
    }

    /*Create Inference Thread*/
    create_thread_ai = pthread_create(&ai_inf_thread, NULL, R_Inf_Thread, NULL);
    if (0 != create_thread_ai)
    {
        sem_trywait(&terminate_req_sem);
        fprintf(stderr, "[ERROR] Failed to create AI Inference Thread.\n");
        ret_main = -1;
        goto end_threads;
    }

    /*Create Capture Thread*/
    create_thread_capture = pthread_create(&capture_thread, NULL, R_Capture_Thread, (void *) capture);
    if (0 != create_thread_capture)
    {
        sem_trywait(&terminate_req_sem);
        fprintf(stderr, "[ERROR] Failed to create Capture Thread.\n");
        ret_main = -1;
        goto end_threads;
    }

    /*Create Image Thread*/
    create_thread_img = pthread_create(&img_thread, NULL, R_Img_Thread, NULL);
    if(0 != create_thread_img)
    {
        sem_trywait(&terminate_req_sem);
        fprintf(stderr, "[ERROR] Failed to create Image Thread.\n");
        ret_main = -1;
        goto end_threads;
    }

    /*Create Display Thread*/
    create_thread_hdmi = pthread_create(&hdmi_thread, NULL, R_Display_Thread, NULL);
    if(0 != create_thread_hdmi)
    {
        sem_trywait(&terminate_req_sem);
        fprintf(stderr, "[ERROR] Failed to create Display Thread.\n");
        ret_main = -1;
        goto end_threads;
    }

    /*Main Processing*/
    main_proc = R_Main_Process();
    if (0 != main_proc)
    {
        fprintf(stderr, "[ERROR] Error during Main Process\n");
        ret_main = -1;
    }
    goto end_threads;

end_threads:
    if(0 == create_thread_hdmi)
    {
        ret = wait_join(&hdmi_thread, DISPLAY_THREAD_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit Display Thread on time.\n");
            ret_main = -1;
        }
    }
    if(0 == create_thread_img)
    {
        ret = wait_join(&img_thread, DISPLAY_THREAD_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit Image Thread on time.\n");
            ret_main = -1;
        }
    }
    if (0 == create_thread_capture)
    {
        ret = wait_join(&capture_thread, CAPTURE_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit Capture Thread on time.\n");
            ret_main = -1;
        }
    }
    if (0 == create_thread_ai)
    {
        ret = wait_join(&ai_inf_thread, AI_THREAD_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit AI Inference Thread on time.\n");
            ret_main = -1;
        }
    }
    if (0 == create_thread_key)
    {
        ret = wait_join(&kbhit_thread, KEY_THREAD_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit Key Hit Thread on time.\n");
            ret_main = -1;
        }
    }

    /*Delete Terminate Request Semaphore.*/
    if (0 == sem_create)
    {
        sem_destroy(&terminate_req_sem);
    }

    /* Exit waylad */
    wayland.exit();
    goto end_close_camera;

end_close_camera:
    /*Close USB Camera.*/
    ret = capture->close_camera();
    if (0 != ret)
    {
        fprintf(stderr, "[ERROR] Failed to close Camera.\n");
        ret_main = -1;
    }
    delete capture;
    goto end_close_drpai;

end_close_drpai:

    /*Close DRP-AI Driver.*/
    if (0 < drpai_fd0)
    {
        errno = 0;
        ret = close(drpai_fd0);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to close DRP-AI Driver: errno=%d\n", errno);
            ret_main = -1;
        }
    }
    if (0 < drpai_fd1)
    {
        errno = 0;
        ret = close(drpai_fd1);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to close DRP-AI Driver: errno=%d\n", errno);
            ret_main = -1;
        }
    }
    goto end_main;

end_main:
    printf("Application End\n");
    return ret_main;
}

