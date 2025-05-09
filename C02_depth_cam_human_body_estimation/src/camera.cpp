/***********************************************************************************************************************
* Copyright (C) 2024 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : camera.cpp
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for MMPose HRNet + Megvii-Base Detection YOLOX with MIPI/USB Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "camera.h"
#include <errno.h>
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/rsutil.h> // for deproject
#include <sstream>
#include <optional>
#include "spdlog/spdlog.h"


/*****************************************
* Function Name : format
* Description   : Format specification.
* Arguments     : fmt = an object that represents the format string.
*                 args = arguments to be formatted
* Return value  : A string object holding the formatted result.
******************************************/
template <typename ... Args>
std::string format(const std::string& fmt, Args ... args)
{
    size_t len = std::snprintf(nullptr, 0, fmt.c_str(), args ...);
    std::vector<char> buf(len + 1);
    std::snprintf(&buf[0], len + 1, fmt.c_str(), args ...);
    return std::string(&buf[0], &buf[0] + len);
}

struct Camera::CameraImpl {
    static constexpr int WAYLANDBUF = (IMAGE_OUTPUT_WIDTH * IMAGE_OUTPUT_HEIGHT * IMAGE_CHANNEL_BGRA * WL_BUF_NUM);
    static constexpr int DRPAIBUF = (CAM_IMAGE_WIDTH * CAM_IMAGE_WIDTH * CAM_IMAGE_CHANNEL_YUY2);

    int32_t camera_width;
    int32_t camera_height;
    int32_t camera_color;

    int32_t depth_width;
    int32_t depth_height;

    rs2::pipeline pipeline;
    rs2::pipeline_profile profile;

    struct intrs {
        rs2_intrinsics intr;
        intrs(const intrs& other) = default;
        intrs(intrs&& other) = default;
        intrs(const rs2_intrinsics& other) : intr(other) {}
    };
    std::shared_ptr<intrs> color_intr;

    rs2::frameset frames;
    rs2::align align;
    std::optional<rs2::video_frame> color;
    std::optional<rs2::depth_frame> depth;

    rs2::hole_filling_filter filter_hf;
    rs2::decimation_filter filter_de;
    rs2::spatial_filter filter_sp;
    rs2::disparity_transform from_disp, to_disp;

    CameraImpl()
        : from_disp(false), to_disp(true), align(RS2_STREAM_COLOR)
    {}

    ~CameraImpl() {}

    /*****************************************
    * Function Name : run_realsense
    * Description   : Execute function with Realsense error trapping
    * Arguments     : f = target function
    * Return value  : 0 if succeeded
    *                 not 0 otherwise
    ******************************************/
    template<typename F>
    int run_realsense(F&& f) try
    {
        return f();
    }
    catch (const rs2::error & e) {
        spdlog::error(format("RealSense error calling %s(%s): %s\n",
            e.get_failed_function().c_str(),
            e.get_failed_args().c_str(),
            e.what()));
        return -1;
    }
    catch (const std::exception& e) {
        spdlog::error(e.what());
        return -1;
    }

    /*****************************************
    * Function Name : start
    * Description   : start streaming
    * Arguments     : -
    * Return value  : 0 if succeeded
    *                 not 0 otherwise
    ******************************************/
    int start()
    {
        return run_realsense([this]() {
            rs2::config cfg;
            cfg.enable_stream(RS2_STREAM_COLOR, camera_width, camera_height, RS2_FORMAT_YUYV, 30);
            cfg.enable_stream(RS2_STREAM_DEPTH, depth_width, depth_height, RS2_FORMAT_Z16, 30);

            profile = pipeline.start(cfg);
            const auto& stream_profile = profile.get_stream(rs2_stream::RS2_STREAM_COLOR);
            color_intr = std::make_shared<intrs>(rs2::video_stream_profile(stream_profile).get_intrinsics());

            return 0;
        });
    }

    /*****************************************
    * Function Name : stop
    * Description   : stop streaming
    * Arguments     : -
    * Return value  : 0 if succeeded
    *                 not 0 otherwise
    ******************************************/
    int stop()
    {
        return run_realsense([this]() {
            pipeline.stop();
            return 0;
        });
    }

    /*****************************************
    * Function Name : capture_one
    * Description   : capture a picture and a depth data
    * Arguments     : -
    * Return value  : 0 if succeeded
    *                 not 0 otherwise
    ******************************************/
    int capture_one()
    {
        return run_realsense([this]() {
            frames = pipeline.wait_for_frames();
#if (1) == FRAME_CALIBRATIN_EVERY_TIME
            /* Frame calibration every time Function. */
            frames = align.process(frames); // very slow...
#endif            
            color = frames.get_color_frame();
            depth = frames.get_depth_frame();
            /*depth = filter_de.process(*depth);
            depth = to_disp.process(*depth);
            depth = filter_sp.process(*depth);
            depth = from_disp.process(*depth);*/ // very slow...
            depth = filter_hf.process(*depth);
            return 0;
        });
    }

    /*****************************************
    * Function Name : video_buffer_alloc_dmabuf
    * Description   : Allocate a DMA buffer for the camera
    * Arguments     : buffer = pointer to the camera_dma_buffer struct
    *                 buf_size = size of the allocation
    * Return value  : 0 if succeeded
    *                 not 0 otherwise
    ******************************************/
    static int8_t video_buffer_alloc_dmabuf(struct camera_dma_buffer *buffer,int buf_size)
    {
        MMNGR_ID id;
        uint32_t phard_addr;
        void *puser_virt_addr;
        int m_dma_fd;

        buffer->size = buf_size;
        mmngr_alloc_in_user_ext(&id, buffer->size, &phard_addr, &puser_virt_addr, MMNGR_VA_SUPPORT_CACHED, NULL);
        memset((void*)puser_virt_addr, 0, buffer->size);
        buffer->idx = id;
        buffer->mem = (void *)puser_virt_addr;
        buffer->phy_addr = phard_addr;
        if (!buffer->mem)
        {
            return -1;
        }
        mmngr_export_start_in_user_ext(&id, buffer->size, phard_addr, &m_dma_fd, NULL);
        buffer->dbuf_fd = m_dma_fd;
        return 0;
    }

    /*****************************************
    * Function Name : video_buffer_free_dmabuf
    * Description   : free a DMA buffer for the camera
    * Arguments     : buffer = pointer to the camera_dma_buffer struct
    * Return value  : -
    ******************************************/
    static void video_buffer_free_dmabuf(struct camera_dma_buffer *buffer)
    {
        mmngr_free_in_user_ext(buffer->idx);
        return;
    }

   /*****************************************
   * Function Name : calc_length
   * Description   : Calculate length between keypoints
   * Arguments     : col = x pos on picture
                     row = y pos on picture
                     dist = z pos on picture. if std::nullopt get it from depth data
   * Return value  : std::nullopt of null Z-depth found, otherwise real-world position
   ******************************************/
    static std::array<float, 3> to_point(const rs2::depth_frame& frame, const rs2_intrinsics& intr, float px, float py, std::optional<float> dist = std::nullopt)
    {
        float depth;
        if (dist) { depth = *dist; }
        else { depth = frame.get_distance(px, py); }

        float p_px[2] = { px, py, };
        float pt[3];
        rs2_deproject_pixel_to_point(pt, &intr, p_px, depth);

        return { pt[0], pt[1], pt[2], };
    }

};

struct Camera::DepthData::DepthDataImpl {
    rs2::depth_frame frame;
    std::weak_ptr<CameraImpl::intrs> intr;

};

Camera::DepthData::DepthData(DepthDataImpl* impl)
    : impl_(impl)
{
    //
}
Camera::DepthData::DepthData(DepthData&& other)
    : impl_(std::move(other.impl_))
{
    other.impl_.reset();
}

Camera::DepthData& Camera::DepthData::operator=(const DepthData& other)
{
    impl_->frame = other.impl_->frame;
    impl_->intr = other.impl_->intr;
    return *this;
}
Camera::DepthData& Camera::DepthData::operator=(DepthData&& other)
{
    impl_->frame = std::move(other.impl_->frame);
    impl_->intr = std::move(other.impl_->intr);
    return *this;
}

void Camera::DepthData::get_color(std::function<void(const uint8_t*, size_t)> f) const
{
    rs2::colorizer crl;
    auto ret = impl_->frame.apply_filter(crl);
    f(reinterpret_cast<const uint8_t*>(ret.get_data()), ret.get_data_size());
}

Camera::DepthData::~DepthData()
{}

/*****************************************
* Function Name : calc_length
* Description   : Calculate length between keypoints
* Arguments     : col = x pos on picture
                  row = y pos on picture
                  dist = z pos on picture. if std::nullopt get it from depth data
* Return value  : std::nullopt if valid data is not exist or null Z-depth found, otherwise real-world position
******************************************/
std::optional<std::array<float, 3>> Camera::DepthData::get_real_pos(float col, float row, std::optional<float> dist) const
{
    auto p_intr = impl_->intr.lock();
    if (p_intr && col < impl_->frame.get_width() && row < impl_->frame.get_height()) {
        return CameraImpl::to_point(impl_->frame, p_intr->intr, col, row, dist);
    }
    else {
        return std::nullopt;
    }
}

int32_t Camera::DepthData::width() const
{
    return DEPTH_IMAGE_WIDTH;
}

int32_t Camera::DepthData::height() const
{
    return DEPTH_IMAGE_HEIGHT;
}

Camera::Camera()
    : impl_(new CameraImpl)
{
    impl_->camera_width   = CAM_IMAGE_WIDTH;
    impl_->camera_height = CAM_IMAGE_HEIGHT;
    impl_->camera_color  = CAM_IMAGE_CHANNEL_YUY2;

    impl_->depth_width    = DEPTH_IMAGE_WIDTH;
    impl_->depth_height   = DEPTH_IMAGE_HEIGHT;

    impl_->filter_sp.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.25);
    impl_->filter_sp.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA, 50);
    impl_->filter_sp.set_option(RS2_OPTION_FILTER_MAGNITUDE, 1);
}

Camera::~Camera()
{
}

/*****************************************
* Function Name : start_camera
* Description   : Function to initialize USB camera capture
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::start_camera()
{
    int8_t ret = 0;
    int8_t wayland_buf_ret = -1;
    int8_t drpai_buf_ret = -1;

    /* Initialize buffer */
    wayland_buf = NULL;
    drpai_buf = NULL;

    wayland_buf = (camera_dma_buffer*)malloc(sizeof(camera_dma_buffer));
    if (NULL == wayland_buf)
    {
        fprintf(stderr, "[ERROR] Failed to malloc the wayland_buf\n");
        goto err_end;
    }
    
    wayland_buf_ret = CameraImpl::video_buffer_alloc_dmabuf(wayland_buf, CameraImpl::WAYLANDBUF);
    
    if (-1 == wayland_buf_ret)
    {
        fprintf(stderr, "[ERROR] Failed to Allocate DMA buffer for the wayland_buf\n");
        goto err_end;
    }
    

    drpai_buf = (camera_dma_buffer*)malloc(sizeof(camera_dma_buffer));
    if (NULL == drpai_buf)
    {
        fprintf(stderr, "[ERROR] Failed to malloc the drpai_buf\n");
        goto err_end;
    }
    
    drpai_buf_ret = CameraImpl::video_buffer_alloc_dmabuf(drpai_buf, CameraImpl::DRPAIBUF);
    if (-1 == drpai_buf_ret)
    {
        fprintf(stderr, "[ERROR] Failed to Allocate DMA buffer for the drpai_buf\n");
        goto err_end;
    }
    
    ret = impl_->start();
    if (0 != ret) 
    {
        printf("failed to start camera\n");
        goto err_end;
    }

    ret = 0;
    goto end;

err_end:
    /* free buffer */
    if (0 == wayland_buf_ret)
    {
        CameraImpl::video_buffer_free_dmabuf(wayland_buf);
    }

    if (0 == drpai_buf_ret)
    {
        CameraImpl::video_buffer_free_dmabuf(drpai_buf);
    }
    
    free(wayland_buf);
    wayland_buf = NULL;

    free(drpai_buf);
    drpai_buf = NULL;

    ret = -1;
    goto end;
end:
    return ret;
}


/*****************************************
* Function Name : close_capture
* Description   : Close camera and free buffer
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::close_camera()
{
    int8_t ret = 0;

    ret = impl_->stop();
    if (0 != ret) return ret;

    CameraImpl::video_buffer_free_dmabuf(wayland_buf);
    free(wayland_buf);
    wayland_buf = NULL;

    CameraImpl::video_buffer_free_dmabuf(drpai_buf);
    free(drpai_buf);
    drpai_buf = NULL;

    return 0;
}

/*****************************************
* Function Name : capture_qbuf
* Description   : Function to enqueue the buffer.
*                 (Call this function after capture_image() to restart filling image data into buffer)
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::capture_qbuf()
{
    return 0;
}

/*****************************************
* Function Name : capture_image
* Description   : Function to capture image and return the physical memory address where the captured image stored.
*                 Must call capture_qbuf after calling this function.
* Arguments     : -
* Return value  : the physical memory address where the captured image stored.
******************************************/
uint64_t Camera::capture_image()
{
    impl_->capture_one();
    if (impl_->color) {
        return reinterpret_cast<uint64_t>(impl_->color->get_data());
    }
    else {
        return 0;
    }
}

/*****************************************
* Function Name : save_bin
* Description   : Get the capture image from buffer and save it into binary file
* Arguments     : filename = binary file name to be saved
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::save_bin(std::string filename)
{
    if (!impl_->color) { return -1; }

    int8_t ret = 0;
    FILE * fp = fopen(filename.c_str(), "wb");
    if (!fp)
    {
        return -1;
    }

    /* Get data from buffer and write to binary file */
    ret = fwrite((uint8_t *)impl_->color->get_data(), sizeof(uint8_t),impl_->color->get_data_size(), fp);
    if (!ret)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/*****************************************
* Function Name : video_buffer_flush_dmabuf
* Description   : flush a DMA buffer in continuous memory area
*                 MUST be called when writing data to DMA buffer
* Arguments     : idx = id of the buffer to be flushed.
*                 size = size to be flushed.
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int Camera::video_buffer_flush_dmabuf(uint32_t idx, uint32_t size)
{
    int mm_ret = 0;
    
    /* Flush capture image area cache */
    mm_ret = mmngr_flush(idx, 0, size);
    
    return mm_ret;
}

/*****************************************
* Function Name : get_img
* Description   : Function to return the camera buffer
* Arguments     : -
* Return value  : camera buffer
******************************************/
const uint8_t* Camera::get_img()
{
    if (impl_->color) {
        return reinterpret_cast<const uint8_t*>(impl_->color->get_data());
    }
    else {
        return nullptr;
    }
}

/*****************************************
* Function Name : get_depth
* Description   : Function to return the camera buffer
* Arguments     : -
* Return value  : camera buffer
******************************************/
std::optional<Camera::DepthData> Camera::get_depth()
{
    if (impl_->depth) {
        auto d = new Camera::DepthData::DepthDataImpl{ *(impl_->depth), impl_->color_intr, };
        return DepthData(d);
    }
    else {
        return std::nullopt;
    }
}

/*****************************************
* Function Name : get_size
* Description   : Function to return the camera buffer size (W x H x C)
* Arguments     : -
* Return value  : camera buffer size (W x H x C )
******************************************/
int32_t Camera::get_size()
{
    if (impl_->color) {
        return impl_->color->get_data_size();
    }
    else {
        return 0;
    }
}

/*****************************************
* Function Name : get_w
* Description   : Get camera_width. This function is currently NOT USED.
* Arguments     : -
* Return value  : camera_width = width of camera capture image.
******************************************/
int32_t Camera::get_w()
{
    return impl_->camera_width;
}

/*****************************************
* Function Name : set_w
* Description   : Set camera_width. This function is currently NOT USED.
* Arguments     : w = new camera capture image width
* Return value  : -
******************************************/
void Camera::set_w(int32_t w)
{
    impl_->camera_width = w;
    return;
}

/*****************************************
* Function Name : get_h
* Description   : Get camera_height. This function is currently NOT USED.
* Arguments     : -
* Return value  : camera_height = height of camera capture image.
******************************************/
int32_t Camera::get_h()
{
    return impl_->camera_height;
}

/*****************************************
* Function Name : set_h
* Description   : Set camera_height. This function is currently NOT USED.
* Arguments     : w = new camera capture image height
* Return value  : -
******************************************/
void Camera::set_h(int32_t h)
{
    impl_->camera_height = h;
    return;
}

/*****************************************
* Function Name : get_c
* Description   : Get camera_color. This function is currently NOT USED.
* Arguments     : -
* Return value  : camera_color = color channel of camera capture image.
******************************************/
int32_t Camera::get_c()
{
    return impl_->camera_color;
}

/*****************************************
* Function Name : set_c
* Description   : Set camera_color. This function is currently NOT USED.
* Arguments     : c = new camera capture image color channel
* Return value  : -
******************************************/
void Camera::set_c(int32_t c)
{
    impl_->camera_color = c;
    return;
}

