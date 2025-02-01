#if GPU == GPU_OPENCL
#include "gpu_opencl.c"
#else
#error "invalid gpu selected"
#endif
