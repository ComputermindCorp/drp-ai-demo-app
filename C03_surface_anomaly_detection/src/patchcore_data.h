/***********************************************************************************************************************
* Copyright (C) 2023 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : patchcore_data.h
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera
***********************************************************************************************************************/

#include "define.h"
#include <iostream>
#include <fstream>
#include <cstring>

class PatchCoreData 
{
    float _thresould;
    float _min_value;
    float _max_value;
    float _coreset_sampling_ratio;
    float _num_neighbors;
    float *_memorybank;
    int _memorybank_shape[2];

public:
    float thresould() const;
    float min_value() const;
    float max_value() const; 
    float coreset_sampling_ratio() const; 
    unsigned int num_neighbors() const; 
    float *memorybank() const;
    int memorybank_shape(unsigned int index) const;

    PatchCoreData() {
        this->_memorybank = nullptr;
    }

    int load(const char *path);
    void dispose();
};
