/* date = January 23rd 2025 10:15 pm */

#ifndef GPU_OPENCL_H
#define GPU_OPENCL_H

#define CL_TARGET_OPENCL_VERSION 300
#include "third_party/CL/opencl.h"

StaticAssert(sizeof(U32) == sizeof(cl_uint), InvalidSize);
StaticAssert(sizeof(U64) == sizeof(cl_ulong), InvalidSize);
StaticAssert(sizeof(F32) == sizeof(cl_float), InvalidSize);
StaticAssert(sizeof(F64) == sizeof(cl_double), InvalidSize);

struct GPU_Buffer
{
  cl_mem buffer;
  U64 size;
};

struct GPU_Kernel
{
  String8 name;
  cl_kernel kernel;
  cl_program program;
};

struct GPU_State
{
  Arena* arena;
  
  cl_platform_id platform;
  cl_device_id device;
  cl_context context;
  cl_command_queue command_queue;
};

global GPU_State* g_opencl_state = 0;

internal cl_mem_flags gpu_flags_to_opencl_flags(GPU_BufferFlags flags);

internal cl_program gpu_opencl_build_program(String8 source);

#endif //GPU_OPENCL_H
