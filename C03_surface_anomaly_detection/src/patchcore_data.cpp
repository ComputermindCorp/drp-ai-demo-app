/***********************************************************************************************************************
* Copyright (C) 2023 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : patchcore_data.cpp
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "patchcore_data.h"

const int offset_threshould = 2;
const int offset_min_value = 6;
const int offset_max_value = 10;
const int offset_coreset_sampling_ratio = 14;
const int offset_num_neigbors = 18;
const int offset_memorybank_shape_dim1 = 22;
const int offset_memorybank_shape_dim2 = 26;
const int offset_memorybank = 30;

const int base_size =
    2 + // Header Symbol 'PC'
    4 + // thresould (float)
    4 + // min_value (float)
    4 + // max_value (float)
    4 + // coreset_sampling_ratio (float)
    4 + // num_neighbors (int)
    4 + // memory_bank_shape[0] (int)
    4   // memory+bank_shape]1] (int)
    ;

/*****************************************
* Function Name : load
* Description : Load patchcore parameter data.
* Arguments : path = path to parameter file.
* Return value : error code. 0:success -1:failed to load file.
* *****************************************/
int PatchCoreData::load(const char *path) {
    std::ifstream ifs(path, std::ios::binary);

    // Get file size.
    ifs.seekg(0, std::ios::end);
    long long int size = ifs.tellg();
    ifs.seekg(0);

    if (size == -1) {
        // Failed to load file.
        return -1;
    }

    // Check file size.
    if (size < base_size) {
        return -1;
    }

    // Load file data to buffer.
    char *buf = new char[size];
    ifs.read(buf, size);

    // Check header symbol.
    if (buf[0] != 'P' || buf[1] != 'C') {
        return -1;
    }

    // Set parameter from buffer.
    this->_thresould = *reinterpret_cast<float*>(&buf[offset_threshould]);
    this->_min_value = *reinterpret_cast<float*>(&buf[offset_min_value]);
    this->_max_value = *reinterpret_cast<float*>(&buf[offset_max_value]);
    this->_coreset_sampling_ratio = *reinterpret_cast<float*>(&buf[offset_coreset_sampling_ratio]);
    this->_num_neighbors = *reinterpret_cast<unsigned int*>(&buf[offset_num_neigbors]);
    this->_memorybank_shape[0] = *reinterpret_cast<unsigned int*>(&buf[offset_memorybank_shape_dim1]);
    this->_memorybank_shape[1] = *reinterpret_cast<unsigned int*>(&buf[offset_memorybank_shape_dim2]);

    // Check Memorybank size.
    int memory_buffer_size = this->_memorybank_shape[0] * this->_memorybank_shape[1] * 4;
    if (size != base_size + memory_buffer_size) {
        return -1;
    }

    // Set Memorybank from buffer.
    this->_memorybank = new float[memory_buffer_size];
    memcpy(this->_memorybank, &buf[offset_memorybank], memory_buffer_size);

    delete [] buf;

    return 0;
}


/*****************************************
* Function Name : dispose
* Description : Release all resource with the object.
* Arguments : 
* Return value : 
* *****************************************/
void PatchCoreData::dispose() {
    if (this->_memorybank != nullptr) {
        delete [] this->_memorybank;
    }
}

float PatchCoreData::thresould() const { return this->_thresould; }
float PatchCoreData::min_value() const { return this->_min_value; }
float PatchCoreData::max_value() const { return this->_max_value; }
float PatchCoreData::coreset_sampling_ratio() const { return this->_coreset_sampling_ratio; }
unsigned int PatchCoreData::num_neighbors() const { return this->_num_neighbors; }
int PatchCoreData::memorybank_shape(unsigned int index) const {
    if (index < 0 || index > 1) {
        return -1;
    }
    return this->_memorybank_shape[index];
}
float *PatchCoreData::memorybank() const { return this->_memorybank; }
