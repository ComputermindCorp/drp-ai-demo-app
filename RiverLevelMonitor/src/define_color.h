/***********************************************************************************************************************
* Copyright (C) 2024 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : define_color.h
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for River Level Monitor with MIPI/USB Camera
***********************************************************************************************************************/
#ifndef DEFINE_COLOR_H
#define DEFINE_COLOR_H

/*****************************************
* color
******************************************/
/* Pascal VOC dataset label list */
const static std::vector<std::string> label_file_map = 
{ 
    "aeroplane",
    "bicycle",
    "bird",
    "boat",
    "bottle",
    "bus",
    "car",
    "cat",
    "chair",
    "cow",
    "diningtable",
    "dog",
    "horse",
    "motorbike",
    "person",
    "pottedplant", 
    "sheep",
    "sofa",
    "train",
    "tvmonitor" 
};

/* box color list */
const static unsigned int box_color[] =
{
    (0xFFFF00u),
    (0xFF0000u),
    (0xC0C0C0u),
    (0xFFA07Au),
    (0xFF1493u),
    (0x006400u),
    (0x00BFFFu),
    (0xDAA520u),
    (0xFF00FFu),
    (0xFFC0CBu),
    (0x008000u),
    (0x800080u),
    (0xFFA500u),
    (0x1E90FFu),
    (0x7CFC00u),
    (0xF000F0u),
    (0xF000FFu),
    (0xFF00FFu),
    (0xFF00FFu),
    (0xFF0FFFu)
};

#define NUM_CLASS_DEEPLABV3                (2)

/*class color (RGB)*/
#define COLOR_NORMAL_DATA           (0x00FF00u) /* in RGB */
#define COLOR_CAUTION_DATA          (0xFFFF00u) /* in RGB */
#define COLOR_WARNING_DATA          (0xFF0000u) /* in RGB */
#define COLOR_HAZARD_DATA           (0x880088u) /* in RGB */

#define COLOR_PERSON_ALERT_DATA     (0xFF0000u) /* in RGB */

/*class name*/
#define CLASS_NAME_RIVER            "river"

#endif


