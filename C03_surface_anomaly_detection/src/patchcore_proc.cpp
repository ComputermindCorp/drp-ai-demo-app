/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : patchcore_proc.cpp
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch ResNet with MIPI/USB Camera or Image
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "patchcore_proc.h"
#include <errno.h>

PatchCoreProc::PatchCoreProc()
{
}

PatchCoreProc::~PatchCoreProc()
{
}

static void preview_memorybank(float *data, int s1, int s2, int n_preview) {
    bool i_first = true;
    for (int i=0; i<s1; i++) {
        if (i >= n_preview && i < s1-n_preview) {
            if (i_first) {
                std::cout << "..." << std::endl;
                i_first = false;
            }
            continue;
        }

        bool j_first = true;
        for (int j=0; j<s2; j++) {
            if (j >= n_preview && j < s2-n_preview) {
                if (j_first) {
                    std::cout << "... ";
                    j_first = false;
                }
                continue;
            }
            std::cout << data[i*s2+j] << ", ";
        }
        std::cout << std::endl;
    }

}

/*****************************************
* Function Name : init
* Description   : Function to initialize patchcore proc
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t PatchCoreProc::init(std::string path)
{
    std::string file_path = path + "/" + mem_bank_name;
    int result = data.load(file_path.c_str());
	if (result != 0) {
        return result;
	}
    anomaly_map = new float[ANOMARY_MAP_SIZE * ANOMARY_MAP_SIZE];
    for(int i = 0; i < ANOMARY_MAP_SIZE; i++)
    {
        for (int j = 0; j < ANOMARY_MAP_SIZE; j++)
        {
            anomaly_map[i * ANOMARY_MAP_SIZE + j] = 0.0f;
        }
    }

    std::cout << "thresould: " << data.thresould() << std::endl;
    std::cout << "min value: " << data.min_value() << std::endl;
    std::cout << "max value: " << data.max_value() << std::endl;
    std::cout << "coreset sampling ratio: " << data.coreset_sampling_ratio() << std::endl;
    std::cout << "num neighbors: " << data.num_neighbors() << std::endl;
    std::cout << "memorybank shape: " << data.memorybank_shape(0) << ", " << data.memorybank_shape(1) << std::endl;

    preview_memorybank(data.memorybank(), data.memorybank_shape(0), data.memorybank_shape(1), 3);

    return 0;
}

/*****************************************
* Function Name : normalize_scholar
* Description   : normalize the values of the anomary score.
* Arguments     : src = input vectors, n = elements of vectors, th = threshold,  
*                 min_value = minimum value, max_value = maximum value.
* Return value  : x
* *****************************************/
float PatchCoreProc::normalize_scholar(float x, float th, float min_value, float max_value)
{
    float value_diff = max_value - min_value;
    if ( min_value >= max_value || std::isinf(value_diff)) {
        return false;
    }

    /*Normalization Process*/
    x = (x - th) / value_diff + 0.5;

    /*Clipping Process*/
    x = std::max(0.0f, std::min(1.0f, x));

    return x;
}

/*****************************************
* Function Name : normalize
* Description   : normalize the values of the input vectors.
* Arguments     : src = input vectors, n = elements of vectors, th = threshold,  
*                 min_value = minimum value, max_value = maximum value.
* Return value  : true if succeeded
* *****************************************/
bool PatchCoreProc::normalize(float* src, int n, float th, float min_value, float max_value)
{
    float value_diff = max_value - min_value;
    if ( n <= 0 || min_value >= max_value || std::isinf(value_diff)) {
        return false;
    }

    for (int i = 0; i < n; i++) {
        /*Normalization Process*/
        src[i] = (src[i] - th) / value_diff + 0.5;

        /*Clipping Process*/
        src[i] = std::max(0.0f, std::min(1.0f, src[i]));
    }
    return true;
}

/*****************************************
* Function Name : get_top
* Description   : Get the n_neighbors smallest values from a list of distances in ascending order.
* Arguments     : n_x1 = number of vectors in x1, n_x2 = number of vectors in x2, n_neighbors = extraction count
* Return value  : dist_topk
* *****************************************/
float* PatchCoreProc::get_top(float* dist, int n_x1, int n_x2, int n_neighbors)
{

    /* allocate output buffer */
    float* dist_topk;
    try {
        dist_topk = new float[n_x1 * n_neighbors];
    }
    catch (...) {
        return NULL;
    }

    /* extraction of nearby vectors */
    for (int i = 0; i < n_x1; i++) {
        float* row_start = dist + i * n_x2;
        std::sort(row_start, row_start + n_x2);
        for (int j = 0; j < n_neighbors; j++) {
            dist_topk[i * n_neighbors + j] = row_start[j];
        }
    }

    return dist_topk;
}

/*****************************************
* Function Name : calc_score
* Description   : Calculate anomaly scores and generate anomaly maps.
* Arguments     : n_x1 = number of vectors in x1, n_x2 = number of vectors in x2, n_neighbors = extraction count.
* Return value  : anomaly_score
* *****************************************/
float PatchCoreProc::calc_score(float* dist, int n_x1, int n_x2, int n_neighbors, float* anomaly_map)
{
    if (n_x1 < 1 || n_x2 < 1 || n_x2 < n_neighbors) {
        return -1.0f;
    }
    float* dist_topk = get_top(dist, n_x1, n_x2, n_neighbors);
    float anomaly_score = -1.0f;

    //Obtain the row index of the maximum value among the nearest neighbors
    int max_index = -1;
    float n_max_val = std::numeric_limits<float>::lowest();
    for (int i = 0; i < n_x1; i++) {
        if (dist_topk[i * n_neighbors] > n_max_val) {
            n_max_val = dist_topk[i * n_neighbors];
            max_index = i;
        }

        //generate anomaly maps
        anomaly_map[i] = dist_topk[i * n_neighbors];
    }

    //Obtain the maximum value within a row vector
    float row_max_val = std::numeric_limits<float>::lowest();
    for (int i = 0; i < n_neighbors; i++) {
        if (dist_topk[max_index * n_neighbors + i] > row_max_val) {
            row_max_val = dist_topk[max_index * n_neighbors + i];
        }
    }

    //Compute the sum of the elements in a row vector
    float sum_exp = 0.0f;
    for (int i = 0; i < n_neighbors; i++) {
        sum_exp += expf(dist_topk[max_index * n_neighbors + i] - row_max_val);
    }
    float softmax = 1.0f / sum_exp;
    float weight = 1.0f - softmax;
    anomaly_score = n_max_val * weight;

    delete [] dist_topk;
    return anomaly_score;
}

// float sub_pow_sum(float *a, float *b, int n) {
//     const int n_parallel = 16; 

//     const int unit = n / n_parallel;

//     float *p_a = a;
//     float *p_b = b;
//     float sum = 0.0;

//     for (int i=0; i<unit; i++) {
//         float32x4x4_t _a = vld4q_f32(p_a);
//         float32x4x4_t _b = vld4q_f32(p_b);
//         _a.val[0] = vsubq_f32(_a.val[0], _b.val[0]);
//         _a.val[0] = vmulq_f32(_a.val[0], _a.val[0]);
        
//         _a.val[1] = vsubq_f32(_a.val[1], _b.val[1]);
//         _a.val[1] = vmulq_f32(_a.val[1], _a.val[1]);
        
//         _a.val[2] = vsubq_f32(_a.val[2], _b.val[2]);
//         _a.val[2] = vmulq_f32(_a.val[2], _a.val[2]);

//         _a.val[3] = vsubq_f32(_a.val[3], _b.val[3]);
//         _a.val[3] = vmulq_f32(_a.val[3], _a.val[3]);

//         sum += vaddvq_f32(_a.val[0]);
//         sum += vaddvq_f32(_a.val[1]);
//         sum += vaddvq_f32(_a.val[2]);
//         sum += vaddvq_f32(_a.val[3]);

//         p_a += n_parallel;
//         p_b += n_parallel;
//     }

//     int rest = n % n_parallel;

//     for (int i=0; i<rest; i++) {
//         float diff = *p_a - *p_b;
//         sum += diff * diff;
//         p_a += 1;
//         p_b += 1;
//     }
//     return sum;
// }

/*****************************************
* Function Name : cdist
* Description   : -
* Arguments     : a,b = input vectors, len_a,len_b = number of input, dim = dimention
* Return value  : ret
* *****************************************/
float* PatchCoreProc::cdist(float *a, float *b, int len_a, int len_b, int dim) {

    if (len_a <= 0 || len_b <= 0 || dim <= 0) {
        return NULL;
    }
    int size = len_a * len_b;
    float *ret = new float[size]();

    #pragma omp parallel for
    for (int i = 0; i < len_a; i++) {
        float *base_a = &a[i * dim];
        
        for (int j = 0; j < len_b; j++) {
            float *base_b = &b[j * dim];
            float sum = 0.0f;

            for (int k = 0; k < dim; k++) {
                const float diff =  base_a[k] - base_b[k];
                sum += diff * diff;
            }
            ret[i * len_b + j] = std::sqrt(sum);
        }
    }

    return ret;
}
// float* PatchCoreProc::cdist(float *a, float *b, int len_a, int len_b, int dim) 
// {
//     int size = len_a * len_b;
//     float *ret = new float[size];
    
//     int i, j;
    
//     omp_set_num_threads(4);
//     #pragma omp parallel for private(j)
//     for(i=0; i<len_b; i++) {
//         //base_b = &b[i * dim];
        
//         for(j=0; j<len_a; j++) {
//             float sum = sub_pow_sum(&a[j*dim], &b[i*dim], dim);
//             ret[j * len_b + i] = std::sqrt(sum);
//         }
//     }

//     // int c = 0;
//     // for(int i=0; i<len_a; i++) {
//     //     for(int j=0; j<len_b; j++) {
//     //         ret[c] = 0.0;
//     //         for(int k=0; k<dim; k++) {
//     //             //std::cout << "A: " << a[i*dim + k] << " B: " << b[j*dim + k] << std::endl;
//     //             ret[c] += std::pow(a[i*dim + k] - b[j*dim + k], 2);
//     //         }
//     //         ret[c] = std::sqrt(ret[c]);
//     //         c++;
//     //     }
//     // }
//     return ret;
// }


/*****************************************
* Function Name : chw2hwc
* Description   : convert a chw-shaped tensor to hwc format.
* Arguments     : src = input vectors, c = number of channels, h = height, w = width
* Return value  : out_put
* *****************************************/
float* PatchCoreProc::chw2hwc(float* src, int c, int h, int w)
{
    if (c <= 0 || h <= 0 || w <= 0) {
        return NULL;
    }

    /* allocate memory for the hwc format */
    float* out_put;
    try {
        out_put = new float[h * w * c];
    }
    catch (...) {
        return NULL;
    }

    /* conversion from chw to hwc */
    for (int i = 0; i < c; i++) {  /* channel */
        for (int j = 0; j < h; j++) {  /* height */
            for (int k = 0; k < w; k++) {  /* width */
                out_put[(j * w + k) * c + i] = src[(i * h + j) * w + k];
            }
        }
    }

    return out_put;
}

/*****************************************
* Function Name : concat
* Description   : Concatenate two vectors of shape CHW along the channel dimension.
* Arguments     : x1 = vector1, x2 = vector2, nc1 = x1 channel size, nc2 = x2 channel size, 
*                 nh = vector height size, nw = vector width size
* Return value  : output
******************************************/
float* PatchCoreProc::concat(float* x1, float* x2, int nc1, int nc2, int nh, int nw)
{
    // check parameter.
    if (nc1 < 1 || nc2 < 1 || nh < 1 || nw < 1) {
        return NULL;
    }

    // allocate output buffer.
    float* output = NULL;
    try {
        output = new float[(nc1 + nc2) * nh * nw];
    }
    catch (...) {
        return NULL;
    }
    const int size_x1 = nc1 * nh * nw;
    const int size_x2 = nc2 * nh * nw;

    std::copy(x1, x1 + size_x1, output);
    std::copy(x2, x2 + size_x2, output + size_x1);
    return output;
}

/*****************************************
* Function Name : upsample
* Description   : Enlarge the size of the vector.
* Arguments     : x = data before upsample, 
*                 nc = channel of the array, nh = height of the array, 
*                 nw = width of the array, scale = scale factor.
* Return value  : output
******************************************/
float* PatchCoreProc::upsample(float* x, int nc, int nh, int nw, int scale)
{
    // check parameter.
    if (nc < 1 || nh < 1 || nw < 1 || scale < 1) {
        return NULL;
    }

    // allocate output buffer.
    float* output;
    try {
        output = new float[nc * nh * scale * nw * scale];
    }
    catch (...) {
        return NULL;
    }

    // upsampling
    #pragma omp parallel for
    for (int c = 0; c < nc; c++) {
        float* xc = &x[c * nh * nw];
        float* oc = &output[c * nh * scale * nw * scale];

        for (int h = 0; h < nh; h++) {
            for (int w = 0; w < nw; w++) {
                int base_x = w * scale;
                for (int offset_y = 0; offset_y < scale; offset_y++) {
                    int base_y = (h * scale + offset_y) * nw * scale;
                    for (int offset_x = 0; offset_x < scale; offset_x++) {
                        oc[base_y + base_x + offset_x] = xc[h * nw + w];
                    }
                }
            }
        }
    }

    return output;
}

/*****************************************
* Function Name : average_pooling
* Description   : Reduce the size of images or data using average pooling.
* Arguments     : src = input vector,
*                 nc = channel of the array, nh = height of the array, nw = width of the array
*                 kernel_size = Size of the kernel, stride = Stride of the window, padding = Size of zero padding added on both sides.
* Return value  : dst
* *****************************************/
float* PatchCoreProc::average_pooling(float* src, int nc, int nh, int nw, int kernel_size, int stride, int padding) 
{
    if (nc < 1 || nh < 1 || nw < 1 || kernel_size < 1 || stride < 1 || padding < 0 || nh + 2 * padding < kernel_size || nw + 2 * padding < kernel_size) {
        return NULL;
    }
    int dst_nh = (nh + 2 * padding - kernel_size) / stride + 1;
    int dst_nw = (nw + 2 * padding - kernel_size) / stride + 1;
    int kernel_num = kernel_size * kernel_size;

    /* allocate output buffer */
    float* dst;
    try {
        dst = new float[nc * dst_nh * dst_nw];
    }
    catch (...) {
        return NULL;
    }

    /* Perform average pooling */
    #pragma omp parallel for
    for (int c = 0; c < nc; c++) {
        for (int h = 0; h < dst_nh; h++) {
            for (int w = 0; w < dst_nw; w++) {

                /* Calculate base position of kernel */
                int src_base_h = h * stride - padding;
                int src_base_w = w * stride - padding;

                /* Accumulate sum for average pooling */
                float sum = 0.0f;
                for (int koffset_h = 0; koffset_h < kernel_size; koffset_h++) {
                    for (int koffset_w = 0; koffset_w < kernel_size; koffset_w++) {
                        int kh = src_base_h + koffset_h;
                        int kw = src_base_w + koffset_w;

                        /* Calculate the sum of values within the kernel, considering zero padding */
                        if (kh >= 0 && kh < nh && kw >= 0 && kw < nw) {
                            sum += src[c * nh * nw + kh * nw + kw];
                        }
                    }
                }
                /* Calculate average */
                float avg = sum / kernel_num;

                /* Store average in output matrix */
                dst[c * dst_nh * dst_nw + h * dst_nw + w] = avg;
            }
        }
    }
    return dst;
}

void save_txt(float *src, std::string txt, int n)
{
    std::ofstream outputfile(txt);
    for (int i=0; i<n; i++) {
        outputfile<<src[i]<<"\n";
    }
    outputfile.close();
}

float* read_txt(std::string txt, int n)
{
    float* ret = new float[n];
    std::ifstream file(txt);
    std::string line;
    std::vector<float> fvec;

    while (std::getline(file, line)) {
        float f = std::stof(line);
        fvec.push_back(f);
    }
    for(int i = 0; i < n; i++){
        if(i >= (int)(fvec.size())){
            break;
        }
        ret[i] = fvec[i];
    }
    return ret;
}

/*****************************************
* Function Name : execute
* Description   : 
* Arguments     : src1 = input vector 1,
*                 src2 = input vector 2,
* Return value  : dst
* *****************************************/
int8_t PatchCoreProc::execute(float* src1, float* src2)
{
    // src1 = read_txt("np_layer2.txt", OUTPUT_LAYER_1_C*OUTPUT_LAYER_1_H*OUTPUT_LAYER_1_W);
    // src2 = read_txt("np_layer3.txt", OUTPUT_LAYER_2_C*OUTPUT_LAYER_2_H*OUTPUT_LAYER_2_W);

    using namespace std;
    chrono::system_clock::time_point start, end;
    double elapsed;
    start = chrono::system_clock::now();
    float* avg1 = average_pooling(src1, OUTPUT_LAYER_1_C, OUTPUT_LAYER_1_H, OUTPUT_LAYER_1_W, kernel_size, stride, padding);
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("average_pooling Time 1 : %lf[ms]\n", elapsed);
    if(avg1 == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to average_pooling proc.\n");
        return -1;
    }
    
    start = chrono::system_clock::now();
    float* avg2 = average_pooling(src2, OUTPUT_LAYER_2_C, OUTPUT_LAYER_2_H, OUTPUT_LAYER_2_W, kernel_size, stride, padding);
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("average_pooling Time 2 : %lf[ms]\n", elapsed);
    if(avg2 == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to average_pooling proc.\n");
        return -1;
    }

    start = chrono::system_clock::now();
    float* upsamp2 = upsample(avg2, OUTPUT_LAYER_2_C, OUTPUT_LAYER_2_H, OUTPUT_LAYER_2_W, 2);
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("upsample Time          : %lf[ms]\n", elapsed);
    if(upsamp2 == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to upsample proc.\n");
        return -1;
    }

    delete [] avg2;
    avg2 = nullptr;

    start = chrono::system_clock::now();
    float* features = concat(avg1, upsamp2, OUTPUT_LAYER_1_C, OUTPUT_LAYER_2_C, OUTPUT_LAYER_1_H, OUTPUT_LAYER_1_W);
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("concat Time            : %lf[ms]\n", elapsed);
    if(features == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to concat proc.\n");
        return -1;
    }

    delete [] avg1;
    avg1 = nullptr;
    delete [] upsamp2;
    upsamp2 = nullptr;

    start = chrono::system_clock::now();
    float* hwc = chw2hwc(features, OUTPUT_LAYER_1_C + OUTPUT_LAYER_2_C, OUTPUT_LAYER_1_H, OUTPUT_LAYER_1_W);
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("chw2hwc Time           : %lf[ms]\n", elapsed);
    if(hwc == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to chw2hwc proc.\n");
        return -1;
    }

    delete [] features;
    features = nullptr;

    start = chrono::system_clock::now();
    float* dist = cdist(hwc, data.memorybank(), OUTPUT_LAYER_1_H * OUTPUT_LAYER_1_W, data.memorybank_shape(0), data.memorybank_shape(1));
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("cdist Time             : %lf[ms]\n", elapsed);
    if(dist == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to chw2hwc proc.\n");
        return -1;
    }

    delete [] hwc;
    hwc = nullptr;
    
    start = chrono::system_clock::now();
    anomaly_score = calc_score(dist, OUTPUT_LAYER_1_H * OUTPUT_LAYER_1_W, data.memorybank_shape(0), data.num_neighbors(), anomaly_map);
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("calc_score Time        : %lf[ms]\n", elapsed);
    if(anomaly_map == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to calc_score proc.\n");
        return -1;
    }

    delete [] dist;
    dist = nullptr;
    
    start = chrono::system_clock::now();
    normalize(anomaly_map, OUTPUT_LAYER_1_H * OUTPUT_LAYER_1_W, data.thresould(), data.min_value(), data.max_value());
    end = chrono::system_clock::now();
    elapsed = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(end-start).count());
    // printf("normalize Time         : %lf[ms]\n", elapsed);

    anomaly_score = normalize_scholar(anomaly_score, data.thresould(), data.min_value(), data.max_value());
    // printf("score --------------->>> %lf\n", anomaly_score);

    return 0;
}