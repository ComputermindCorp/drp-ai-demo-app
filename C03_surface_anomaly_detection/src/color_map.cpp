/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : color_map.cpp
* Version      : 1.00
* Description  : for RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera or Image
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "color_map.h"
#include <errno.h>

ColorMap::ColorMap()
{
    	// b = [colorsys.hsv_to_rgb(h, 1.0, 1.0) for h in np.linspace(0.0, 1/6, 103)]
	std::vector<float> b_hues = linspace(0.0f, 1.0f / 6, 103);
	std::vector<std::vector<float>> b;
	for (float h : b_hues) {
		b.push_back(hsv_to_rgb(h, 1.0f, 1.0f));
	}

	// g = [colorsys.hsv_to_rgb(h, 1.0, 1.0) for h in np.linspace(1/6, 1/3, 25)]
	std::vector<float> g_hues = linspace(1.0f / 6, 1.0f / 3, 25);
	std::vector<std::vector<float>> g;
	for (float h : g_hues) {
		g.push_back(hsv_to_rgb(h, 1.0f, 1.0f));
	}

	// r = [colorsys.hsv_to_rgb(h, 1.0, 1.0) for h in np.linspace(0.5, 2/3, 128)]
	std::vector<float> r_hues = linspace(0.5f, 2.0f / 3, 128);
	std::vector<std::vector<float>> r;
	for (float h : r_hues) {
		r.push_back(hsv_to_rgb(h, 1.0f, 1.0f));
	}

    std::vector<std::vector<float>> all_colors;
	all_colors.insert(all_colors.end(), b.begin(), b.end());
	all_colors.insert(all_colors.end(), g.begin(), g.end());
	all_colors.insert(all_colors.end(), r.begin(), r.end());

	rgb = scale_and_convert_to_uint8(all_colors);
}

ColorMap::~ColorMap()
{
}

/*****************************************
* Function Name : hsv_to_rgb
* Description   : Function to convert from HSV to RGB
* Arguments     : h = 
*               : s = 
*               : v = 
* Return value  : vsctor of { r, g, b }
******************************************/
std::vector<float> ColorMap::hsv_to_rgb(float h, float s, float v)
{
	float r, g, b = 0;
	int i = static_cast<int>(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	switch (i % 6) {
	case 0: r = v; g = t; b = p; break;
	case 1: r = q; g = v; b = p; break;
	case 2: r = p; g = v; b = t; break;
	case 3: r = p; g = q; b = v; break;
	case 4: r = t; g = p; b = v; break;
	case 5: r = v; g = p; b = q; break;
	default: r = 0; g = 0; b = 0; break; 
	}
	return { r, g, b };
}

/*****************************************
* Function Name : linspace
* Description   : Function to generate Linear space
* Arguments     : start = 
*               : end = 
*               : num = 
* Return value  : vsctor
******************************************/
std::vector<float> ColorMap::linspace(float start, float end, int num)
{
	std::vector<float> values(num);
	float step = (end - start) / (num - 1);
	for (int i = 0; i < num; ++i) {
		values[i] = start + step * i;
	}
	return values;
}

/*****************************************
* Function Name : custom_clamp
* Description   : Function to clamp
* Arguments     : value = 
*               : low = 
*               : high = 
* Return value  : T
******************************************/
template<typename T>
constexpr T ColorMap::custom_clamp(T value, T low, T high)
{
	return (value < low) ? low : (value > high) ? high : value;
}

/*****************************************
* Function Name : scale_and_convert_to_uint8
* Description   : Convert RGB values to the 0-255 range
* Arguments     : colors =  
* Return value  : vector rgb<uint8_t>
******************************************/
std::vector<std::vector<uint8_t>> ColorMap::scale_and_convert_to_uint8(const std::vector<std::vector<float>>& colors)
{
	std::vector<std::vector<uint8_t>> tmp;
	for (const auto& color : colors) {
        std::vector<uint8_t> rgb;
		for (float component : color) {
			uint8_t scaled = static_cast<uint8_t>(custom_clamp(component * 255.0f, 0.0f, 255.0f));
			rgb.push_back(scaled);
		}
        tmp.push_back(rgb);
	}
	return tmp;
}

/*****************************************
* Function Name : get_color_map
* Description   : Convert RGB values to the 0-255 range
* Arguments     : score =  
* Return value  : vector rgb<uint8_t>
******************************************/
std::vector<uint8_t> ColorMap::get_color_map(float score)
{
    uint8_t index = static_cast<uint8_t>(custom_clamp(score * 255.0f, 0.0f, 255.0f));
    if(index > rgb.size())
    {
        std::vector<uint8_t> ret{255, 255, 255};
        return ret;
    }
    return rgb[index];
}