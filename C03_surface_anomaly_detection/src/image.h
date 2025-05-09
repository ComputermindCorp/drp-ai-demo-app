/***********************************************************************************************************************
* Copyright (C) 2023 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : image.h
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera
***********************************************************************************************************************/

#ifndef IMAGE_H
#define IMAGE_H

#include "define.h"
#include "color_map.h"
#include <opencv2/opencv.hpp>

class Image
{
    public:
        Image();
        ~Image();

        uint8_t* img_buffer[WL_BUF_NUM];
        uint8_t* overlay_buffer[WL_BUF_NUM];
        uint8_t get_buf_id();
        void write_string_rgb(std::string str, uint32_t align_type, uint32_t x, uint32_t y, float size, uint32_t color);

        void paste(cv::Mat dst, cv::Mat src, int x, int y, int width, int height);
        cv::Mat create_main_sc(int mode);
        cv::Mat create_result_sc(cv::Mat ret_img, float* map, double score, double threshold, double pre, double inf, double post);
        cv::Mat convert_to_disp_img(int mode, cv::Mat img, std::string name);

        uint8_t* get_img(uint8_t id);
        uint8_t* get_overlay_img(uint8_t id);
        uint8_t at(int32_t a);
        void set(int32_t a, uint8_t val);

        uint8_t init(uint32_t w, uint32_t h, uint32_t c, uint32_t ow, uint32_t oh, uint32_t oc);
        void convert_format();
        void convert_size(int in_w, int resize_w, bool is_padding);
        void camera_to_image(const uint8_t* buffer, int32_t size);
    private:
        uint8_t buf_id = 0;
        uint8_t idx = 0;

        /* Input Image (YUYV from V4L2) Information */
        uint32_t img_h;
        uint32_t img_w;
        uint32_t img_c;
        /* Output Image (BGRA for Wayland) Information */
        uint32_t out_h;
        uint32_t out_w;
        uint32_t out_c;
        ColorMap cls_c_map;

        cv::Mat create_heatmap_image(float* anomaly_map);
        void draw_rect_add_txt(cv::Mat src, std::string text, int x0, int y0, int x1, int y1, cv::Scalar bg = CV_ASH, cv::Scalar font = CV_WHITE, double scale = 1.0);
        void draw_rect_add_txt_multi(cv::Mat src, std::vector<std::pair<std::string, double>> txt_dic, int x0, int y0, int x1, int y1, cv::Scalar bg = CV_ASH, cv::Scalar font = CV_WHITE);
        uint8_t Clip(int value);
};

#endif
