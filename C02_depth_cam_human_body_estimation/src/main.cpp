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
* Description  : RZ/V2H DRP-AI Sample Application for MMPose HRNet + Megvii-Base Detection YOLOX with Depth Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
/*DRPAI Driver Header*/
#include <linux/drpai.h>
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
/*Mutual exclusion*/
#include <mutex>
#include <optional>
#include <chrono>
#include <deque>
#include <opencv2/opencv.hpp>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

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
//static uint8_t postproc_data[HRNET_MODEL_IN_W * HRNET_MODEL_IN_H];
//static uint8_t postproc_data1[DRPAI_OUT_WIDTH * DRPAI_OUT_HEIGHT];

static float drpai_output_buf0[num_inf_out];
static float drpai_output_buf1[INF_OUT_SIZE];
//static float  drpai_output_buf1[HRNET_MODEL_IN_W * HRNET_MODEL_IN_H*HRNET_MODEL_OUT_NUM*(NUM_CLASS_DEEPLABV3-1)];
static uint64_t capture_address;
static uint8_t buf_id;
static Image img;
static std::optional<Camera::DepthData> depth_data;

/*AI Inference for DRPAI*/
/* DRP-AI TVM[*1] Runtime object */
MeraDrpRuntimeWrapper runtime;
MeraDrpRuntimeWrapper runtime_2;
/* Pre-processing Runtime object */
PreRuntime preruntime;
PreRuntime preruntime_2;

static int drpai_fd0 = -1;
static int drpai_fd1 = -1;
static drpai_handle_t *drpai_hdl0 = NULL;
static drpai_data_t drpai_data0;
static drpai_handle_t *drpai_hdl1 = NULL;
static drpai_data_t drpai_data1;
static double yolox_drpai_time = 0;
static double hrnet_drpai_time = 0;
#ifdef DISP_AI_FRAME_RATE
static double ai_fps = 0;
static double cap_fps = 0;
static double proc_time_capture = 0;
static uint32_t array_cap_time[30] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
#endif /* DISP_AI_FRAME_RATE */
static double yolox_proc_time = 0;
static double hrnet_proc_time = 0;
static uint32_t disp_time = 0;
static uint32_t array_drp_time[30] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
static uint32_t array_disp_time[30] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
static int32_t flip_mode;
static int32_t drpai_freq;
static uint32_t ai_time = 0;
static float hrnet_preds[NUM_OUTPUT_C][3];
static uint16_t id_x[NUM_OUTPUT_C];
static uint16_t id_y[NUM_OUTPUT_C];
static uint16_t id_x_local[NUM_OUTPUT_C]; /*To be used only in Inference Threads*/
static uint16_t id_y_local[NUM_OUTPUT_C]; /*To be used only in Inference Threads*/
static float id_x_depth;
static float id_y_depth;
static float id_z_depth;

static int16_t cropx;
static int16_t cropy;
static int16_t croph;
static int16_t cropw;
static float lowest_kpt_score;
static float lowest_kpt_score_local; /*To be used only in Inference Threads*/

class body_point {
    using T_I = std::vector<size_t>;

    const T_I i_from_;
    const T_I i_to_;

    static inline float w_c(float w) { return w; }
    static inline float h_c(float h) { return h; }
    static inline float sqr(float v) { return v * v; }

public:
    body_point(const T_I& i_from, const T_I& i_to)
        : i_from_(i_from), i_to_(i_to)
    {}
    body_point(size_t i_from, size_t i_to)
        : i_from_({ i_from, }), i_to_({ i_to, })
    {}

    /*****************************************
    * Function Name : calc_length
    * Description   : Calculate length between keypoints
    * Arguments     : -
    * Return value  : std::nullopt of null Z-depth found, otherwise length
    ******************************************/
    std::optional<float> calc_length(std::optional<float> dist = std::nullopt) const
    {
        static const auto get_depth = [&](std::array<float, 3>& ret, const T_I& is) -> bool {
            ret[0] = ret[1] = ret[2] = 0.0;

            for (size_t i = 0; i < is.size(); ++i) {
                auto x = static_cast<float>(id_x[is[i]]);
                auto y = static_cast<float>(id_y[is[i]]);

                auto p = depth_data->get_real_pos(w_c(x), h_c(y), dist);
                if (!p) { return false; }

                ret[0] += (*p)[0];
                ret[1] += (*p)[1];
                ret[2] += (*p)[2];
            }

            ret[0] /= is.size();
            ret[1] /= is.size();
            ret[2] /= is.size();

            return true;
        };

        if (!depth_data) { return std::nullopt; }

        std::array<float, 3> from;
        std::array<float, 3> to;

        if (!get_depth(from, i_from_)) { return std::nullopt; }
        if (!get_depth(to, i_to_)) { return std::nullopt; }

        return std::sqrt(sqr(from[0] - to[0]) + sqr(from[1] - to[1]) + sqr(from[2] - to[2]));
    }
};

class body_spec {
public:
    static constexpr size_t N = 30;
    using T_I = std::vector<body_point>;

    const std::string name_;

    std::deque<float> vs_;

    const T_I bs_;

    void put(float v)
    {
        if (vs_.size() >= N) { vs_.pop_front(); }
        vs_.push_back(v);
    }

public:
    body_spec(const std::string& name, const T_I& bs)
        : name_(name), bs_(bs)
    {}
    body_spec(const std::string& name, const body_point& b)
        : name_(name), bs_({ b, })
    {}

    /*****************************************
    * Function Name : calc_length
    * Description   : Calculate length between keypoints
    * Arguments     : dist = Z-dist of all keypoints
    * Return value  : -
    ******************************************/
    void calc_length(std::optional<float> dist = std::nullopt)
    {
        float length = 0.0;
        for (const auto& b : bs_) {
            auto l = b.calc_length(dist);
            if (!l) { return; }
            length += *l;
        }
        if (lowest_kpt_score > TH_KPT){
            put(length);
        }
    }

    /*****************************************
    * Function Name : mean
    * Description   : Get mean of all lengths
    * Arguments     : -
    * Return value  : std::nullopt if this instance hasn't enough data otherwise mean of lengths
    ******************************************/
    std::optional<float> mean() const
    {
        if (vs_.size() < N) { return std::nullopt; }

        return std::accumulate(vs_.cbegin(), vs_.cend(), 0.0) / vs_.size();
        //return vs_[vs_.size()-1];
    }

    const std::string& name() const { return name_; }

};

/* Correspondence between part of body and COCO keypoints */
static std::vector<body_spec> array_body_spec = {
    body_spec{ "Height", body_point{ { 0, }, { 15, 16, }, }, },
    body_spec{ "Head Width", body_point{ 3, 4, }, },
    body_spec{ "Shoulder Width", body_point{ 5, 6, }, },
    body_spec{ "Body Length", body_point{ { 5, 6, }, { 11, 12, }, }, },
    body_spec{ "Arm Length", {
        body_point{ 5, 7, },
        body_point{ 7, 9, },
    }, },
    body_spec{ "Leg Length", {
        body_point{ 11, 13, },
        body_point{ 13, 15, },
    }, },
};

/*YOLOX*/
static uint32_t bcount = 0;
static uint32_t ppl_count_local = 0; /*To be used only in Inference Threads*/
static uint32_t ppl_count = 0;
static vector<detection> det_res;
static int8_t display_state = 0;

static Wayland wayland;
static vector<detection> det;
static Camera* capture = NULL;

static double yolox_pre_time = 0;
static double yolox_post_time = 0;
static double yolox_ai_time = 0;
static double hrnet_pre_time = 0;
static double hrnet_post_time = 0;
static double hrnet_ai_time = 0;

/*Global frame */
cv::Mat g_frame;
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
 * Function Name : Get_Depth_Distance
 * Description   : Get Depth Distance
 * Arguments     : 
 *               : 
 * Return value  : 
 ******************************************/
static void Get_Depth_Distance()
{
    float x = CAM_IMAGE_WIDTH/2;
    float y = CAM_IMAGE_HEIGHT/2;

    auto p = depth_data->get_real_pos(x, y, std::nullopt);
    if (!p) { return; }

    id_x_depth = (*p)[0];
    id_y_depth = (*p)[1];
    id_z_depth = (*p)[2];
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
                                
                                Box bb = {center_x, center_y, box_w, box_h};
                                d = {bb, pred_class, probability};
                                det_buff.push_back(d);
                                count++;
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
        spdlog::info(" Bounding Box Number : {}",i+1);
        spdlog::info(" Bounding Box        : (X, Y, W, H) = ({}, {}, {}, {})", (int)det_buff[i].bbox.x, (int)det_buff[i].bbox.y, (int)det_buff[i].bbox.w, (int)det_buff[i].bbox.h);
        spdlog::info(" Detected Class      : {} (Class {})", label_file_map[det_buff[i].c].c_str(), det_buff[i].c);
        spdlog::info(" Probability         : {} %", (std::round((det_buff[i].prob*100) * 10) / 10));
        iBoxCount++;
    }
    spdlog::info(" Bounding Box Count  : {}", BoundingBoxCount);
    spdlog::info(" Person Count        : {}", iBoxCount);

    {
        std::unique_lock<std::mutex> lk(mtx);
        /* Clear the detected result list */
        det.clear();
        copy(det_buff.begin(), det_buff.end(), back_inserter(det));
    }

    return ;
}

/*****************************************
* Function Name : people_counter
* Description   : Function to pick a person who have maximum probability
* Arguments     : det = detected boxes details
*                 box_count = total number of boxes
* Return value  : -1 if no person who is probability >0 else index of det
******************************************/
int64_t pick_person(const vector<detection>& det, uint32_t box_count)
{
    std::unique_lock<std::mutex> lk(mtx);

    int64_t found = -1;
    float prob_max = 0;
    for(uint32_t i = 0; i < box_count; i++)
    {
        if(prob_max >= det[i].prob) {
            continue;
        }
        else {
            found = static_cast<int64_t>(i);
            prob_max = det[i].prob;
        }
    }

    return found;
}

/*****************************************
* Function Name : offset_hrnet
* Description   : Get the offset number to access the HRNet attributes
* Arguments     : b = Number to indicate which region [0~17]
*                 y = Number to indicate which region [0~64]
*                 x = Number to indicate which region [0~48]
* Return value  : offset to access the HRNet attributes.
*******************************************/
static int32_t offset_hrnet(int32_t b, int32_t y, int32_t x)
{
    return b * NUM_OUTPUT_W * NUM_OUTPUT_H + y * NUM_OUTPUT_W + x;
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
* Function Name : R_Post_Proc_HRNet
* Description   : CPU post-processing for HRNet
*                 Microsoft COCO: Common Objects in Context' ECCV'2014
*                 More details can be found in the `paper
*                 <https://arxiv.org/abs/1405.0312>
*                 COCO Keypoint Indexes:
*                 0: 'nose',
*                 1: 'left_eye',
*                 2: 'right_eye',
*                 3: 'left_ear',
*                 4: 'right_ear',
*                 5: 'left_shoulder',
*                 6: 'right_shoulder',
*                 7: 'left_elbow',
*                 8: 'right_elbow',
*                 9: 'left_wrist',
*                 10: 'right_wrist',
*                 11: 'left_hip',
*                 12: 'right_hip',
*                 13: 'left_knee',
*                 14: 'right_knee',
*                 15: 'left_ankle',
*                 16: 'right_ankle'
* Arguments     : floatarr = drpai output address
*                 n_pers = number of the person detected
* Return value  : -
******************************************/
static void R_Post_Proc_HRNet(float* floatarr)
{
    std::unique_lock<std::mutex> lk(mtx);

    float score;
    int32_t b = 0;
    int32_t y = 0;
    int32_t x = 0;
    int32_t offs = 0;

    float center[] = {(float)(cropw / 2 -1), (float)(croph / 2 - 1)};
    int8_t ind_x = -1;
    int8_t ind_y = -1;
    float max_val = -1;
    float scale_x = 0;
    float scale_y = 0;
    float coords_x = 0;
    float coords_y = 0;
    float diff_x;
    float diff_y;
    int8_t i;

    for(b = 0; b < NUM_OUTPUT_C; b++)
    {
        float scale[] = {(float)(cropw / 200.0), (float)(croph / 200.0)};
        ind_x = -1;
        ind_y = -1;
        max_val = -1;
        for(y = 0; y < NUM_OUTPUT_H; y++)
        {
            for(x = 0; x < NUM_OUTPUT_W; x++)
            {
                offs = offset_hrnet(b, y, x);
                if (max_val < floatarr[offs])
                {
                    /*Update the maximum value and indices*/
                    max_val = floatarr[offs];
                    ind_x = x;
                    ind_y = y;
                }
            }
        }
        if (0 > max_val)
        {
            ind_x = -1;
            ind_y = -1;
            lowest_kpt_score_local = 0;
            return;
        }
        hrnet_preds[b][0] = float(ind_x);
        hrnet_preds[b][1] = float(ind_y);
        hrnet_preds[b][2] = max_val;
        offs = offset_hrnet(b, ind_y, ind_x);
        if ((ind_y > 1) && (ind_y < NUM_OUTPUT_H -1))
        {
            if ((ind_x > 1) && (ind_x < (NUM_OUTPUT_W -1)))
            {
                diff_x = floatarr[offs + 1] - floatarr[offs - 1];
                diff_y = floatarr[offs + NUM_OUTPUT_W] - floatarr[offs - NUM_OUTPUT_W];
                hrnet_preds[b][0] += sign(diff_x) * 0.25;
                hrnet_preds[b][1] += sign(diff_y) * 0.25;
            }
        }

        /*transform_preds*/
        scale[0] *= 200;
        scale[1] *= 200;
        //udp (Unbiased Data Processing) = False
        scale_x = scale[0] / (NUM_OUTPUT_W);
        scale_y = scale[1] / (NUM_OUTPUT_H);
        coords_x = hrnet_preds[b][0];
        coords_y = hrnet_preds[b][1];
        hrnet_preds[b][0] = (coords_x * scale_x) + center[0] - (scale[0] * 0.5);
        hrnet_preds[b][1] = (coords_y * scale_y) + center[1] - (scale[1] * 0.5);
    }
    /* Clear the score in preparation for the update. */
    lowest_kpt_score_local = 0;
    score = 1;
    for (i = 0; i < NUM_OUTPUT_C; i++)
    {
        /* Adopt the lowest score. */
        if (hrnet_preds[i][2] < score)
        {
            score = hrnet_preds[i][2];
        }
    }
    /* Update the score for display thread. */
    lowest_kpt_score_local = score;
    /* HRnet Logout. */
    spdlog::info("HRNet Result-------------------------------------");
    for (i = 0; i < NUM_OUTPUT_C; i++)
    {
        spdlog::info("  ID {}: ({}, {}): {}%", i, (std::round((hrnet_preds[i][0]) * 100) / 100), (std::round((hrnet_preds[i][1]) * 100) / 100), (std::round((hrnet_preds[i][2]*100) * 10) / 10));
    }
}

/*****************************************
* Function Name : R_HRNet_Coord_Convert
* Description   : Convert the post processing result into drawable coordinates
* Arguments     : -
* Return value  : -
******************************************/
static void R_HRNet_Coord_Convert()
{
    /* Render skeleton on image and print their details */
    int32_t posx;
    int32_t posy;
    int8_t i;

    std::unique_lock<std::mutex> lk(mtx);

    for (i = 0; i < NUM_OUTPUT_C; i++)
    {
#if (0) == INF_YOLOX_SKIP
        /* Conversion from input image coordinates to display image coordinates. */
        /* +0.5 is for rounding.*/
        posx = (int32_t)(hrnet_preds[i][0] + 0.5) + cropx + OUTPUT_ADJ_X;
        posy = (int32_t)(hrnet_preds[i][1] + 0.5) + cropy + OUTPUT_ADJ_Y;
        /* Make sure the coordinates are not off the screen. */
        posx = (posx < 0) ? 0 : posx;
        posx = (posx > IMREAD_IMAGE_WIDTH - KEY_POINT_SIZE -1 ) ? IMREAD_IMAGE_WIDTH -KEY_POINT_SIZE -1 : posx;
        posy = (posy < 0) ? 0 : posy;
        posy = (posy > IMREAD_IMAGE_HEIGHT -KEY_POINT_SIZE -1) ? IMREAD_IMAGE_HEIGHT -KEY_POINT_SIZE -1 : posy;
#else
        /* Conversion from input image coordinates to display image coordinates. */
        /* +0.5 is for rounding.                                                 */
        posx = (int32_t)(hrnet_preds[i][0] / CROPPED_IMAGE_WIDTH  * CROPPED_IMAGE_WIDTH  + 0.5) + OUTPUT_LEFT + OUTPUT_ADJ_X;
        posy = (int32_t)(hrnet_preds[i][1] / CROPPED_IMAGE_HEIGHT * CROPPED_IMAGE_HEIGHT + 0.5) + OUTPUT_TOP  + OUTPUT_ADJ_Y;
        /* Make sure the coordinates are not off the screen. */
        posx    = (posx < OUTPUT_LEFT) ? OUTPUT_LEFT : posx;
        posy    = (posy < OUTPUT_TOP)  ? OUTPUT_TOP  : posy;
        posx = (posx > OUTPUT_LEFT + CROPPED_IMAGE_WIDTH  - 1) ? (OUTPUT_LEFT + CROPPED_IMAGE_WIDTH   - 1) : posx;
        posy = (posy > OUTPUT_TOP  + CROPPED_IMAGE_HEIGHT - 1) ? (OUTPUT_TOP  + CROPPED_IMAGE_HEIGHT  - 1) : posy;
#endif
        id_x_local[i] = posx;
        id_y_local[i] = posy;
    }

    return;
}

inline float mean(float a, float b)
{
    return a / 2 + b / 2;
}

inline float sqr(float x)
{
    return x * x;
}

template<typename T>
inline bool in_range(T x, T min, T max)
{
    return x > min && x < max;
}

/*****************************************
* Function Name : draw_skeleton
* Description   : Draw Complete Skeleton on image.
* Arguments     : -
* Return value  : -
******************************************/
static void draw_skeleton()
{
    int32_t sk_id;
    uint8_t v;
    uint8_t i;
    float   thre_kpt = TH_KPT;

    std::unique_lock<std::mutex> lk(mtx);

    /*Check If All Key Points Were Detected: If Over Threshold, It will Draw Complete Skeleton*/
    if (lowest_kpt_score > thre_kpt)
    {
        /* Draw limb */
        for (sk_id = 0; sk_id < NUM_LIMB; sk_id++)
        {
            uint8_t sk[] = {skeleton[sk_id][0], skeleton[sk_id][1]};
            int pos1[] = {id_x[sk[0]], id_y[sk[0]]};
            int pos2[] = {id_x[sk[1]], id_y[sk[1]]};
            
            if ((0 < pos1[0]) && (MIPI_WIDTH > pos1[0])
                && (0 < pos1[1]) && (MIPI_WIDTH > pos1[1]))
            {
                if ((0 < pos2[0]) && (MIPI_WIDTH > pos2[0])
                    && (0 < pos2[1]) && (MIPI_WIDTH > pos2[1]))
                {
                    img.draw_line2(pos1[0], pos1[1], pos2[0],pos2[1], YELLOW_DATA);
                }
            }
        }

        /*Draw Rectangle As Key Points*/
        for(v = 0; v < NUM_OUTPUT_C; v++)
        {
            /*Draw Rectangles On Each Skeleton Key Points*/
            img.draw_rect(id_x[v], id_y[v], KEY_POINT_SIZE, KEY_POINT_SIZE, RED_DATA);
            img.draw_rect(id_x[v], id_y[v], KEY_POINT_SIZE+1, KEY_POINT_SIZE+1, RED_DATA);
        }
    }

    return;
}

/*****************************************
* Function Name : draw_bounding_box
* Description   : Draw bounding box on image.
* Arguments     : -
* Return value  : 0 if succeeded
*               not 0 otherwise
******************************************/
void draw_bounding_box()
{
    vector<detection> det_buff;
    stringstream stream;
    string result_str;
    int32_t i = 0, found = -1;
    float current_prob = 0;
    uint32_t color=0;
 
    mtx.lock();
    copy(det_res.begin(), det_res.end(), back_inserter(det_buff));
    mtx.unlock();


    /* Draw bounding box on RGB image. */
    for (i = 0; i < det_buff.size(); i++)
    {
        /* Skip the overlapped bounding boxes */
        if (det_buff[i].prob <= current_prob) continue;
        found = i;
    }

    if (found != -1) {
        color = box_color[det_buff[found].c];
        /* Clear string stream for bounding box labels */
        stream.str("");
        /* Draw the bounding box on the image */
        stream << fixed << setprecision(2) << det_buff[found].prob;
        result_str = label_file_map[det_buff[found].c]+ " "+ stream.str();
        img.draw_rect_box((int)det_buff[found].bbox.x, (int)det_buff[found].bbox.y,
            (int)det_buff[found].bbox.w, (int)det_buff[found].bbox.h, result_str.c_str(),color);
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

    int32_t index = 0;
    stringstream stream;
    string str = "";
    string DispStr = "";

    double disp_pre_time = yolox_pre_time + hrnet_pre_time;
    double disp_ai_time = yolox_ai_time + hrnet_ai_time;
    double disp_post_time = yolox_post_time + hrnet_post_time;
    double disp_total_time = disp_pre_time  + disp_ai_time + disp_post_time;
    
    index=0;
    /* Draw Total Time Result on RGB image.*/
    stream.str("");
    stream << "Total AI Time (HRNET) : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(disp_total_time * 10) / 10 << "msec";
    str = stream.str();
    index++;
    img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    
    if (switch_time_display == 1){
#if (0) == INF_YOLOX_SKIP
        /* Draw PreProcess Time on RGB image.*/
        stream.str("");
        stream << "YOLOX DRP-AI TVM Pre-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(yolox_pre_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    
        /* Draw Inference Time on RGB image.*/
        stream.str("");
        stream << "DRP-AI TVM (Inference + Data loading) : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(yolox_ai_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    
        /* Draw PostProcess Time on RGB image.*/
        stream.str("");
        stream << "CPU Post-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(yolox_post_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
#endif
    
        /* Draw PreProcess Time on RGB image.*/
        stream.str("");
        stream << "HRNet DRP-AI TVM Pre-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(hrnet_pre_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);

        /* Draw Inference Time on RGB image.*/
        stream.str("");
        stream << "DRP-AI TVM (Inference + Data loading) : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(hrnet_ai_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);

        /* Draw PostProcess Time on RGB image.*/
        stream.str("");
        stream << "CPU Post-Processing : " << std::setw(4) << std::fixed << std::setprecision(1) << std::round(hrnet_post_time * 10) / 10 << "msec";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);

#ifdef DISP_AI_FRAME_RATE
        /* Draw AI/Camera Frame Rate on RGB image.*/
        stream.str("");
        stream << "AI/Camera Frame Rates : " << std::setw(3) << (uint32_t)ai_fps << "/" << (uint32_t)cap_fps << "fps";
        str = stream.str();
        index++;
        img->write_string_rgb(str, 2, TEXT_WIDTH_OFFSET_R, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
#endif /* DISP_AI_FRAME_RATE */
    }
    
    index = 1;
    for (const auto& spec : array_body_spec) {
        auto m = spec.mean();
        if (!m) { continue; }

        stream.str("");
        stream << spec.name() << ": " << std::setw(3) << ((*m) * 100) << "[cm]";
        str = stream.str();

        img->write_string_rgb(str, 1, TEXT_WIDTH_OFFSET_L, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
        ++index;
    }
#ifdef DISTANCE_TO_TARGET_DISP
    stream.str("");
    stream << "Distance to target : " << std::setw(3) << (id_z_depth * 100) << "[cm]";
    str = stream.str();
    img->write_string_rgb(str, 1, TEXT_WIDTH_OFFSET_L, LINE_HEIGHT_OFFSET + (LINE_HEIGHT * index), CHAR_SCALE_LARGE, WHITE_DATA);
    spdlog::info("Distance to target :  {} [cm]", (id_z_depth * 100));
#endif /* DISTANCE_TO_TARGET_DISP */

#ifdef DEBUG_TIME_FLG
    end = chrono::system_clock::now();
    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0);
    printf("Draw Text Time            : %lf[ms]\n", time);
#endif // DEBUG_TIME_FLG

    return 0;
}

/*****************************************
* Function Name : convert_format
* Description   : Convert YUYV image to BGRA format
* Arguments     : -
* Return value  : -
******************************************/
void convert_format_Mat(uint8_t* imgbuffer)
{
	
#ifdef DEBUG_TIME_FLG
    using namespace std;
    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
#endif // DEBUG_TIME_FLG
	uint8_t* img_buffer_rgb;
    int img_w = CAM_IMAGE_WIDTH;
    int img_h = CAM_IMAGE_HEIGHT;
    int img_c = IMAGE_CHANNEL_BGRA;
    cv::Mat yuyv_image(img_h, img_w, CV_8UC2, imgbuffer);
    cv::Mat bgra_image;
    cv::cvtColor(yuyv_image, bgra_image, cv::COLOR_YUV2BGRA_YUYV);
    g_frame = bgra_image;
#ifdef DEBUG_TIME_FLG
    end = chrono::system_clock::now();
    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0);
    printf("Convert Mat Time : %lf[ms]\n", time);
#endif // DEBUG_TIME_FLG
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
    double total_hrnet_drpai_time = 0;
    double total_hrnet_proc_time = 0;
    timespec yolox_sta_time;
    timespec yolox_end_time;
    static struct timespec yolox_drp_start_time;
    static struct timespec yolox_drp_end_time;
    timespec hrnet_sta_time;
    timespec hrnet_end_time;
    static struct timespec hrnet_drp_start_time;
    static struct timespec hrnet_drp_end_time;

    static struct timespec inf_start_time;
    static struct timespec inf_end_time;
    static struct timespec yolox_run_start_time;
    static struct timespec yolox_run_end_time;
    static struct timespec yolox_pre_start_time;
    static struct timespec yolox_pre_end_time;
    static struct timespec yolox_post_start_time;
    static struct timespec yolox_post_end_time;
    static struct timespec hrnet_run_start_time;
    static struct timespec hrnet_run_end_time;
    static struct timespec hrnet_pre_start_time;
    static struct timespec hrnet_pre_end_time;
    static struct timespec hrnet_post_start_time;
    static struct timespec hrnet_post_end_time;
    static struct timespec drp_prev_time = { .tv_sec = 0, .tv_nsec = 0, };
    /*HRNet Modify Parameters*/
    drpai_crop_t crop_param;
    static string drpai_param_file;
    uint32_t drp_param_info_size;
    uint8_t i;
    int64_t det_ppl_1 = 0;

    printf("Inference Thread Starting\n");
    
    in_param.pre_in_shape_w = CAM_IMAGE_WIDTH;
    in_param.pre_in_shape_h = CAM_IMAGE_HEIGHT;
    in_param_2.pre_in_shape_w = CAM_IMAGE_WIDTH;
    in_param_2.pre_in_shape_h = CAM_IMAGE_HEIGHT;
    
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
        ppl_count_local = 0;

#if (0) == INF_YOLOX_SKIP
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
        cv::resize(g_frame, frame1, size);

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

        runtime.Run(drpai_freq);
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
        //people_counter(det_res, det_ppl, bcount, &ppl_count_local);
        det_ppl_1 = pick_person(det_res, bcount);
        ppl_count_local = (det_ppl_1 != -1) ? 1 : 0;

        /* R_Post_Proc time end*/
        ret = timespec_get(&yolox_post_end_time, TIME_UTC);
        if (0 == ret)
        {
            fprintf(stderr, "[ERROR] Failed to Get R_Post_Proc End Time\n");
            goto err;
        }
        yolox_post_time = (timedifference_msec(yolox_post_start_time, yolox_post_end_time)*TIME_COEF);

#else
        det_ppl_1 = 0;
        ppl_count_local = 1;  /* YOLOX Skip*/
#endif  /* INF_YOLOX_SKIP */

        while(1)
        {

            /*If Person is detected run HRNet for Pose Estimation three times*/
            if(det_ppl_1 != -1)
            {
#if (0) == INF_YOLOX_SKIP
                croph = det_res[det_ppl_1].bbox.h + CROP_ADJ_X;
                cropw = det_res[det_ppl_1].bbox.w + CROP_ADJ_Y;
#else
                /* YOLOX Skip*/
                croph = CROPPED_IMAGE_HEIGHT;
                cropw = CROPPED_IMAGE_WIDTH;
#endif  /* INF_YOLOX_SKIP */
                /*Checks that cropping height and width does not exceeds image dimension*/
                if(croph < 1)
                {
                    croph = 1;
                }
                else if(croph > IMREAD_IMAGE_HEIGHT)
                {
                    croph = IMREAD_IMAGE_HEIGHT;
                }
                else
                {
                    /*Do Nothing*/
                }
                if(cropw < 1)
                {
                    cropw = 1;
                }
                else if(cropw > IMREAD_IMAGE_WIDTH)
                {
                    cropw = IMREAD_IMAGE_WIDTH;
                }
                else
                {
                    /*Do Nothing*/
                }
#if (0) == INF_YOLOX_SKIP
                /*Compute Cropping Y Position based on Detection Result*/
                /*If Negative Cropping Position*/
                if(det_res[det_ppl_1].bbox.y < (croph/2))
                {
                    cropy = 0;
                }
                else if(det_res[det_ppl_1].bbox.y > (IMREAD_IMAGE_HEIGHT-croph/2)) /*If Exceeds Image Area*/
                {
                    cropy = IMREAD_IMAGE_HEIGHT-croph;
                }
                else
                {
                    cropy = (int16_t)det_res[det_ppl_1].bbox.y - croph/2;
                }
                /*Compute Cropping X Position based on Detection Result*/
                /*If Negative Cropping Position*/
                if(det_res[det_ppl_1].bbox.x < (cropw/2))
                {
                    cropx = 0;
                }
                else if(det_res[det_ppl_1].bbox.x > (IMREAD_IMAGE_WIDTH-cropw/2)) /*If Exceeds Image Area*/
                {
                    cropx = IMREAD_IMAGE_WIDTH-cropw;
                }
                else
                {
                    cropx = (int16_t)det_res[det_ppl_1].bbox.x - cropw/2;
                }
#else
                cropx = OUTPUT_LEFT;
                cropy = 0;
#endif
                /*Checks that combined cropping position with width and height does not exceed the image dimension*/
                if(cropx + cropw > IMREAD_IMAGE_WIDTH)
                {
                    cropw = IMREAD_IMAGE_WIDTH - cropx;
                }
                if(cropy + croph > IMREAD_IMAGE_HEIGHT)
                {
                    croph = IMREAD_IMAGE_HEIGHT - cropy;
                }
                /*Change HRNet Crop Parameters*/
                crop_param.img_owidth = (uint16_t)cropw;
                crop_param.img_oheight = (uint16_t)croph;
                crop_param.pos_x = (uint16_t)cropx;
                crop_param.pos_y = (uint16_t)cropy;
                crop_param.obj.address = drpai_hdl1->drpai_address.drp_param_addr;
                crop_param.obj.size = drpai_hdl1->drpai_address.drp_param_size;
                

                //----------------------------------------------------------------------------------------------
                // HRNET
                //----------------------------------------------------------------------------------------------
                in_param_2.pre_in_addr    = (uintptr_t) capture_address;
                in_param_2.pre_in_shape_w = CAM_IMAGE_WIDTH;
                in_param_2.pre_in_shape_h = CAM_IMAGE_HEIGHT;
                /*Gets Pre-process starting time*/
                ret = timespec_get(&hrnet_pre_start_time, TIME_UTC);
                if (0 == ret)
                {
                    fprintf(stderr, "[ERROR] Failed to get Pre-process Start Time\n");
                    goto err;
                }
/* PREPROC_MODE_HRNET (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_HRNET == 1 
                ret = preruntime_2.Pre(&in_param_2, &output_ptr2, &out_size);
                if (0 < ret)
                {
                    fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime_2 Pre()\n");
                    goto err;
                }
#else
                // hrnet
                cv::Mat cropped_image = g_frame;
                cv::Mat frame2;
                cv::Size size2(HRNET_MODEL_IN_W, HRNET_MODEL_IN_H);
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

                cv::Mat frame_hrnet = frameCHWres;

#endif  /* PREPROC_MODE_HRNET */

                
                /*Set Pre-processing output to be inference input. */
/* PREPROC_MODE_HRNET (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_HRNET == 1 
                runtime_2.SetInput(0, (float*)output_ptr2);
#else
                runtime_2.SetInput(0, frame_hrnet.ptr<float>());
#endif  /* PREPROC_MODE_HRNET */

                /*Gets AI Pre-process End Time*/
                ret = timespec_get(&hrnet_pre_end_time, TIME_UTC);
                if ( 0 == ret)
                {
                    fprintf(stderr, "[ERROR] Failed to Get Pre-process End Time\n");
                    goto err;
                }
                
                /*Pre-process Time Result*/
                hrnet_pre_time = (timedifference_msec(hrnet_pre_start_time, hrnet_pre_end_time) * TIME_COEF);

                /*Gets inference starting time*/
                ret = timespec_get(&hrnet_run_start_time, TIME_UTC);
                if (0 == ret)
                {
                    fprintf(stderr, "[ERROR] Failed to get Inference Start Time\n");
                    goto err;
                }

                runtime_2.Run(drpai_freq);
                /*Gets AI Inference End Time*/
                ret = timespec_get(&hrnet_run_end_time, TIME_UTC);
                if ( 0 == ret)
                {
                    fprintf(stderr, "[ERROR] Failed to Get Inference End Time\n");
                    goto err;
                }
                /*Inference Time Result*/
                hrnet_ai_time = (timedifference_msec(hrnet_run_start_time, hrnet_run_end_time) * TIME_COEF);

                /*Gets Post-process starting time*/
                ret = timespec_get(&hrnet_post_start_time, TIME_UTC);
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
                //R_Post_Proc_DeepLabV3(drpai_output_buf1);
                /*CPU Post Processing For HRNet & Display the Results*/
                R_Post_Proc_HRNet(&drpai_output_buf1[0]);

                if(lowest_kpt_score_local > 0)
                {
                    R_HRNet_Coord_Convert();
                }

                /* R_Post_Proc time end*/
                ret = timespec_get(&hrnet_post_end_time, TIME_UTC);
                if (0 == ret)
                {
                    fprintf(stderr, "[ERROR] Failed to Get R_Post_Proc End Time\n");
                    goto err;
                }
                hrnet_post_time = (timedifference_msec(hrnet_post_start_time, hrnet_post_end_time)*TIME_COEF);
                
            }
            else {
                lowest_kpt_score_local = 0;
            }

            /*Copy data for Display Thread*/
            lowest_kpt_score = lowest_kpt_score_local;
            memcpy(id_x, id_x_local, sizeof(id_x_local));
            memcpy(id_y, id_y_local,sizeof(id_y_local));
            ppl_count = ppl_count_local;

            break;
        }

#if (0) == INF_YOLOX_SKIP
        /*Display Processing YOLOX Time On Log File*/
        yolox_drpai_time = (yolox_pre_time + yolox_ai_time + yolox_post_time);
        spdlog::info("YOLOX");
        spdlog::info(" DRP-AI TVM Pre-Processing :  {} [ms]", std::round(yolox_pre_time  * 10) / 10);
        spdlog::info(" DRP-AI TVM (Inference + Data loading) : {} [ms]", std::round(yolox_ai_time * 10) / 10);
        spdlog::info(" CPU Post-Processing : {} [ms]", std::round(yolox_post_time * 10) / 10);
#endif

        /*Display Processing HRNet Time On Log File*/
        /*Display Processing Time On Console*/        
        hrnet_drpai_time = (hrnet_pre_time + hrnet_ai_time + hrnet_post_time);
        spdlog::info("HRNet");
        spdlog::info(" DRP-AI TVM Pre-Processing :  {} [ms]", std::round(hrnet_pre_time  * 10) / 10);
        spdlog::info(" DRP-AI TVM (Inference + Data loading) : {} [ms]", std::round(hrnet_ai_time * 10) / 10);
        spdlog::info(" CPU Post-Processing : {} [ms]", std::round(hrnet_post_time * 10) / 10);
        
        /*Display Processing Frame Rate On Log File*/
        ai_time = (uint32_t)((timedifference_msec(drp_prev_time, hrnet_post_end_time) * TIME_COEF));
        int idx = inf_cnt % SIZE_OF_ARRAY(array_drp_time);
        array_drp_time[idx] = ai_time;
        drp_prev_time = hrnet_post_end_time;
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
    int32_t _cap_offset;
    int32_t _img_offset;
    int8_t ret = 0;
    int32_t counter = 0;
    uint8_t * img_buffer0;
    uint8_t capture_stabe_cnt = 8;  // Counter to wait for the camera to stabilize
    int32_t cap_cnt = -1;
#ifdef DISP_AI_FRAME_RATE
    static struct timespec capture_time;
    static struct timespec capture_time_prev = { .tv_sec = 0, .tv_nsec = 0, };
#endif /* DISP_AI_FRAME_RATE */

#if (0) == INPUT_CAM_TYPE
    double elapsed_time_last_disp = 0;
    double target_disp_fps = 15.0;
#endif

    printf("Capture Thread Starting\n");

    img_buffer0 = (uint8_t *)capture->drpai_buf->mem;
    if (MAP_FAILED == img_buffer0)
    {
        fprintf(stderr, "[ERROR] Failed to mmap\n");
        goto err;
    }
#if (1) == DRPAI_INPUT_PADDING
    /** Fill buffer with the brightness 114. */
    for( uint32_t i = 0; i < CAM_IMAGE_WIDTH * CAM_IMAGE_WIDTH * CAM_IMAGE_CHANNEL_YUY2; i += 4 )
    {
        /// Y =  0.299R + 0.587G + 0.114B
        img_buffer0[i]   = 114;    
        img_buffer0[i+2] = 114;
        /// U = -0.169R - 0.331G + 0.500B + 128
        img_buffer0[i+1] = 128;
        /// V =  0.500R - 0.419G - 0.081B + 128
        img_buffer0[i+3] = 128;
    }
#endif  /* (1) == DRPAI_INPUT_PADDING */
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
                const uint8_t* img_buffer = capture->get_img();
                if (!inference_start.load())
                {
                    /* Copy captured image to Image object. This will be used in Display Thread. */
                    memcpy(img_buffer0, img_buffer, capture->get_size());
/* PREPROC_MODE_HRNET (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_HRNET == 0
                    convert_format_Mat(img_buffer0);
#endif  /* PREPROC_MODE_HRNET */
                    depth_data = capture->get_depth();
                    /* Flush capture image area cache */
                    ret = capture->video_buffer_flush_dmabuf(capture->drpai_buf->idx, capture->drpai_buf->size);
                    if (0 != ret)
                    {
                        goto err;
                    }
                    
                    inference_start.store(1); /* Flag for AI Inference Thread. */
                }

#if (0) == INPUT_CAM_TYPE
                /**  
                 * To stabilize the Capture thread and AI Inference thread when this application uses the USB camera, control the display frame rate to 15 fps.
                 * In details, controls the number of frames to be sent the Image thread.
                 * This thread just has to send the frame to the Image thread every 66.6 msec (=1000[msec]/15[fps]).
                 */
                elapsed_time_last_disp += proc_time_capture;
                if( 1000 / target_disp_fps <= elapsed_time_last_disp )
                {
                    elapsed_time_last_disp = fmod(elapsed_time_last_disp, 1000 / target_disp_fps);

                    if (!img_obj_ready.load())
                    {
                        img.camera_to_image(img_buffer, capture->get_size());
                        img_obj_ready.store(1); /* Flag for Display Thread. */
                    }
                }
#else
                if (!img_obj_ready.load())
                {
                    img.camera_to_image(img_buffer, capture->get_size());
                    img_obj_ready.store(1); /* Flag for Display Thread. */
                }
#endif
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
    bool padding = false;
#ifdef CAM_INPUT_VGA
    padding = true;
#endif // CAM_INPUT_VGA
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

            for (auto& spec : array_body_spec) {
                spec.calc_length();
            }
            Get_Depth_Distance();

            // uncomment to show depth colormap
            /*depth_data->get_color([&](const uint8_t* data, size_t size) {
                size_t j = 0;
                for (size_t i = 0; i < (1920 * 1080 * 3); i += 6) {
                    auto p = data + i;
                    int y1 = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
                    int y2 = 0.299 * p[3] + 0.587 * p[4] + 0.114 * p[5];
                    int u = -0.147 * p[0] - 0.289 * p[1] + 0.436 * p[2] + 128;
                    int v = 0.615 * p[0] - 0.515 * p[1] - 0.100 * p[2] + 128;
                    img.set(++j, y1);
                    img.set(++j, u);
                    img.set(++j, y2);
                    img.set(++j, v);
                }
            });*/
            
            /* Draw Complete Skeleton. */
            draw_skeleton();

            /* Draw center box */
            if (CAM_IMAGE_WIDTH == 1280){
                img.draw_rect(426, 45, 428, 630, 0x0000FFu);
            }else{
                img.draw_rect(660, 80, 600, 920, 0x0000FFu);                
            }

            /* Convert YUYV image to BGRA format. */
            img.convert_format();

            /* Convert output image size. */
            img.convert_size(CAM_IMAGE_WIDTH, DRPAI_OUT_WIDTH, padding);

#if (0) == INF_YOLOX_SKIP
            /* Draw bounding box on image. */
            draw_bounding_box();
#endif
            /* Flip the image. */
            if (flip_mode == 1)
            {
                img.flip();
            }
                        
            /*displays AI Inference Results on display.*/
            print_result(&img);

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

    /* Initialize waylad */
    ret = wayland.init(capture->wayland_buf->idx, IMAGE_OUTPUT_WIDTH, IMAGE_OUTPUT_HEIGHT, IMAGE_CHANNEL_BGRA);
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
            wayland.commit(img.get_img(buf_id), NULL);

            if (display_state == 0) display_state = 1;

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
    /* Exit waylad */
    wayland.exit();
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

#if END_DET_TYPE
        int fd;
        char str[BUF_SIZE];
        char str_end[BUF_SIZE] = "end";
        ssize_t size;
        mkfifo("/tmp/appdetect", 0666);
        fd = open("/tmp/appdetect", O_RDWR);
        size = read(fd, str, BUF_SIZE);
        if (size > 0)
        {
            /* When mouse clicked. */
            printf("mouse clicked. : %s\n", str);
            str[size] = '\n';

            if (strcmp(str, str_end) == 0)
            {
                if (system("echo \"end\" > /tmp/gui") == -1)
                {
                    printf("[ERROR] Failed to send command\n");
                }
                goto err;
            }
        }
        close(fd);
#else
        c = getchar();
        if ('d' == c || 'D' == c)
        {
            if (switch_time_display == 0){
                switch_time_display = 1;
            }else{
                switch_time_display = 0;
            }
            c = getchar();
        }else if (EOF != c)
        {
            /* When key is pressed. */
            printf("key Detected.\n");
            goto err;
        }
#endif // END_DET_TYPE

        /* When nothing is detected. */
        usleep(WAIT_TIME);
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

#if END_DET_TYPE
        if (display_state == 1)
        {
            if (system("./../app_pointer_det & ") == -1)
            {
                printf("Command Error\n");
                goto main_proc_end;
            }
            display_state = 2;
        }
#endif

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
    sprintf(time_buf,"logs/%s_app_body_meas_cam.log",date_buf);
    auto logger = spdlog::basic_logger_mt("logger", time_buf);
    spdlog::set_default_logger(logger);

    /* OpenCVA Disable Code */
    unsigned long OCA_list[16]; 
    for (int i=0; i < 16; i++) OCA_list[i] = 0; 
    OCA_Activate( &OCA_list[0] ); 

    /* DRP-AI Frequency Setting */
    if (2 <= argc)
    {
        flip_mode = atoi(argv[1]);
    }
    else
    {
        flip_mode = FLIP_MODE_DEF;
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

    printf("RZ/V2H DRP-AI Sample Application\n");
    printf("Model : MMPose HRNet | %s  \n", AI1_DESC_NAME);
#if (0) == INF_YOLOX_SKIP
    printf("Model : Megvii-Base Detection YOLOX | %s\n", AI0_DESC_NAME);
#endif
    printf("Input : %s\n", INPUT_CAM_NAME);
    spdlog::info("************************************************");
    spdlog::info("  RZ/V2H DRP-AI Sample Application");
    spdlog::info("  Model : MMPose HRNet with YOLOX | {} {}", AI1_DESC_NAME,AI0_DESC_NAME);
    spdlog::info("  Input : {}", INPUT_CAM_NAME);
    spdlog::info("************************************************");
    printf("Argument : <mirror_flip_display_mode> = %d\n", flip_mode);
    printf("Argument : <AI-MAC_freq_factor> = %d\n", drpai_freq);

     uint64_t drpaimem_addr_start = 0;
    
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
    
    /*For HRNet*/
/* PREPROC_MODE_HRNET (n = 0: use OpenCV, n = 1: use preruntime)*/
#if PREPROC_MODE_HRNET == 1 
    /*Load pre_dir object to DRP-AI */
    ret = preruntime_2.Load(pre_dir_hrnet);
    if (0 < ret)
    {
        fprintf(stderr, "[ERROR] Failed to run Pre-processing Runtime_2 Load().\n");
        goto end_close_drpai;
    }
#endif  /* PREPROC_MODE_HRNET */

#if (0) == INF_YOLOX_SKIP
    drpaimem_addr_start = drpaimem_addr_start + DRPAI_MEM_OFFSET_HRNET;
#endif  /* INF_YOLOX_SKIP */
	
    runtime_status = runtime_2.LoadModel(model_dir_hrnet, drpaimem_addr_start);

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
    ret = img.init(CAM_IMAGE_WIDTH, CAM_IMAGE_HEIGHT, CAM_IMAGE_CHANNEL_YUY2, IMAGE_OUTPUT_WIDTH, IMAGE_OUTPUT_HEIGHT, IMAGE_CHANNEL_BGRA, capture->wayland_buf->mem);
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

