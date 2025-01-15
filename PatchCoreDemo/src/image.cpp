/***********************************************************************************************************************
* Copyright (C) 2023 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : image.cpp
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "image.h"
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

Image::Image()
{

}

Image::~Image()
{

}

/*****************************************
* Function Name : get_img
* Description   : Function to return the camera buffer
* Arguments     : -
* Return value  : camera buffer
******************************************/
uint8_t* Image::get_img(uint8_t id)
{
    return img_buffer[id];
}

uint8_t* Image::get_overlay_img(uint8_t id)
{
    return overlay_buffer[id];
}


/*****************************************
* Function Name : init
* Description   : Function to initialize img_buffer in Image class
*                 This application uses udmabuf in order to
*                 continuous memory area for DRP-AI input data
* Arguments     : w = input image width in YUYV
*                 h = input image height in YUYV
*                 c = input image channel in YUYV
*                 ow = output image width in BGRA to be displayed via Wayland
*                 oh = output image height in BGRA to be displayed via Wayland
*                 oc = output image channel in BGRA to be displayed via Wayland
*                 mem = pointer to the memory for the display buffer
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
uint8_t Image::init(uint32_t w, uint32_t h, uint32_t c, uint32_t ow, uint32_t oh, uint32_t oc)
{
    /*Initialize input image information */
    img_w = w;
    img_h = h;
    img_c = c;
    /*Initialize output image information*/
    out_w = ow;
    out_h = oh;
    out_c = oc;
    
    return 0;
}

/*****************************************
* Function Name : paste
* Description   : Paste src image to dst image
* Arguments     : dst = dst image
*                 src = src image
*                 x, y = Paste start point coordinates
*                 width, height = Paste size
* Return Value  : -
******************************************/
void Image::paste(cv::Mat dst, cv::Mat src, int x, int y, int width, int height) 
{
	cv::Mat resized_img;
	cv::resize(src, resized_img, cv::Size(width, height));

	if (x >= dst.cols || y >= dst.rows) return;
	int w = (x >= 0) ? std::min(dst.cols - x, resized_img.cols) : std::min(std::max(resized_img.cols + x, 0), dst.cols);
	int h = (y >= 0) ? std::min(dst.rows - y, resized_img.rows) : std::min(std::max(resized_img.rows + y, 0), dst.rows);
	int u = (x >= 0) ? 0 : std::min(-x, resized_img.cols - 1);
	int v = (y >= 0) ? 0 : std::min(-y, resized_img.rows - 1);
	int px = std::max(x, 0);
	int py = std::max(y, 0);

	cv::Mat roi_dst = dst(cv::Rect(px, py, w, h));
	cv::Mat roi_resized = resized_img(cv::Rect(u, v, w, h));
	roi_resized.copyTo(roi_dst);
}

/*****************************************
 * Function Name : draw_rect_add_txt
 * Description   : This function to draws a rectangle and adds text on it.
 * Arguments     : The function takes four parameters: a string variable named "text" 
 *                 which represents the text to be added on the rectangle, 
 *                 and four integer variables named "x0", "y0", "x1", and "y1" 
 *                 which represent the coordinates of the rectangle's top-left and bottom-right corners.
 ******************************************/
void Image::draw_rect_add_txt(cv::Mat src, std::string text, int x0, int y0, int x1, int y1, cv::Scalar bg, cv::Scalar font, double scale)
{
    cv::Size textSize = getTextSize(text, FONT_HERSHEY_SIMPLEX, scale, 2, 0);
    rectangle(src, Point(x0, y0), Point(x1, y1), bg, -1);
    rectangle(src, Point(x0, y0), Point(x1, y1), font, 1);
    putText(src, text, Point(
        x0 + ((x1 - x0) - textSize.width) / 2, 
        y0 + ((y1 - y0) - textSize.height) / 2 + textSize.height), 
        FONT_HERSHEY_SIMPLEX, scale, font, 2);
}

void Image::draw_rect_add_txt_multi(cv::Mat src, std::vector<std::pair<std::string, double>> txt_dic, int x0, int y0, int x1, int y1, cv::Scalar bg, cv::Scalar font)
{
    rectangle(src, Point(x0, y0), Point(x1, y1), bg, -1);
    rectangle(src, Point(x0, y0), Point(x1, y1), font, 1);

    int offset_y = 25;
    for (const auto& e : txt_dic) 
    {
        int thin = (int)(e.second * 3.5);
        cv::Size textSize = getTextSize(e.first, FONT_HERSHEY_SIMPLEX, e.second, thin, 0);
        putText(src, e.first, Point(
            x0 + ((x1 - x0) - textSize.width) / 2, 
            y0 + textSize.height + offset_y), 
            FONT_HERSHEY_SIMPLEX, e.second, font, thin);
        offset_y += textSize.height + 15;
    }
}

/*****************************************
* Function Name : convert_to_disp_img
* Description   : Convert an image to a display image
* Arguments     : mode = application mode
* Return Value  : -
******************************************/
cv::Mat Image::convert_to_disp_img(int mode, cv::Mat img, std::string name)
{
    cv::Mat work_mat = cv::Mat::zeros(cv::Size(960,540), CV_8UC4);
    cv::Mat convert_mat;

    if(mode != 0){
        cv::resize(img, convert_mat, cv::Size(work_mat.cols, work_mat.rows));
        int width = DRPAI_INPUT_WIDTH;
        int height = DRPAI_INPUT_HEIGHT;
        int x = (convert_mat.cols - width) / 2;
        int y = (convert_mat.rows - height) / 2;
        cv::rectangle(convert_mat, cv::Point(x, y), cv::Point(x + width, y + height), CV_RED, 2);
        return convert_mat.clone();
    }
    else{
        cv::resize(img, convert_mat, cv::Size(img.cols, work_mat.rows));
        cv::Mat Roi1(work_mat, cv::Rect((work_mat.cols - convert_mat.cols) / 2, (work_mat.rows - convert_mat.rows) / 2, convert_mat.cols, convert_mat.rows));
        convert_mat.copyTo(Roi1);

        draw_rect_add_txt(work_mat, "<", 20, (work_mat.rows - 100) / 2, 65, (work_mat.rows - 100) / 2 + 100);
        draw_rect_add_txt(work_mat, ">", 895, (work_mat.rows - 100) / 2, 940, (work_mat.rows - 100) / 2 + 100);
        cv::Size textSize = getTextSize(name, FONT_HERSHEY_SIMPLEX, 0.8, 2, 0);
        putText(work_mat, name, Point(20, 10 + textSize.height), FONT_HERSHEY_SIMPLEX, 0.8, CV_WHITE, 2);
        return work_mat.clone();
    }
}

/*****************************************
* Function Name : create_heatmap_image
* Description   : create heatmap image
* Arguments     : -
* Return Value  : -
******************************************/
cv::Mat Image::create_heatmap_image(float* anomaly_map)
{
    uint8_t* heatmap_buf = new uint8_t[ANOMARY_MAP_SIZE * ANOMARY_MAP_SIZE * 4];
    int index = 0;

    for(int i = 0; i < ANOMARY_MAP_SIZE; i++)
    {
        for (int j = 0; j < ANOMARY_MAP_SIZE; j++)
        {
            // get color map
            vector<uint8_t> new_map = cls_c_map.get_color_map(anomaly_map[i * ANOMARY_MAP_SIZE + j]);

            heatmap_buf[index++] = new_map[0];     // blue
            heatmap_buf[index++] = new_map[1];     // green
            heatmap_buf[index++] = new_map[2];     // red
            heatmap_buf[index++] = 255;            // alpha
        }
    }
    cv::Mat heatmap_image(ANOMARY_MAP_SIZE, ANOMARY_MAP_SIZE, CV_8UC4, heatmap_buf);
    cv::Mat resize_image;
    /* Resize */
    cv::resize(heatmap_image, resize_image, cv::Size(DRPAI_INPUT_WIDTH, DRPAI_INPUT_HEIGHT));
    delete [] heatmap_buf;
    heatmap_buf = nullptr;

    return resize_image.clone();
}

/*****************************************
* Function Name : create_result_sc
* Description   : Create result screen
* Arguments     : -
* Return Value  : -
******************************************/
cv::Mat Image::create_result_sc(cv::Mat ret_img, float* map, double score, double threshold, double pre, double inf, double post)
{
    double scale = 0.7;
    int y = 34;
    std::stringstream stream;
    cv::Size textSize;
    cv::Mat base_sc(Size(WIN_SIZE_WIDTH, 600), CV_8UC4, Scalar(80, 63, 51));

    cv::Mat heat_map = create_heatmap_image(map);
    cv::cvtColor(ret_img, ret_img, cv::COLOR_RGB2BGRA);
    cv::addWeighted(ret_img, 0.5, heat_map, 1 - 0.5, 0.0, ret_img);

    cv::Mat Roi1(base_sc, cv::Rect(30, 44, ret_img.cols, ret_img.rows));
    ret_img.copyTo(Roi1);

    std::vector<std::pair<std::string, double>> time_dic{
        {"DRP-AI TVM Pre-Processing : ", pre},
        {"DRP-AI TVM Inference : ", inf},
        {"CPU Post-Processing : ", post},
    };
    for (const auto& e : time_dic) 
    {
        stream.str("");
        stream << e.first << std::fixed << std::setprecision(1) << std::round(e.second * 10) / 10 << "ms";
        std::string str_time = stream.str();
        textSize = getTextSize(str_time, FONT_HERSHEY_SIMPLEX, scale, 2, 0);
        putText(base_sc, str_time, Point((base_sc.cols - 30) - textSize.width, y + textSize.height + 10), FONT_HERSHEY_SIMPLEX, scale, CV_WHITE, 2);
        y += textSize.height + 10;
    }

    stream.str("");
    stream << std::fixed << std::setprecision(2) << std::round(score * 100) / 100;
    std::string str_score = stream.str();
    stream.str("");
    stream << "NG Threshold : " << std::fixed << std::setprecision(2) << std::round(threshold * 100) / 100;
    std::string str_thresh = stream.str();
    std::string str_result = "OK";
    cv::Scalar scalar = CV_GREEN;
    if(score > threshold)
    {
        str_result = "NG";
        scalar = CV_RED;
    }
    std::vector<std::pair<std::string, double>> txt_dic{
        {"Result", 0.8},
        {str_result, 1.6},
        {"", 0.8},
        {"Anomaly Score", 0.8},
        {str_score, 1.6},
        {str_thresh, 0.6},
    };
    draw_rect_add_txt_multi(base_sc, txt_dic, 580, y + 30, 980, y + 30 + 270, scalar);
    return base_sc.clone();
}

/*****************************************
* Function Name : create_main_sc
* Description   : Create main screen
* Arguments     : mode = application mode
* Return Value  : -
******************************************/
cv::Mat Image::create_main_sc(int mode)
{
    cv::Mat base_sc(Size(WIN_SIZE_WIDTH, WIN_SIZE_HEIGHT), CV_8UC4, CV_NABY);
    std::string str_mode;
    std::string str_info;
    if(mode == 0)
    {
        str_mode = "Mode : Image";
        str_info = "*Select the image to be inspected and press Start.";
        //draw_rect_add_txt(base_sc, "Start", (WIN_SIZE_WIDTH - 250) / 2, 688, (WIN_SIZE_WIDTH - 250) / 2 + 250, 748);
    }
    else
    {
        str_mode = "Mode : Camera";
        str_info = "*Place the object to be inspected in the rectangle and press Start.";
    }
    cv::putText(base_sc, str_mode.c_str(), cv::Point(40, 60), cv::FONT_HERSHEY_SIMPLEX, 1.4, CV_WHITE, 2);
    cv::putText(base_sc, str_info.c_str(), cv::Point(40, 105), cv::FONT_HERSHEY_SIMPLEX, 0.8, CV_WHITE, 2);
    
    draw_rect_add_txt(base_sc, "x", 940, 30, 1000, 65, CV_NABY, CV_WHITE, 0.7);
    draw_rect_add_txt(base_sc, "Start", (WIN_SIZE_WIDTH - 250) / 2, 688, (WIN_SIZE_WIDTH - 250) / 2 + 250, 748);
    return base_sc.clone();
}


/*****************************************
* Function Name : write_string_rgb
* Description   : OpenCV putText() in RGB
* Arguments     : str = string to be drawn
*                 x = bottom left coordinate X of string to be drawn
*                 y = bottom left coordinate Y of string to be drawn
*                 scale = scale for letter size
*                 color = letter color must be in RGB, e.g. white = 0xFFFFFF
* Return Value  : -
******************************************/
void Image::write_string_rgb(std::string str, uint32_t align_type,  uint32_t x, uint32_t y, float scale, uint32_t color)
{
    uint8_t thickness = CHAR_THICKNESS;
    /*Extract RGB information*/
    uint8_t r = (color >> 16) & 0x0000FF;
    uint8_t g = (color >>  8) & 0x0000FF;
    uint8_t b = color & 0x0000FF;
    int ptx = 0;
    int pty = 0;
    /*OpenCV image data is in BGRA */
    cv::Mat bgra_image(out_h, out_w, CV_8UC4, img_buffer[buf_id]);

    int baseline = 0;
    cv::Size size = cv::getTextSize(str.c_str(), cv::FONT_HERSHEY_SIMPLEX, scale, thickness + 2, &baseline);
    if (align_type == 1)
    {
        ptx = x;
        pty = y;
    }
    else if (align_type == 2)
    {
        ptx = out_w - (size.width + x);
        pty = y;
    }
    /*Color must be in BGR order*/
    cv::putText(bgra_image, str.c_str(), cv::Point(ptx, pty), cv::FONT_HERSHEY_SIMPLEX, scale, cv::Scalar(0x00, 0x00, 0x00, 0xFF), thickness + 2);
    cv::putText(bgra_image, str.c_str(), cv::Point(ptx, pty), cv::FONT_HERSHEY_SIMPLEX, scale, cv::Scalar(b, g, r, 0xFF), thickness);
}


/*****************************************
* Function Name : convert_format
* Description   : Convert YUYV image to BGRA format
* Arguments     : -
* Return value  : -
******************************************/
void Image::convert_format()
{
#ifdef DEBUG_TIME_FLG
    using namespace std;
    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
#endif // DEBUG_TIME_FLG

    uint8_t* pd = img_buffer[buf_id];
    uint8_t buffer[img_w * img_h * out_c];
    int pix_count = 0;
    for (int i = 0; i < (int)(img_h * img_w / 2); i++)
    {
        int y0 = (int)pd[0] - 16;
        int u0 = (int)pd[1] - 128;
        int y1 = (int)pd[2] - 16;
        int v0 = (int)pd[3] - 128;

        pd += 4;
        buffer[pix_count++] = Clip((298 * y0 + 516 * u0 + 128) >> 8); // blue
        buffer[pix_count++] = Clip((298 * y0 - 100 * u0 - 208 * v0 + 128) >> 8); // green
        buffer[pix_count++] = Clip((298 * y0 + 409 * v0 + 128) >> 8); // red
        buffer[pix_count++] = 255;

        buffer[pix_count++] = Clip((298 * y1 + 516 * u0 + 128) >> 8); // blue
        buffer[pix_count++] = Clip((298 * y1 - 100 * u0 - 208 * v0 + 128) >> 8); // green
        buffer[pix_count++] = Clip((298 * y1 + 409 * v0 + 128) >> 8); // red
        buffer[pix_count++] = 255;
    }
    memcpy(img_buffer[buf_id], &buffer, img_w * img_h * out_c);

#ifdef DEBUG_TIME_FLG
    end = chrono::system_clock::now();
    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0);
    printf("Convert YUYV To BGRA Time : %lf[ms]\n", time);
#endif // DEBUG_TIME_FLG
}

uint8_t Image::Clip(int value)
{
    //unsigned char ret = (uint8_t)std::round(value);
    if (value > 255)
    {
        value = 255;
    }
    if (value < 0)
    {
        value = 0;
    }
    return value;
}


/*****************************************
* Function Name : convert_size
* Description   : Scale down the input data (1920x1080) to the output data (1280x720) using OpenCV.
* Arguments     : -
* Return value  : -
******************************************/
void Image::convert_size(int in_w, int resize_w, bool is_padding)
{
    if (in_w == resize_w)
    {
        return;
    }

    cv::Mat org_image(img_h, img_w, CV_8UC4, img_buffer[buf_id]);
    cv::Mat resize_image;
    /* Resize */
    cv::resize(org_image, resize_image, cv::Size(), 1.0 * resize_w / in_w, 1.0 * resize_w / in_w);
	
    if (is_padding)
    {
        cv::Mat dst_image;
        copyMakeBorder(resize_image, dst_image, 0, 0, (out_w - resize_w) / 2, (out_w - resize_w) / 2, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 255));
        memcpy(img_buffer[buf_id], dst_image.data, out_w * out_h * out_c);
    }
    else
    {
        memcpy(img_buffer[buf_id], resize_image.data, out_w * out_h * out_c);
    }
}

/*****************************************
* Function Name : camera_to_image
* Description   : Function to copy the external image buffer data to img_buffer
*                 This is only place where the buf_id is updated.
* Arguments     : buffer = buffer to copy the image data
*                 size = size of buffer
* Return value  : none
******************************************/
void Image::camera_to_image(const uint8_t* buffer, int32_t size)
{
    /* Update buffer id */
    buf_id = ++buf_id % WL_BUF_NUM;
    memcpy(img_buffer[buf_id], buffer, sizeof(uint8_t)*size);
}


/*****************************************
* Function Name : at
* Description   : Get the value of img_buffer at index a.
*                 This function is NOT used currently.
* Arguments     : a = index of img_buffer
* Return Value  : value of img_buffer at index a
******************************************/
uint8_t Image::at(int32_t a)
{
    return img_buffer[buf_id][a];
}

/*****************************************
* Function Name : set
* Description   : Set the value of img_buffer at index a.
*                 This function is NOT used currently.
* Arguments     : a = index of img_buffer
*                 val = new value to be set
* Return Value  : -
******************************************/
void Image::set(int32_t a, uint8_t val)
{
    img_buffer[buf_id][a] = val;
    return;
}
/*****************************************
* Function Name : get_buf_id
* Description   : Get the value of the buf_id.
* Arguments     : -
* Return Value  : value of buf_id-
******************************************/
uint8_t Image::get_buf_id(void)
{
    return buf_id;
}
