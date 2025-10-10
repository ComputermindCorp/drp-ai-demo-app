/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : patchcore_proc.h
* Version      : 1.00
* Description  : RZ/V2H and RZ/V2N DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera or Image
***********************************************************************************************************************/

#ifndef PATCHCORE_PROC_H
#define PATCHCORE_PROC_H

#include <algorithm>
#include <cmath>
#include <chrono>
#include <omp.h>
#include <arm_neon.h>
#include "define.h"
#include "patchcore_data.h"

class PatchCoreProc 
{
    public:
        PatchCoreProc();
        ~PatchCoreProc();
        float ng_threshold = 0.5;
        float anomaly_score = 0.5;
        float* anomaly_map;

        int8_t init(std::string path);
        int8_t execute(float* src1, float* src2);

    private:
        PatchCoreData data;
        int kernel_size = 3;
        int stride      = 1;
        int padding     = 1;

        float normalize_scholar(float x, float th, float min_value, float max_value);
        bool normalize(float* src,int n, float th, float min_value, float max_value);
        float* get_top(float* dist, int n_x1, int n_x2, int n_neighbors);
        float calc_score(float* dist, int n_x1, int n_x2, int n_neighbors, float* anomaly_map);
        float* cdist(float *a, float *b, int len_a, int len_b, int dim);
        float* chw2hwc(float* src, int c, int h, int w);
        float* concat(float* x1, float* x2, int nc1, int nc2, int nh, int nw);
        float* upsample(float* x, int nc, int nh, int nw, int scale);
        float* average_pooling(float* src, int nc, int nh, int nw, int kernel_size, int stride, int padding);

};

#endif
