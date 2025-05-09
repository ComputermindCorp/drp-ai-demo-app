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
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera or Image
***********************************************************************************************************************/
/*****************************************
 * includes
 ******************************************/
#include <builtin_fp16.h>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "PreRuntime.h"
#include "MeraDrpRuntimeWrapper.h"

/*Definition of Macros & other variables*/
#include "define.h"
#include "image.h"
#include "capture.h"
#include "patchcore_proc.h"

using namespace cv;
using namespace std;

MeraDrpRuntimeWrapper runtime;
PreRuntime preruntime;
static Image cls_img;
static Capture cls_cap;
static PatchCoreProc cls_patchcore;
static mutex mtx;
static pthread_t capture_thread;
static pthread_t inference_thread;
int32_t create_thread_inference = -1;
/*Flags*/
static atomic<uint8_t> inference_start (0);

cv::Mat frame;
cv::Mat inf_img;
cv::Mat result_img;

bool exit_clicked       = false;
bool start_clicked      = false;
bool inf_flag           = false;
cv::Rect next_rect      = cv::Rect(925, 370, 45, 100);
cv::Rect prev_rect      = cv::Rect(50, 370, 45, 100);
cv::Rect start_rect     = cv::Rect((WIN_SIZE_WIDTH - 250) / 2, 688, 250, 60);
cv::Rect exit_rect      = cv::Rect(940, 30, 60, 35);

int drpai_fd = -1;
float drpai_output_buf[OUTPUT_FEATURE_SIZE];
uint64_t drpaimem_addr_start = 0;
double pre_time = 0.0;
double inf_time = 0.0;
double post_time = 0.0;
/*command line argument*/
int32_t app_mode;
char* file_path;
int32_t drpai_freq;

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
            uint16_t* data_ptr = reinterpret_cast<uint16_t*>(std::get<1>(output_buffer));
            for (int j = 0; j<output_size; j++)
            {
                /*FP16 to FP32 conversion*/
                drpai_output_buf[j + size_count] = float16_to_float32(data_ptr[j]);
            }
        }
        else if (InOutDataType::FLOAT32 == std::get<0>(output_buffer))
        {
            /*Output Data = std::get<1>(output_buffer)*/
            float* data_ptr = reinterpret_cast<float*>(std::get<1>(output_buffer));
            for (int j = 0; j<output_size; j++)
            {
                drpai_output_buf[j + size_count] = data_ptr[j];
            }
        }
        else
        {
            fprintf(stderr, "[ERROR] Output data type : not floating point.\n");
            ret = -1;
            break;
        }
        size_count += output_size;
    }
    return ret;
}

/*****************************************
* Function Name : R_Inf_Proc
* Description   : Executes the DRP-AI inference proc
* Arguments     : -
* Return value  : -
******************************************/
void R_Inf_Proc()
{
    /*Variable for Performance Measurement*/
    struct timespec start_time;
    struct timespec end_time;
    /*Variable for getting Inference output data*/
    void* output_ptr;
    uint32_t out_size;
    /*Variable for Pre-processing parameter configuration*/
    s_preproc_param_t in_param;
    float floatarr1[OUTPUT_LAYER_1_SIZE];
    float floatarr2[OUTPUT_LAYER_2_SIZE];
    int8_t ret = 0;

    /* get image */
    cv::Mat img = inf_img.clone();
    // uint8_t *temp_input = new uint8_t[img.rows * img.cols * 3];
    // memcpy(temp_input, img.data, img.rows * img.cols * 3);

    // in_param.pre_in_addr    = (uintptr_t)temp_input;
    in_param.pre_in_addr    = (uintptr_t)img.data;
    in_param.pre_in_shape_w = img.cols;
    in_param.pre_in_shape_h = img.rows;

    /* AI Pre-process */
    timespec_get(&start_time, TIME_UTC);
    ret = preruntime.Pre(&in_param, &output_ptr, &out_size);
    if (0 < ret)
    {
        fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime Pre()\n");
        return;
    }
    /* Set Pre-processing output to be inference input. */
    runtime.SetInput(0, (float*)output_ptr);
    timespec_get(&end_time, TIME_UTC);
    /*Pre-process Time Result*/
    pre_time = (timedifference_msec(start_time, end_time) * TIME_COEF);

    /* AI Inference */
    timespec_get(&start_time, TIME_UTC);
    runtime.Run();
    timespec_get(&end_time, TIME_UTC);
    /*Inference Time Result*/
    inf_time = (timedifference_msec(start_time, end_time) * TIME_COEF);
    
    /* Process to read the DRPAI output data. */
    timespec_get(&start_time, TIME_UTC);
    ret = get_result();
    if (0 != ret)
    {
        fprintf(stderr, "[ERROR] Failed to get result from memory.\n");
        return;
    }

    /* Preparation for Post-Processing */
    float* pd = drpai_output_buf;
    memcpy(floatarr1, pd, sizeof(float)*OUTPUT_LAYER_1_SIZE);
    pd += OUTPUT_LAYER_1_SIZE;
    memcpy(floatarr2, pd, sizeof(float)*OUTPUT_LAYER_2_SIZE);
    cls_patchcore.execute(floatarr1, floatarr2);

    timespec_get(&end_time, TIME_UTC);
    /*Post-process Time Result*/
    post_time = (timedifference_msec(start_time, end_time) * TIME_COEF);
    
    inf_flag = true;
}

/*****************************************
* Function Name : R_Inference_Thread
* Description   : Executes Inference with Inference thread.
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Inference_Thread(void *threadid)
{
    printf("Inference Thread Starting\n");

    while(1)
    {
        if(exit_clicked){
            break;
        }
        if (!inference_start.load()){
            continue;
        }
        R_Inf_Proc();

        inference_start.store(0);
        /*Wait for 1 TICK.*/
        usleep(WAIT_TIME);
    } /*End of Loop*/

    printf("Inference Thread Terminated\n");
    pthread_exit(NULL);
}

/*****************************************
 * Function Name : mouse_callback_button_click.
 * Description   : This is a mouse callback function that is triggered when a mouse button is clicked.
 * Arguments     : event: represents the mouse event (e.g., left button down, right button up)
 *                 x, y: the x and y coordinates of the mouse click.
 *                 flags: additional flags associated with the mouse event (e.g., control key pressed).
 *                 userdata: a pointer to user-defined data that can be used to pass additional information 
 *                 to the callback function.
 ******************************************/
void mouse_callback_button_click(int event, int x, int y, int flags, void *userdata)
{
    if (event == EVENT_LBUTTONDOWN)
    {       
        if (next_rect.contains(Point(x, y)))
        {
            std::cout << "clicked next button \n";
            cls_cap.set_idx_inc();
        }
        else if (prev_rect.contains(Point(x, y)))
        {
            std::cout << "clicked prev button \n";
            cls_cap.set_idx_dec();
        }
        else if (start_rect.contains(Point(x, y)))
        {
            std::cout << "clicked start button \n";
            if (app_mode == 0){
                /* inference proc start */
                inf_img = result_img.clone();
                R_Inf_Proc();
            }
            else{
                if(!start_clicked){
                    /*Create Inference Thread*/
                    create_thread_inference = pthread_create(&inference_thread, NULL, R_Inference_Thread, NULL);
                    if (0 != create_thread_inference)
                    {
                        fprintf(stderr, "[ERROR] Failed to create Inference Thread.\n");
                    }
                }
            }
            start_clicked = true;
        }
        else if (exit_rect.contains(Point(x, y)))
        {
            std::cout << "clicked exit button \n";
            exit_clicked = true;
        }
    }
}

/*****************************************
* Function Name : R_Capture_Thread
* Description   : Executes the V4L2 capture with Capture thread.
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Capture_Thread(void *threadid)
{
    uint8_t capture_stabe_cnt = 8;
    printf("Capture Thread Starting\n");

    while(1)
    {
        if(exit_clicked){
            break;
        }
        mtx.lock();
        frame = cls_cap.get_img(app_mode);
        mtx.unlock();

        if( app_mode != 0 && capture_stabe_cnt > 0 ){
            capture_stabe_cnt--;
        }
        else{
            mtx.lock();
            result_img = cls_cap.convert_inf_img(app_mode);
            if (app_mode != 0 && !inference_start.load()){
                inf_img = result_img.clone();
                inference_start.store(1);
            }
            mtx.unlock();
        }

        /*Wait for 1 TICK.*/
        usleep(WAIT_TIME);
    } /*End of Loop*/

    printf("Capture Thread Terminated\n");
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
    // int8_t ret = 0;
    printf("Main Loop Starts\n");
    namedWindow(app_name, WINDOW_NORMAL);
    resizeWindow(app_name, WIN_SIZE_WIDTH, WIN_SIZE_HEIGHT);
    while(waitKey(30) != 27)
    {
        if(exit_clicked){
            break;
        }
        /*Update screen*/
        cv::Mat main_sc = cls_img.create_main_sc(app_mode);
        mtx.lock();
        cv::Mat disp_img = cls_img.convert_to_disp_img(app_mode, frame.clone(), cls_cap.get_name());
        mtx.unlock();
        cls_img.paste(main_sc, disp_img.clone(), 30, 130, disp_img.cols, disp_img.rows);
        setMouseCallback(app_name, mouse_callback_button_click);
        cv::imshow(app_name, main_sc);

        if(start_clicked && inf_flag){
            mtx.lock();
            cv::Mat ret_img = cls_img.create_result_sc(
                result_img.clone(), 
                cls_patchcore.anomaly_map, 
                cls_patchcore.anomaly_score, 
                cls_patchcore.ng_threshold, 
                pre_time, 
                inf_time, 
                post_time);
            mtx.unlock();
            cv::imshow("Patch Core Demo Result", ret_img);
            inf_flag = false;
        }

        /*Wait for 1 TICK.*/
        usleep(WAIT_TIME);
    }
    return 0;
}

/*****************************************
* Function Name : get_drpai_start_addr
* Description   : Function to get the start address of DRPAImem.
* Arguments     : -
* Return value  : uint32_t = DRPAImem start address in 32-bit.
******************************************/
uint64_t get_drpai_start_addr()
{
    int ret = 0;
    drpai_data_t drpai_data;

    errno = 0;

    drpai_fd = open("/dev/drpai0", O_RDWR);
    if (0 > drpai_fd )
    {
        LOG(FATAL) << "[ERROR] Failed to open DRP-AI Driver : errno=" << errno;
        return (uint64_t)NULL;
    }

    /* Get DRP-AI Memory Area Address via DRP-AI Driver */
    ret = ioctl(drpai_fd , DRPAI_GET_DRPAI_AREA, &drpai_data);
    if (-1 == ret)
    {
        LOG(FATAL) << "[ERROR] Failed to get DRP-AI Memory Area : errno=" << errno ;
        return (uint64_t)NULL;
    }

    return drpai_data.address;
}

/*****************************************
* Function Name : init_drpai
* Description   : Function to initialize DRP-AI.
* Arguments     : -
* Return value  : If non-zero, DRP-AI memory start address.
*                 0 is failure.
******************************************/
int32_t init_drpai()
{
    int ret = 0;
    bool runtime_status = false; 
    InOutDataType input_data_type;

    /*Load model_dir structure and its weight to runtime object */
    /*Get DRP-AI memory start address*/
    // drpaimem_addr_start = 0;
    drpaimem_addr_start = get_drpai_start_addr();
    if (drpaimem_addr_start == 0)
    {
        /* Error notifications are output from function get_drpai_start_addr(). */
	    fprintf(stderr, "[ERROR] Failed to get DRP-AI memory area start address. \n");
        return -1;
    }

    /*Load pre_dir object to DRP-AI */
    ret = preruntime.Load(pre_dir);
    if (0 < ret)
    {
        fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime Load().\n");
        return -1;
    }

    runtime_status = runtime.LoadModel(model_dir, drpaimem_addr_start);
    if(!runtime_status)
    {
        fprintf(stderr, "[ERROR] Failed to load model.\n");
        return -1;
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
        return -1;
    }
    else
    {
        fprintf(stderr, "[ERROR] Input data type : neither FP32 nor FP16.\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int32_t ret = 0;
    int8_t main_proc = 0;
    int32_t create_thread_capture = -1;
    
    /* DRP-AI Frequency Setting */
    if (3 <= argc){
        app_mode = atoi(argv[1]);
        file_path = argv[2];
    }
    else{
        fprintf(stderr, "[ERROR] Few command line arguments\n");
        goto end_main;
    }

    if (4 <= argc){
        drpai_freq = atoi(argv[3]);
    }
    else{
        drpai_freq = DRPAI_FREQ;
    }

    printf("************************************************\n");
    printf("RZ/V2H DRP-AI Sample Application\n");
    printf("Model : PyTorch ResNet    | %s\n", model_dir.c_str());
    printf("Input : %s\n", INPUT_CAM_NAME);
    printf("************************************************\n");
    printf("Argument : <app_mode> = %d\n", app_mode);
    printf("Argument : <input_file_path> = %s\n", file_path);
    printf("Argument : <AI-MAC_freq_factor> = %d\n", drpai_freq);

    /* OpenCVA Disable Code */
    unsigned long OCA_list[16];
    for (int i=0; i < 16; i++) OCA_list[i] = 0;
    OCA_Activate( &OCA_list[0] );

    /*Init DRP-AI*/
    ret = init_drpai();
    if (0 < ret)
    {
        goto end_main;
    }

    /*Init PatchCore Proc*/
    ret = cls_patchcore.init(file_path);
    if (0 < ret)
    {
        fprintf(stderr, "[ERROR] Failed to open file\n");
        goto end_main;
    }

    /*Init Capture*/
    ret = cls_cap.init(app_mode, file_path);
    if (0 < ret)
    {
        goto end_main;
    }

    /*Create Capture Thread*/
    create_thread_capture = pthread_create(&capture_thread, NULL, R_Capture_Thread, NULL);
    if (0 != create_thread_capture)
    {
        fprintf(stderr, "[ERROR] Failed to create Capture Thread.\n");
        goto end_threads;
    }


    /*Main Processing*/
    main_proc = R_Main_Process();
    if (0 != main_proc)
    {
        fprintf(stderr, "[ERROR] Error during Main Process\n");
    }
    destroyAllWindows();
    goto end_threads;

end_threads:
    if (0 == create_thread_capture){
        ret = wait_join(&capture_thread, CAPTURE_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit Capture Thread on time.\n");
        }
    }
    if (app_mode != 0 && 0 == create_thread_inference){
        ret = wait_join(&inference_thread, CAPTURE_TIMEOUT);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to exit Inference Thread on time.\n");
        }
    }
    goto end_close_camera;
    
end_close_camera:
    if(app_mode != 0)
    {
        /*Close Camera.*/
        ret = cls_cap.close_camera();
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to close Camera.\n");
        }
    }
    goto end_close_drpai;

end_close_drpai:
    /*Close DRP-AI Driver.*/
    if (0 < drpai_fd)
    {
        errno = 0;
        ret = close(drpai_fd);
        if (0 != ret)
        {
            fprintf(stderr, "[ERROR] Failed to close DRP-AI Driver: errno=%d\n", errno);
        }
    }
    goto end_main;

end_main:
    printf("Application End\n");
    return 0;
}
