/***********************************************************************************************************************
* Copyright (C) 2023 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : color_map.h
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera
***********************************************************************************************************************/

#ifndef COLOR_MAP_H
#define COLOR_MAP_H

#include "define.h"
#include <iostream>
#include <vector>
#include <cmath>

class ColorMap
{
    public:
        ColorMap();
        ~ColorMap();

        std::vector<uint8_t> get_color_map(float score);

    private:
        std::vector<std::vector<uint8_t>> rgb;

        std::vector<float> hsv_to_rgb(float h, float s, float v);
        std::vector<float> linspace(float start, float end, int num);
        template<typename T>
        constexpr T custom_clamp(T value, T low, T high);
        std::vector<std::vector<uint8_t>> scale_and_convert_to_uint8(const std::vector<std::vector<float>>& colors);
        
};

#endif
