#if GPU == GPU_OPENCL
#include "opencl/gpu_opencl.c"
#elif GPU == GPU_VULKAN
#include "vulkan/gpu_vulkan.c"
#else
#error "invalid gpu selected"
#endif
