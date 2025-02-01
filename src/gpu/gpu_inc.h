/* date = January 23rd 2025 10:14 pm */

#ifndef GPU_INC_H
#define GPU_INC_H

#define GPU_NULL 0
#define GPU_OPENCL 1

#define GPU GPU_OPENCL

#include "gpu.h"

#if GPU == GPU_OPENCL
#include "gpu_opencl.h"
#else
#error "invalid gpu selected"
#endif

#endif //GPU_INC_H
