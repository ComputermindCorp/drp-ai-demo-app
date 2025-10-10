/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : capture.h
* Version      : 1.00
* Description  : RZ/V2H and RZ/V2N DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera or Image
***********************************************************************************************************************/

#ifndef CAPTURE_H
#define CAPTURE_H

#include <linux/videodev2.h>
#include "define.h"
#include <opencv2/opencv.hpp>

/* This block of code is only accessible from C code. */
#ifdef __cplusplus
extern "C" {
#endif
#include "mmngr_user_public.h"
#include "mmngr_buf_user_public.h"
#ifdef __cplusplus
}
#endif

class Capture
{
    public:
        Capture();
        ~Capture();

        struct camera_dma_buffer
        {
         /* The index of the buffer. */
         uint32_t idx;
         /* The file descriptor for the DMA buffer. */
         uint32_t dbuf_fd;
         /* The size of the buffer in bytes. */
         uint32_t size;
         /* The physical address of DMA buffer. */
         uint32_t phy_addr;
         /* The pointer to the memory for the buffer. */
         void *mem;           
        };
        std::string str_path;
        std::vector<std::string> file_list;
        int32_t file_index = -1;

        int8_t init(int mode, const char* folder_path);
        cv::Mat get_img(int mode);
        cv::Mat convert_inf_img(int mode);
        int32_t get_size();
        int32_t get_w();
        void set_w(int32_t w);
        int32_t get_h();
        void set_h(int32_t h);
        int32_t get_c();
        void set_c(int32_t c);
        std::string get_name();
        void set_idx_inc();
        void set_idx_dec();
        
        int8_t start_camera();
        int8_t capture_qbuf();
        uint64_t capture_image();
        int8_t close_camera();
        int8_t save_bin(std::string filename);
        int video_buffer_flush_dmabuf(uint32_t idx, uint32_t size);

    private:
        std::string device;
        int32_t camera_width;
        int32_t camera_height;
        int32_t camera_color;
        int m_fd;
        uint8_t *buffer[CAP_BUF_NUM];
        cv::Mat img_buffer;

        #define CAPTUREBUF      (CAM_IMAGE_WIDTH * CAM_IMAGE_HEIGHT * CAM_IMAGE_CHANNEL_YUY2)

        struct v4l2_buffer buf_capture;
        struct camera_dma_buffer *dma_buf[CAP_BUF_NUM];
        cv::Mat convert_format(uint8_t * tmp);
        int8_t xioctl(int8_t fd, int32_t request, void *arg);
        int8_t start_capture();
        int8_t stop_capture();
        int8_t open_camera_device();
        int8_t init_camera_fmt();
        int8_t init_buffer();
        int8_t video_buffer_alloc_dmabuf(struct camera_dma_buffer *buffer,int buf_size);
        void video_buffer_free_dmabuf(struct camera_dma_buffer *buffer);
        int8_t get_files(const char* folder_path);
};

#endif
