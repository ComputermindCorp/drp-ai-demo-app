/***********************************************************************************************************************
* Copyright (C) 2024 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : camera.h
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for MMPose HRNet + Megvii-Base Detection YOLOX with MIPI/USB Camera
***********************************************************************************************************************/

#ifndef CAMERA_H
#define CAMERA_H

#include <linux/videodev2.h>
#include <memory>
#include <optional>
#include <functional>
#include <array>
#include "define.h"
/* This block of code is only accessible from C code. */
#ifdef __cplusplus
extern "C" {
#endif
#include "mmngr_user_public.h"
#include "mmngr_buf_user_public.h"
#ifdef __cplusplus
}
#endif

class Camera
{
    public:
        Camera();
        ~Camera();

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
        struct camera_dma_buffer *wayland_buf;
        struct camera_dma_buffer *drpai_buf;

        int8_t start_camera();
        int8_t capture_qbuf();
        uint64_t capture_image();
        int8_t close_camera();
        int8_t save_bin(std::string filename);
        int video_buffer_flush_dmabuf(uint32_t idx, uint32_t size);

        class DepthData {
            struct DepthDataImpl;
            std::unique_ptr<DepthDataImpl> impl_;

        public:
            friend class Camera;

            DepthData(DepthDataImpl*);
            DepthData(DepthData&&);
            ~DepthData();

            DepthData& operator=(const DepthData&);
            DepthData& operator=(DepthData&&);

            std::optional<std::array<float, 3>> get_real_pos(float col, float row, std::optional<float> dist = std::nullopt) const;
            int32_t width() const;
            int32_t height() const;
            void get_color(std::function<void(const uint8_t*, size_t)> f) const;

        };

        const uint8_t* get_img();
        std::optional<DepthData> get_depth();
        int32_t get_size();
        int32_t get_w();
        void set_w(int32_t w);
        int32_t get_h();
        void set_h(int32_t h);
        int32_t get_c();
        void set_c(int32_t c);

    private:
        struct CameraImpl;
        std::unique_ptr<CameraImpl> impl_;

};

#endif

