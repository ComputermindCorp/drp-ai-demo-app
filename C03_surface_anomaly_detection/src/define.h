/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : define.h
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera or Image
***********************************************************************************************************************/

#ifndef DEFINE_MACRO_H
#define DEFINE_MACRO_H

/*****************************************
* includes
******************************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <vector>
#include <map>
#include <fstream>
#include <errno.h>
#include <math.h>
#include <iomanip>
#include <atomic>
#include <semaphore.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <dirent.h>
#include <glob.h>

/*****************************************
* Macro for ResNet50
******************************************/
/* Input Camera support */
/* n = 0: USB Camera, n = 1: eCAM22 */
#define INPUT_CAM_TYPE              (0)

/* Output Camera Size */
#if INPUT_CAM_TYPE == 0
#define CAM_INPUT_VGA
#else /*INPUT_CAM_TYPE*/
#define CAM_INPUT_FHD
#endif /*INPUT_CAM_TYPE*/
#define MIPI_CAM_RES "1920x1080"

/*Time Measurement Flag*/
//#define DEBUG_TIME_FLG

/*Display AI frame rate*/
#undef DISP_AI_FRAME_RATE

/* DRP-AI memory offset for model object file*/
#define DRPAI_MEM_OFFSET            (0X38E0000)

/*****************************************
* Static Variables for ResNet50
* Following variables need to be changed in order to custormize the AI model
*  - label_list = class labels to be classified
*  - drpai_prefix = directory name of DRP-AI Object files (DRP-AI Translator output)
******************************************/
const static std::string app_name   = "Patch Core Demo";
/* Model Binary */
const static std::string model_dir  = "patch_core_demo";
/* Pre-processing Runtime Object */
const static std::string pre_dir    = model_dir + "/preprocess";
//const static std::string mem_bank_name = "out.bin";
const static std::string mem_bank_name = "resnet18_quantization_onnx_size224_param_0.0002_9.bin";

/*****************************************
* Macro for ResNet
******************************************/
/*DRP-AI Output image information*/
#ifdef CAM_INPUT_VGA
#define DRPAI_INPUT_WIDTH           (480)
#define DRPAI_INPUT_HEIGHT          (480)
#else /* CAM_INPUT_FHD */
#define DRPAI_INPUT_WIDTH           (512)
#define DRPAI_INPUT_HEIGHT          (512)
#endif

/*DRP-AI Output image information*/
#define OUTPUT_LAYER_1_C            (128)
#define OUTPUT_LAYER_1_H            (28)
#define OUTPUT_LAYER_1_W            (28)
#define OUTPUT_LAYER_2_C            (256)
#define OUTPUT_LAYER_2_H            (14)
#define OUTPUT_LAYER_2_W            (14)
#define OUTPUT_LAYER_1_SIZE         (OUTPUT_LAYER_1_C * OUTPUT_LAYER_1_H * OUTPUT_LAYER_1_W)
#define OUTPUT_LAYER_2_SIZE         (OUTPUT_LAYER_2_C * OUTPUT_LAYER_2_H * OUTPUT_LAYER_2_W)
#define OUTPUT_FEATURE_SIZE         (OUTPUT_LAYER_1_SIZE + OUTPUT_LAYER_2_SIZE)

#define ANOMARY_MAP_SIZE            (28)

/*****************************************
* Macro for Application
******************************************/
/*Maximum DRP-AI Timeout threshold*/
#define DRPAI_TIMEOUT               (5)
/* DRP_MAX_FREQ and DRPAI_FREQ are the   */
/* frequency settings for DRP-AI.        */
/* Basically use the default values      */

#define DRPAI_FREQ                  (2)
/* DRPAI_FREQ can be set from 1 to 127   */
/* 1,2: 1GHz                             */
/* 3: 630MHz                             */
/* 4: 420MHz                             */
/* 5: 315MHz                             */
/* ...                                   */
/* 127: 10MHz                            */
/* Calculation Formula:                  */
/*     1260MHz /(DRPAI_FREQ - 1)         */
/*     (When DRPAI_FREQ = 3 or more.)    */

/*Camera:: Capture Image Information*/
#ifdef CAM_INPUT_VGA
#define CAM_IMAGE_WIDTH             (640)
#define CAM_IMAGE_HEIGHT            (480)
#else /* CAM_INPUT_FHD */
#define CAM_IMAGE_WIDTH             (1920)
#define CAM_IMAGE_HEIGHT            (1080)
#endif

#define CAM_IMAGE_CHANNEL_YUY2      (2)
#define CAM_IMAGE_SIZE              (CAM_IMAGE_WIDTH * CAM_IMAGE_HEIGHT * CAM_IMAGE_CHANNEL_YUY2)

/*Camera:: Capture Information */
#if INPUT_CAM_TYPE == 1
#define CAP_BUF_NUM                 (6)
#define INPUT_CAM_NAME              "MIPI Camera"
#else /* INPUT_CAM_TYPE */
#define CAP_BUF_NUM                 (3)
#define INPUT_CAM_NAME              "USB Camera"
#endif /* INPUT_CAM_TYPE */

/*Display Information */
#define WIN_SIZE_WIDTH              (1024)
#define WIN_SIZE_HEIGHT             (768)
#define DSP_SIZE_WIDTH              (960)
#define DSP_SIZE_HEIGHT             (540)

#ifdef CAM_INPUT_VGA
#define IMAGE_OUTPUT_WIDTH          (720)
#define IMAGE_OUTPUT_HEIGHT         (540)
#define RECTANGLE_WIDTH             (540)
#define RECTANGLE_HEIGHT            (540)
#else /* CAM_INPUT_FHD */
#define IMAGE_OUTPUT_WIDTH          (960)
#define IMAGE_OUTPUT_HEIGHT         (540)
#define RECTANGLE_WIDTH             (512)
#define RECTANGLE_HEIGHT            (512)
#endif

#define IMAGE_CHANNEL_BGRA          (4)
#define WL_BUF_NUM                  (2)

/*input image memory area Information*/
#define IMG_AREA_ORG_ADDRESS        (0xD0000000)    /* Note: Don't change this address */
#define IMG_AREA_CNV_ADDRESS        (0x58000000)    /* CMA area start address used by mmngr */
#define IMG_AREA_SIZE               (0x20000000)    /* CMA area size */

/*Image:: Text information to be drawn on image*/
#define CHAR_SCALE_LARGE            (0.8)
#define CHAR_SCALE_SMALL            (0.7)
#define CHAR_THICKNESS              (2)
#define LINE_HEIGHT                 (30) /*in pixel*/
#define LINE_HEIGHT_OFFSET          (20) /*in pixel*/
#define TEXT_WIDTH_OFFSET           (10) /*in pixel*/

#define WHITE_DATA                  (0xFFFFFF) /* in RGB */
#define CV_BLUE                     cv::Scalar(255, 0, 0)
#define CV_WHITE                    cv::Scalar(255, 255, 255)
#define CV_BLACK                    cv::Scalar(0, 0, 0)
#define CV_GREEN                    cv::Scalar(0, 255, 0)
#define CV_RED                      cv::Scalar(0, 0, 255)
#define CV_ASH                      cv::Scalar(150, 150, 150)
#define CV_NABY                     cv::Scalar(80, 63, 51)

/*Waiting Time*/
#define WAIT_TIME                   (1000) /* microseconds */

/*Timer Related*/
#define CAPTURE_TIMEOUT             (20)  /* seconds */
#define AI_THREAD_TIMEOUT           (20)  /* seconds */
#define DISPLAY_THREAD_TIMEOUT      (20)  /* seconds */
#define KEY_THREAD_TIMEOUT          (5)   /* seconds */
#define TIME_COEF                   (1)

/*Buffer size for writing data to memory via DRP-AI Driver.*/
#define BUF_SIZE                    (1024)

/*Array size*/
#define SIZE_OF_ARRAY(array) (sizeof(array)/sizeof(array[0]))

#endif
